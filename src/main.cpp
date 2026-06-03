#include "PCH.h"
#include "ChatUI.h"
#include "ChatClient.h"
#include <SimpleIni.h>

CSimpleIniA ini(true, false, false);
static std::mutex g_iniMutex;
std::string serverUrl = "wss://chat.fallenworld.nexus/ws";
std::string username = "Player";
bool g_privacyAccepted = false;
std::atomic<uint64_t> g_steamID = 0;
bool g_chatEnabled = true;
bool g_tutorialSeen = false;
bool g_introDismissed = false;
int g_fontSize = 14;
int g_bgOpacity = 60;

uint64_t FetchSteamID()
{
	logger::info("FetchSteamID: attempting");
	HMODULE hSteam = GetModuleHandleA("steam_api64.dll");
	if (!hSteam) {
		logger::warn("FetchSteamID: steam_api64.dll not loaded");
		return 0;
	}

	auto pfnSteamUser = reinterpret_cast<void*(*)()>(GetProcAddress(hSteam, "SteamUser"));
	if (!pfnSteamUser) {
		logger::warn("FetchSteamID: SteamUser export not found");
		return 0;
	}

	void* user = pfnSteamUser();
	if (!user) {
		logger::warn("FetchSteamID: SteamUser() returned null");
		return 0;
	}

	// CSteamID uses hidden-pointer return (MSVC x64): call vtable slot directly
	// so RDX points to our storage, not a garbage code-section address.
	void** vtable = *reinterpret_cast<void***>(user);
	using GetSteamID_fn = void*(__fastcall*)(void* self, uint64_t* result);
	auto pfnGetSteamID = reinterpret_cast<GetSteamID_fn>(vtable[2]);

	uint64_t id = 0;
	pfnGetSteamID(user, &id);
	if (id != 0)
		logger::info("FetchSteamID: got {}", id);
	else
		logger::warn("FetchSteamID: vtable call returned 0");
	return id;
}

void SaveUsername(const std::string& newName)
{
	username = newName;
	std::lock_guard<std::mutex> lock(g_iniMutex);
	ini.LoadFile("Data\\F4SE\\Plugins\\FalloutChat.ini");
	ini.SetValue("General", "username", newName.c_str());
	ini.SaveFile("Data\\F4SE\\Plugins\\FalloutChat.ini");
	ini.Reset();
}

void SavePrivacyPolicy()
{
	std::lock_guard<std::mutex> lock(g_iniMutex);
	ini.LoadFile("Data\\F4SE\\Plugins\\FalloutChat.ini");
	ini.SetBoolValue("General", "privacy_accepted", true);
	ini.SaveFile("Data\\F4SE\\Plugins\\FalloutChat.ini");
	g_privacyAccepted = true;
	ini.Reset();
}

void SaveChatEnabled(bool enabled)
{
	g_chatEnabled = enabled;
	std::lock_guard<std::mutex> lock(g_iniMutex);
	ini.LoadFile("Data\\F4SE\\Plugins\\FalloutChat.ini");
	ini.SetBoolValue("General", "chat_enabled", enabled);
	ini.SaveFile("Data\\F4SE\\Plugins\\FalloutChat.ini");
	ini.Reset();
}

void SaveTutorialSeen()
{
	std::lock_guard<std::mutex> lock(g_iniMutex);
	ini.LoadFile("Data\\F4SE\\Plugins\\FalloutChat.ini");
	ini.SetBoolValue("General", "tutorial_seen", true);
	ini.SaveFile("Data\\F4SE\\Plugins\\FalloutChat.ini");
	g_tutorialSeen = true;
	ini.Reset();
}

void SaveIntroDismissed()
{
	std::lock_guard<std::mutex> lock(g_iniMutex);
	ini.LoadFile("Data\\F4SE\\Plugins\\FalloutChat.ini");
	ini.SetBoolValue("General", "intro_dismissed", true);
	ini.SaveFile("Data\\F4SE\\Plugins\\FalloutChat.ini");
	g_introDismissed = true;
	ini.Reset();
}

void SaveFontSize(int size)
{
	g_fontSize = size;
	std::lock_guard<std::mutex> lock(g_iniMutex);
	ini.LoadFile("Data\\F4SE\\Plugins\\FalloutChat.ini");
	ini.SetLongValue("General", "font_size", size);
	ini.SaveFile("Data\\F4SE\\Plugins\\FalloutChat.ini");
	ini.Reset();
}

void SaveOpacity(int opacity)
{
	g_bgOpacity = opacity;
	std::lock_guard<std::mutex> lock(g_iniMutex);
	ini.LoadFile("Data\\F4SE\\Plugins\\FalloutChat.ini");
	ini.SetLongValue("General", "bg_opacity", opacity);
	ini.SaveFile("Data\\F4SE\\Plugins\\FalloutChat.ini");
	ini.Reset();
}

void LoadConfigs()
{
	logger::info("LoadConfigs: reading FalloutChat.ini");
	std::lock_guard<std::mutex> lock(g_iniMutex);
	ini.LoadFile("Data\\F4SE\\Plugins\\FalloutChat.ini");

	// Server URL is hardcoded — not user-configurable to prevent redirection attacks
	serverUrl = "wss://chat.fallenworld.nexus/ws";
	username = ini.GetValue("General", "username", "");

	if (username.empty())
		username = "Player";

	bool privacyAlreadyAccepted = ini.GetBoolValue("General", "privacy_accepted", false);
	g_privacyAccepted  = privacyAlreadyAccepted;
	g_chatEnabled      = ini.GetBoolValue("General", "chat_enabled", true);
	g_tutorialSeen     = ini.GetBoolValue("General", "tutorial_seen", false);
	g_introDismissed   = ini.GetBoolValue("General", "intro_dismissed", privacyAlreadyAccepted);
	g_fontSize         = (int)ini.GetLongValue("General", "font_size",  14);
	g_bgOpacity        = (int)ini.GetLongValue("General", "bg_opacity", 60);
	ini.Reset();

	logger::info("LoadConfigs: url='{}' username='{}' privacy={} enabled={} tutorialSeen={} introDismissed={} fontSize={} opacity={}",
		serverUrl, username, g_privacyAccepted, g_chatEnabled, g_tutorialSeen, g_introDismissed, g_fontSize, g_bgOpacity);
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
	// OG F4SE calls this before Load; NG F4SE uses F4SEPlugin_Version instead.
	// Keep minimal — no logging here (logger not yet initialized for OG path).
	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name        = "FalloutChat";
	a_info->version     = Version::MAJOR;

	if (a_f4se->IsEditor())
		return false;

	if (a_f4se->RuntimeVersion() < F4SE::RUNTIME_1_10_162)
		return false;

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se, { .logName = "FalloutChat" });
	logger::info("{} v{}", Version::PROJECT, Version::NAME);
	LoadConfigs();

	const F4SE::MessagingInterface* messageInterface = F4SE::GetMessagingInterface();
	messageInterface->RegisterListener([](F4SE::MessagingInterface::Message* msg) -> void {
		if (msg->type == F4SE::MessagingInterface::kGameDataReady) {
			logger::info("F4SE: kGameDataReady — initializing chat");
			FalloutChat::ChatUI::Initialize();
			if (username.empty())
				username = "Player";
			g_steamID = FetchSteamID();
			FalloutChat::ChatClient::GetSingleton().Initialize(serverUrl, username, g_steamID);
		} else if (msg->type == F4SE::MessagingInterface::kPostLoadGame) {
			logger::info("F4SE: kPostLoadGame — creating view");
			FalloutChat::ChatUI::CreateView();
		} else if (msg->type == F4SE::MessagingInterface::kNewGame) {
			logger::info("F4SE: kNewGame — creating view");
			FalloutChat::ChatUI::CreateView();
		}
	});

	return true;
}
