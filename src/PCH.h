#pragma once

#pragma warning(push)
#include "F4SE/F4SE.h"
#include "RE/Fallout.h"
#pragma warning(pop)

#include <spdlog/spdlog.h>

#define DLLEXPORT __declspec(dllexport)

// F4SE::Init sets up the spdlog default logger; REX::INFO/WARN/ERROR route through it.
// logger:: aliases spdlog directly so existing logger::info/warn/error calls work unchanged.
namespace logger = spdlog;

using namespace std::literals;

#include "Version.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

#include <ixwebsocket/IXWebSocket.h>

#include <atomic>
#include <string>
#include <vector>
#include <mutex>
#include <memory>
