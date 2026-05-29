#include "PCH.h"
#include "ChatUI.h"
#include "ChatClient.h"
#include "KeyHandler.h"
#include <shellapi.h>

extern bool g_privacyAccepted;
extern void SavePrivacyPolicy();
extern void SaveIntroDismissed();
extern void SaveUsername(const std::string& newName);
extern uint64_t g_steamID;
extern uint64_t FetchSteamID();

namespace FalloutChat
{
	namespace ChatUI
	{
		static PRISMA_UI_API::IVPrismaUI2* g_api = nullptr;
		static PrismaView g_view = 0;
		static bool g_chatOpen = false;

		// Escape backslashes and double-quotes for embedding in a JS double-quoted string literal.
		static std::string EscapeJS(std::string s)
		{
			size_t pos = 0;
			while ((pos = s.find('\\', pos)) != std::string::npos) { s.replace(pos, 1, "\\\\"); pos += 2; }
			pos = 0;
			while ((pos = s.find('"', pos))  != std::string::npos) { s.replace(pos, 1, "\\\""); pos += 2; }
			return s;
		}

		// Strip outer double-quotes and unescape \" and \\ from a JSON string argument.
		static std::string UnquoteJS(const char* jsonArg)
		{
			std::string s = jsonArg ? jsonArg : "";
			if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
				s = s.substr(1, s.size() - 2);
				std::string out;
				out.reserve(s.size());
				for (size_t i = 0; i < s.size(); ++i) {
					if (s[i] == '\\' && i + 1 < s.size()) {
						if      (s[i+1] == '"')  { out += '"';  ++i; }
						else if (s[i+1] == '\\') { out += '\\'; ++i; }
						else                     { out += s[i]; }
					} else {
						out += s[i];
					}
				}
				return out;
			}
			return s;
		}

		static void OnDomReady(PrismaView view)
		{
			logger::info("ChatUI: DOM ready");
			
			// Register JS -> C++ callbacks
			g_api->RegisterJSListener(view, "SendMessage", [](const char* jsonArgs) {
				std::string text = UnquoteJS(jsonArgs);
				if (text.empty()) return;

				if (g_steamID == 0) {
					g_steamID = FetchSteamID();
					if (g_steamID != 0)
						ChatClient::GetSingleton().SetSteamID(g_steamID);
				}

				if (g_steamID == 0) {
					g_api->Invoke(g_view, "addSystemMessage('Unable to verify Steam status. Try again in a moment.')");
					return;
				}

				if (text.size() > 6 && text.substr(0, 6) == "/name ") {
					std::string newName = text.substr(6);
					while (!newName.empty() && newName.front() == ' ') newName.erase(newName.begin());
					while (!newName.empty() && newName.back() == ' ') newName.pop_back();
					if (!newName.empty()) {
						ChatClient::GetSingleton().SetUsername(newName);
						ChatClient::GetSingleton().SendRename(newName);
						::SaveUsername(newName);
						g_api->Invoke(g_view, ("addSystemMessage(\"Username changed to: " + EscapeJS(newName) + "\")").c_str());
					}
					return;
				}

				auto* player = RE::PlayerCharacter::GetSingleton();
				std::string locName = "";
				if (player) {
					if (auto* loc = player->currentLocation) {
						if (loc->GetFullName()) locName = loc->GetFullName();
					} else if (auto* cell = player->GetParentCell()) {
						if (cell->GetFullName()) locName = cell->GetFullName();
					}
				}

				ChatClient::GetSingleton().Send(text, locName);
			});

			g_api->RegisterJSListener(view, "CloseChat", [](const char*) {
				ToggleChat();
			});

			g_api->RegisterJSListener(view, "OpenURL", [](const char* urlArgs) {
				std::string url = UnquoteJS(urlArgs);
				ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
			});
			
			g_api->RegisterJSListener(view, "SetPrivacyAccepted", [](const char*) {
				::SavePrivacyPolicy();
				::SaveIntroDismissed();
			});
		}

		void Initialize()
		{
			g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>();
			if (!g_api) {
				logger::error("PrismaUI API not available. Chat UI will not work.");
				return;
			}
			KeyHandler::RegisterSink();
			(void)KeyHandler::GetSingleton()->Register(0x7A, KeyEventType::KEY_DOWN, []() { // F11
				ToggleChat();
			});
			(void)KeyHandler::GetSingleton()->Register(0x1B, KeyEventType::KEY_DOWN, []() { // Escape
				if (g_chatOpen) ToggleChat();
			});
		}

		void CreateView()
		{
			if (!g_api || g_view != 0) return;

			g_view = g_api->CreateView("chat.html", OnDomReady);
			g_api->RegisterConsoleCallback(g_view, [](PrismaView, PRISMA_UI_API::ConsoleMessageLevel, const char* msg) {
				logger::info("[JS] {}", msg);
			});
			
			g_api->Hide(g_view);
		}

		void ToggleChat()
		{
			if (!g_api || !g_api->IsValid(g_view)) return;
			
			g_chatOpen = !g_chatOpen;
			
			if (g_chatOpen) {
				g_api->Show(g_view);
				g_api->Focus(g_view, false, false);
				g_api->Invoke(g_view, "onChatOpened()");
			} else {
				g_api->Unfocus(g_view);
				g_api->Hide(g_view);
				g_api->Invoke(g_view, "onChatClosed()");
			}
		}

		bool IsChatOpen()
		{
			return g_chatOpen;
		}

		void OnMessagesReceived()
		{
			if (!g_api || !g_api->IsValid(g_view)) return;

			auto msgs = ChatClient::GetSingleton().GetNewMessages();
			for (const auto& m : msgs) {
				std::string js = "receiveMessage(\""
					+ EscapeJS(m.sender) + "\", \""
					+ EscapeJS(m.text)   + "\", \""
					+ EscapeJS(m.timestamp) + "\");";
				g_api->Invoke(g_view, js.c_str());
			}
		}
	}
}
