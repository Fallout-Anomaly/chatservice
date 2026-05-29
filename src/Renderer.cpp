#include "PCH.h"
#include "Renderer.h"
#include "ChatClient.h"
#include <shellapi.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <set>
#include <cmath>
#include "RE/Bethesda/MenuCursor.h"
#include "icons/IconsFontAwesome6.h"
#include "icons/IconsFontAwesome6Brands.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern bool g_privacyAccepted;
extern void SavePrivacyPolicy();
extern void SaveUsername(const std::string& newName);
extern uint64_t g_steamID;
extern uint64_t FetchSteamID();

namespace FalloutChat
{
	namespace Renderer
	{
		typedef HRESULT(APIENTRY* Present_t)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
		Present_t oPresent = nullptr;

		typedef HRESULT(APIENTRY* ResizeBuffers_t)(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
		ResizeBuffers_t oResizeBuffers = nullptr;

		WNDPROC oWndProc = nullptr;

		bool g_initialized = false;
		bool g_chatOpen = false;
		HWND g_hwnd = nullptr;
		ID3D11RenderTargetView* g_pRenderTargetView = nullptr;

		std::vector<ChatMessage> g_chatHistory;
		char g_inputBuffer[256] = "";

		static std::set<std::string> s_mutedPlayers;
		static int s_unreadCount = 0;

		static const ImVec4 SENDER_PALETTE[] = {
			ImVec4(0.2f, 0.8f, 1.0f, 1.0f),
			ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
			ImVec4(0.9f, 0.4f, 0.9f, 1.0f),
			ImVec4(0.4f, 0.95f, 0.4f, 1.0f),
			ImVec4(1.0f, 0.5f, 0.5f, 1.0f),
			ImVec4(0.5f, 0.85f, 1.0f, 1.0f),
			ImVec4(1.0f, 1.0f, 0.4f, 1.0f),
			ImVec4(0.85f, 0.6f, 1.0f, 1.0f),
		};

		static ImVec4 SenderColor(const std::string& name)
		{
			uint32_t hash = 5381;
			for (char c : name) hash = ((hash << 5) + hash) + static_cast<uint8_t>(c);
			constexpr size_t N = sizeof(SENDER_PALETTE) / sizeof(SENDER_PALETTE[0]);
			return SENDER_PALETTE[hash % N];
		}

		static float s_passiveAlpha = 0.0f;
		static std::chrono::steady_clock::time_point s_lastMessageArrival{};
		static bool s_passiveEverTriggered = false;

		static constexpr float PASSIVE_FADE_IN  = 0.25f;
		static constexpr float PASSIVE_FADE_OUT = 1.5f;
		static float s_passiveHoldTime = 5.0f;
		static float s_passiveMaxAlpha = 0.85f;

		static std::string CurrentTimestamp()
		{
			auto now = std::chrono::system_clock::now();
			auto t = std::chrono::system_clock::to_time_t(now);
			std::tm tm_now;
			localtime_s(&tm_now, &t);
			std::ostringstream oss;
			oss << std::put_time(&tm_now, "%H:%M:%S");
			return oss.str();
		}

		static std::string GetPlayerLocation()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) return "";

			if (auto* loc = player->currentLocation) {
				const char* name = loc->GetFullName();
				if (name && name[0] != '\0') return name;
			}

			if (auto* cell = player->GetParentCell()) {
				const char* name = cell->GetFullName();
				if (name && name[0] != '\0') return name;
			}

			return "";
		}

		void HandleSendInput()
		{
			std::string text(g_inputBuffer);
			memset(g_inputBuffer, 0, sizeof(g_inputBuffer));
			if (text.empty()) return;

			if (g_steamID == 0) {
				g_steamID = FetchSteamID();
				if (g_steamID != 0)
					ChatClient::GetSingleton().SetSteamID(g_steamID);
			}

			auto sysMsg = [&](const std::string& txt) {
				ChatMessage m;
				m.sender    = "System";
				m.text      = txt;
				m.timestamp = CurrentTimestamp();
				g_chatHistory.push_back(m);
			};

			if (g_steamID == 0) {
				sysMsg("Unable to verify Steam status. Try again in a moment.");
				return;
			}

			if (text.size() > 6 && text.substr(0, 6) == "/name ") {
				std::string newName = text.substr(6);
				while (!newName.empty() && newName.front() == ' ') newName.erase(newName.begin());
				while (!newName.empty() && newName.back() == ' ') newName.pop_back();
				if (!newName.empty()) {
					ChatClient::GetSingleton().SetUsername(newName);
					::SaveUsername(newName);
					sysMsg("Username changed to: " + newName);
				}
				return;
			}

			if (text.size() > 6 && text.substr(0, 6) == "/mute ") {
				std::string target = text.substr(6);
				while (!target.empty() && target.back() == ' ') target.pop_back();
				if (!target.empty()) {
					s_mutedPlayers.insert(target);
					sysMsg("Muted: " + target);
				}
				return;
			}

			if (text.size() > 8 && text.substr(0, 8) == "/unmute ") {
				std::string target = text.substr(8);
				while (!target.empty() && target.back() == ' ') target.pop_back();
				if (!target.empty()) {
					s_mutedPlayers.erase(target);
					sysMsg("Unmuted: " + target);
				}
				return;
			}

			ChatClient::GetSingleton().Send(text, GetPlayerLocation());
		}

		void ToggleChat(bool forceState, bool state)
		{
			g_chatOpen = forceState ? state : !g_chatOpen;

			if (g_chatOpen)
				s_unreadCount = 0;

			ImGuiIO& io = ImGui::GetIO();
			if (g_chatOpen) {
				io.MouseDrawCursor = true;
				if (auto menuCursor = RE::MenuCursor::GetSingleton()) {
					menuCursor->RegisterCursor();
					menuCursor->ClearConstraints();
				}
				::ClipCursor(nullptr);
			} else {
				io.MouseDrawCursor = false;
				if (auto menuCursor = RE::MenuCursor::GetSingleton()) {
					menuCursor->UnregisterCursor();
				}
				memset(g_inputBuffer, 0, sizeof(g_inputBuffer));
			}

			if (auto controlMap = RE::ControlMap::GetSingleton()) {
				controlMap->SetTextEntryMode(g_chatOpen);
				controlMap->ignoreKeyboardMouse = g_chatOpen;
			}
		}

		bool IsChatOpen()
		{
			return g_chatOpen;
		}

		LRESULT CALLBACK WndProc_Hook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			if (uMsg == WM_KEYDOWN && wParam == VK_F11) {
				ToggleChat(true, !g_chatOpen);
				return 1;
			}

			if (g_chatOpen) {
				if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) {
					return 1;
				}

				switch (uMsg) {
				case WM_KEYDOWN:
				case WM_KEYUP:
				case WM_CHAR:
				case WM_SYSKEYDOWN:
				case WM_SYSKEYUP:
				case WM_LBUTTONDOWN:
				case WM_LBUTTONUP:
				case WM_RBUTTONDOWN:
				case WM_RBUTTONUP:
				case WM_MBUTTONDOWN:
				case WM_MBUTTONUP:
				case WM_MOUSEMOVE:
				case WM_MOUSEWHEEL:
				case WM_XBUTTONDOWN:
				case WM_XBUTTONUP:
					return 1;
				}
			}

			return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
		}

		void ApplyFalloutTheme()
		{
			ImGuiStyle& style = ImGui::GetStyle();
			style.WindowRounding = 8.0f;
			style.FrameRounding = 4.0f;
			style.PopupRounding = 4.0f;
			style.ScrollbarRounding = 12.0f;
			style.GrabRounding = 4.0f;

			ImVec4* colors = style.Colors;

			ImVec4 mainColor = ImVec4(0.0f, 1.0f, 0.3f, 1.0f);
			ImVec4 bgDark = ImVec4(0.02f, 0.08f, 0.04f, 0.75f);
			ImVec4 textCol = ImVec4(0.4f, 1.0f, 0.6f, 1.0f);

			colors[ImGuiCol_Text] = textCol;
			colors[ImGuiCol_TextDisabled] = ImVec4(0.2f, 0.6f, 0.3f, 1.0f);
			colors[ImGuiCol_WindowBg] = bgDark;
			colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
			colors[ImGuiCol_PopupBg] = ImVec4(0.01f, 0.05f, 0.02f, 0.95f);
			colors[ImGuiCol_Border] = mainColor;
			colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

			colors[ImGuiCol_FrameBg] = ImVec4(0.02f, 0.12f, 0.05f, 0.6f);
			colors[ImGuiCol_FrameBgHovered] = ImVec4(0.05f, 0.2f, 0.08f, 0.8f);
			colors[ImGuiCol_FrameBgActive] = ImVec4(0.1f, 0.3f, 0.12f, 1.0f);

			colors[ImGuiCol_TitleBg] = bgDark;
			colors[ImGuiCol_TitleBgActive] = ImVec4(0.03f, 0.15f, 0.06f, 0.9f);
			colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0f, 0.0f, 0.0f, 0.4f);

			colors[ImGuiCol_MenuBarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
			colors[ImGuiCol_ScrollbarBg] = ImVec4(0.01f, 0.05f, 0.02f, 0.5f);
			colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.0f, 0.8f, 0.2f, 0.6f);
			colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.0f, 1.0f, 0.3f, 0.8f);
			colors[ImGuiCol_ScrollbarGrabActive] = mainColor;

			colors[ImGuiCol_CheckMark] = mainColor;
			colors[ImGuiCol_SliderGrab] = ImVec4(0.0f, 0.8f, 0.2f, 0.8f);
			colors[ImGuiCol_SliderGrabActive] = mainColor;

			colors[ImGuiCol_Button] = ImVec4(0.02f, 0.12f, 0.05f, 0.6f);
			colors[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.8f, 0.2f, 0.8f);
			colors[ImGuiCol_ButtonActive] = mainColor;

			colors[ImGuiCol_Header] = ImVec4(0.02f, 0.15f, 0.05f, 0.6f);
			colors[ImGuiCol_HeaderHovered] = ImVec4(0.0f, 0.8f, 0.2f, 0.8f);
			colors[ImGuiCol_HeaderActive] = mainColor;

			colors[ImGuiCol_ResizeGrip] = ImVec4(0.0f, 0.8f, 0.2f, 0.4f);
			colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.0f, 0.8f, 0.2f, 0.8f);
			colors[ImGuiCol_ResizeGripActive] = mainColor;

			colors[ImGuiCol_Tab] = ImVec4(0.02f, 0.12f, 0.05f, 0.6f);
			colors[ImGuiCol_TabHovered] = ImVec4(0.0f, 0.8f, 0.2f, 0.8f);
			colors[ImGuiCol_TabActive] = mainColor;
		}

		HRESULT APIENTRY Hook_Present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
		{
			if (!g_initialized) {
				auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
				if (rendererData && rendererData->device && rendererData->context) {
					g_hwnd = (HWND)rendererData->renderWindow[0].hwnd;

					IMGUI_CHECKVERSION();
					ImGui::CreateContext();

					{
						ImGuiIO& io = ImGui::GetIO();
						io.Fonts->AddFontDefault();

						static const ImWchar brands_ranges[] = { ICON_MIN_FAB, ICON_MAX_FAB, 0 };
						ImFontConfig brands_cfg;
						brands_cfg.MergeMode = true;
						brands_cfg.GlyphMinAdvanceX = 13.0f;
						io.Fonts->AddFontFromFileTTF("Data\\F4SE\\Plugins\\fa-brands-400.ttf", 13.0f, &brands_cfg, brands_ranges);

						static const ImWchar solid_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
						ImFontConfig solid_cfg;
						solid_cfg.MergeMode = true;
						solid_cfg.GlyphMinAdvanceX = 13.0f;
						io.Fonts->AddFontFromFileTTF("Data\\F4SE\\Plugins\\fa-solid-900.ttf", 13.0f, &solid_cfg, solid_ranges);
					}

					ApplyFalloutTheme();

					ImGui_ImplWin32_Init(g_hwnd);
					ImGui_ImplDX11_Init(rendererData->device, rendererData->context);

					oWndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc_Hook);

					g_initialized = true;
				}
			}

			if (g_initialized && !g_pRenderTargetView) {
				auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
				ID3D11Texture2D* pBackBuffer = nullptr;
				if (SUCCEEDED(pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)))) {
					rendererData->device->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
					pBackBuffer->Release();
				}
			}

			if (g_initialized && g_pRenderTargetView) {
				auto newMsgs = ChatClient::GetSingleton().GetNewMessages();
				if (!newMsgs.empty()) {
					g_chatHistory.insert(g_chatHistory.end(), newMsgs.begin(), newMsgs.end());
					if (g_chatHistory.size() > 100) {
						g_chatHistory.erase(g_chatHistory.begin(), g_chatHistory.begin() + (g_chatHistory.size() - 100));
					}
					s_lastMessageArrival = std::chrono::steady_clock::now();
					s_passiveEverTriggered = true;
					if (!g_chatOpen)
						s_unreadCount += static_cast<int>(newMsgs.size());
				}

				ImGui_ImplDX11_NewFrame();
				ImGui_ImplWin32_NewFrame();
				ImGui::NewFrame();

				if (g_chatOpen) {
					if (auto menuCursor = RE::MenuCursor::GetSingleton()) {
						menuCursor->ClearConstraints();
					}
					::ClipCursor(nullptr);
				}

				if (g_chatOpen && !::g_privacyAccepted) {
					ImGui::SetNextWindowSize(ImVec2(500, 250), ImGuiCond_FirstUseEver);
					ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
					ImGui::Begin("Data Privacy Policy", &g_chatOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
					ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Action Required:");
					ImGui::Separator();
					ImGui::TextWrapped(
						"Before you join the Fallen World global chat, please review our Data Privacy Policy.\n\n"
						"By using this chat system, you agree that your messages, username, and "
						"game reports may be stored and reviewed by server administrators to enforce "
						"our language and moderation policies.\n\n"
						"Severe violations will result in automated blocking and potential bans. "
						"We do not sell or share your data with any third parties."
					);
					ImGui::Spacing(); ImGui::Spacing();
					if (ImGui::Button("I Agree & Continue", ImVec2(200, 30))) {
						::SavePrivacyPolicy();
					}
					ImGui::SameLine();
					if (ImGui::Button("Decline & Close", ImVec2(200, 30))) {
						ToggleChat(true, false);
					}
					ImGui::End();
				}

				if (s_passiveEverTriggered && !g_chatOpen) {
					float elapsed = std::chrono::duration<float>(
						std::chrono::steady_clock::now() - s_lastMessageArrival).count();
					if (elapsed < PASSIVE_FADE_IN)
						s_passiveAlpha = elapsed / PASSIVE_FADE_IN;
					else if (elapsed < PASSIVE_FADE_IN + s_passiveHoldTime)
						s_passiveAlpha = 1.0f;
					else if (elapsed < PASSIVE_FADE_IN + s_passiveHoldTime + PASSIVE_FADE_OUT)
						s_passiveAlpha = 1.0f - (elapsed - PASSIVE_FADE_IN - s_passiveHoldTime) / PASSIVE_FADE_OUT;
					else
						s_passiveAlpha = 0.0f;
				}

				bool showChat = (g_chatOpen && ::g_privacyAccepted) ||
				                (s_passiveEverTriggered && s_passiveAlpha > 0.01f && !g_chatOpen);

				if (showChat) {
					float alpha = g_chatOpen ? 1.0f : (s_passiveAlpha * s_passiveMaxAlpha);

					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
					ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
					ImGui::SetNextWindowPos(ImVec2(12.0f, ImGui::GetIO().DisplaySize.y - 312.0f), ImGuiCond_FirstUseEver);
					ImGui::SetNextWindowBgAlpha(0.75f);

					ImGuiWindowFlags wflags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
					if (!g_chatOpen) {
						wflags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
						          ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing;
					}

					bool closeRequested = false;
					ImGui::Begin("Fallout Multi-Chat", g_chatOpen ? &closeRequested : nullptr, wflags);

					if (g_chatOpen) {
						float winW = ImGui::GetWindowWidth();
						bool wideEnough = winW >= 320.0f;

						if (wideEnough) {
							ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "Made with love by the Fallen World team");
							ImGui::SameLine(winW - 220.0f);

							ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.345f, 0.396f, 0.949f, 0.85f));
							ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.345f, 0.396f, 0.949f, 1.0f));
							ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.271f, 0.322f, 0.878f, 1.0f));
							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
							if (ImGui::Button(ICON_FA_DISCORD " Discord", ImVec2(100, 0)))
								ShellExecuteA(NULL, "open", "https://discord.com/invite/TAueAV8Utk", NULL, NULL, SW_SHOWNORMAL);
							ImGui::PopStyleColor(4);

							ImGui::SameLine();
							if (ImGui::Button(ICON_FA_GLOBE " Website", ImVec2(76, 0)))
								ShellExecuteA(NULL, "open", "https://fallenworld.nexus/", NULL, NULL, SW_SHOWNORMAL);

							ImGui::SameLine();
						} else {
							ImGui::SameLine(winW - 34.0f);
						}

						if (ImGui::Button(ICON_FA_GEAR, ImVec2(26, 0)))
							ImGui::OpenPopup("##ChatSettings");

						if (ImGui::BeginPopup("##ChatSettings")) {
							ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "Chat Settings");
							ImGui::Separator();
							ImGui::SetNextItemWidth(180.0f);
							ImGui::SliderFloat("Passive opacity", &s_passiveMaxAlpha, 0.1f, 1.0f, "%.2f");
							ImGui::SetNextItemWidth(180.0f);
							ImGui::SliderFloat("Hold time (sec)", &s_passiveHoldTime, 1.0f, 15.0f, "%.0f");
							if (!s_mutedPlayers.empty()) {
								ImGui::Separator();
								ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Muted players:");
								for (const auto& name : s_mutedPlayers)
									ImGui::Text("  %s", name.c_str());
							}
							ImGui::Separator();
							ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "Commands");
							ImGui::TextDisabled("/me <action>       - Emote (e.g. /me waves)");
							ImGui::TextDisabled("/name <username>   - Change your display name");
							ImGui::TextDisabled("/mute <player>     - Mute a player locally");
							ImGui::TextDisabled("/unmute <player>   - Unmute a player");
							ImGui::TextDisabled("/report <player> <reason>  - Report a player");
							ImGui::TextDisabled("F11  - Open / close chat");
							ImGui::Separator();
							ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.345f, 0.396f, 0.949f, 0.85f));
							ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.345f, 0.396f, 0.949f, 1.0f));
							ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.271f, 0.322f, 0.878f, 1.0f));
							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
							if (ImGui::Button(ICON_FA_DISCORD " Discord", ImVec2(180, 0)))
								ShellExecuteA(NULL, "open", "https://discord.com/invite/TAueAV8Utk", NULL, NULL, SW_SHOWNORMAL);
							ImGui::PopStyleColor(4);
							if (ImGui::Button(ICON_FA_GLOBE " Website", ImVec2(180, 0)))
								ShellExecuteA(NULL, "open", "https://fallenworld.nexus/", NULL, NULL, SW_SHOWNORMAL);
							ImGui::EndPopup();
						}

						ImGui::Separator();

						bool isConnected = ChatClient::GetSingleton().IsConnected();
						int  onlineCount = ChatClient::GetSingleton().GetOnlineCount();
						if (isConnected) {
							ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected");
							if (onlineCount > 0) {
								ImGui::SameLine();
								ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), ICON_FA_USER " %d online", onlineCount);
							}
						} else {
							ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Disconnected - Reconnecting...");
						}
						ImGui::Separator();
					}

					float footerHeight = g_chatOpen
						? (ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing())
						: 0.0f;
					ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footerHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

					for (const auto& msg : g_chatHistory) {
						if (!msg.sender.empty() && s_mutedPlayers.count(msg.sender)) continue;

						ImGui::TextColored(ImVec4(0.3f, 0.5f, 0.3f, 1.0f), "[%s]", msg.timestamp.c_str());
						ImGui::SameLine();

						if (msg.isEmote) {
							std::string emoteText = msg.text;
							if (!emoteText.empty() && emoteText[0] == ' ') emoteText = emoteText.substr(1);
							ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "* %s %s", msg.sender.c_str(), emoteText.c_str());
						} else {
							ImGui::TextColored(SenderColor(msg.sender), "%s:", msg.sender.c_str());
							ImGui::SameLine();
							ImGui::TextWrapped("%s", msg.text.c_str());
						}
					}

					if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
						ImGui::SetScrollHereY(1.0f);

					ImGui::EndChild();

					if (g_chatOpen) {
						ImGui::Separator();
						ImGui::PushItemWidth(-60.0f);
						bool reclaimFocus = false;
						if (ImGui::InputText("##Input", g_inputBuffer, IM_ARRAYSIZE(g_inputBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
							HandleSendInput();
							reclaimFocus = true;
						}
						ImGui::PopItemWidth();
						ImGui::SetItemDefaultFocus();
						if (reclaimFocus || ImGui::IsWindowAppearing())
							ImGui::SetKeyboardFocusHere(-1);
						ImGui::SameLine();
						if (ImGui::Button("Send", ImVec2(50, 0)))
							HandleSendInput();
					}

					ImGui::End();
					ImGui::PopStyleVar();

					if (closeRequested)
						ToggleChat(true, false);
				}

				// ── Unread badge ─────────────────────────────────────────────────────
				if (!g_chatOpen && s_unreadCount > 0) {
					ImGuiIO& badgeIO = ImGui::GetIO();
					float pulse = 0.5f + 0.5f * sinf(static_cast<float>(ImGui::GetTime()) * 3.0f);
					ImU32 glowColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.8f + 0.2f * pulse, 0.3f, 0.9f));
					ImU32 bgColor   = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.12f, 0.04f, 0.88f));

					char badgeText[32];
					snprintf(badgeText, sizeof(badgeText), "%d new", s_unreadCount);

					ImVec2 textSize = ImGui::CalcTextSize(badgeText);
					constexpr float padX = 7.0f, padY = 4.0f;
					ImVec2 badgePos(18.0f, badgeIO.DisplaySize.y - 24.0f);
					ImVec2 rectMin(badgePos.x - padX, badgePos.y - padY);
					ImVec2 rectMax(badgePos.x + textSize.x + padX, badgePos.y + textSize.y + padY);

					auto* dl = ImGui::GetForegroundDrawList();
					dl->AddRectFilled(rectMin, rectMax, bgColor, 6.0f);
					dl->AddRect(rectMin, rectMax, glowColor, 6.0f, 0, 1.5f);
					dl->AddText(badgePos, glowColor, badgeText);
				}

				ImGui::EndFrame();
				ImGui::Render();

				auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
				rendererData->context->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
				ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
			}

			return oPresent(pSwapChain, SyncInterval, Flags);
		}

		HRESULT APIENTRY Hook_ResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
		{
			if (g_pRenderTargetView) {
				g_pRenderTargetView->Release();
				g_pRenderTargetView = nullptr;
			}

			return oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
		}

		void InstallHooks()
		{

			auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
			if (!rendererData || !rendererData->renderWindow[0].swapChain) {
				logger::critical("Failed to retrieve DXGI SwapChain from BSGraphics!");
				return;
			}

			IDXGISwapChain* swapChain = rendererData->renderWindow[0].swapChain;
			void** vtable = *reinterpret_cast<void***>(swapChain);

			if (MH_Initialize() != MH_OK) {
				logger::critical("Failed to initialize MinHook!");
				return;
			}

			if (MH_CreateHook(vtable[8], &Hook_Present, reinterpret_cast<LPVOID*>(&oPresent)) != MH_OK) {
				logger::critical("Failed to hook IDXGISwapChain::Present");
				return;
			}

			if (MH_CreateHook(vtable[13], &Hook_ResizeBuffers, reinterpret_cast<LPVOID*>(&oResizeBuffers)) != MH_OK) {
				logger::critical("Failed to hook IDXGISwapChain::ResizeBuffers");
				return;
			}

			if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
				logger::critical("Failed to enable hooks");
				return;
			}
		}

		void UninstallHooks()
		{
			MH_DisableHook(MH_ALL_HOOKS);
			MH_Uninitialize();

			if (g_hwnd && oWndProc) {
				SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)oWndProc);
			}

			if (g_pRenderTargetView) {
				g_pRenderTargetView->Release();
				g_pRenderTargetView = nullptr;
			}

			ImGui_ImplDX11_Shutdown();
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
		}
	}
}
