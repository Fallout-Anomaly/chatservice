set_xmakever("3.0.0")
set_project("FalloutChat")
set_version("1.0.0")
set_languages("c++23")
set_warnings("allextra")
set_encodings("utf-8")

add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- Pre-built library roots — no re-compile on configure.
-- NewCommonLib must be built first: cd ../NewCommonLib && xmake
local newcl       = "$(scriptdir)/../NewCommonLib"
local newcl_build = newcl .. "/build/windows/x64/release"
-- spdlog built by NewCommonLib's first xmake run (SPDLOG_USE_STD_FORMAT build)
local spdlog_root = "C:/Users/Game/AppData/Local/.xmake/packages/s/spdlog/v1.16.0/1053dccb2da94316b3bef3cb1db15374"
-- ixwebsocket + mbedTLS from existing vcpkg cmake build (x64-windows-static-md)
local vcpkg_root  = "$(scriptdir)/build/vcpkg_installed/x64-windows-static-md"

target("FalloutChat")
    set_kind("shared")
    set_arch("x64")

    -- NewCommonLib: OG (1.10.x) + NG (1.10.980/984) + AE (1.11.x) support
    add_includedirs(newcl .. "/include")
    add_includedirs(newcl .. "/lib/commonlib-shared/include")
    add_linkdirs(newcl_build)
    add_links("commonlibf4", "commonlib-shared")

    -- spdlog — must match the build used by NewCommonLib (SPDLOG_USE_STD_FORMAT)
    add_includedirs(spdlog_root .. "/include")
    add_linkdirs(spdlog_root .. "/lib")
    add_links("spdlog")
    add_defines("SPDLOG_COMPILED_LIB", "SPDLOG_USE_STD_FORMAT", "SPDLOG_WCHAR_TO_UTF8_SUPPORT")

    -- ixwebsocket + mbedTLS (TLS required for wss:// server URL)
    add_includedirs(vcpkg_root .. "/include")
    add_linkdirs(vcpkg_root .. "/lib")
    add_links("ixwebsocket", "mbedtls", "mbedcrypto", "mbedx509", "p256m", "everest", "zs")
    add_defines("IXWEBSOCKET_USE_TLS")

    add_defines("COMMONLIB_RUNTIMECOUNT=3")
    add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX", "_UNICODE")

    add_files("src/**.cpp")
    add_headerfiles("src/**.h", "include/**.h")
    add_includedirs("src", "include")

    set_pcxxheader("src/PCH.h")

    if is_plat("windows") then
        add_cxflags("/permissive-", "/wd4200", "/wd4201", "/wd4324")
        add_syslinks("advapi32", "bcrypt", "crypt32", "d3d11", "d3dcompiler",
                     "dbghelp", "dxgi", "ole32", "oleaut32",
                     "shell32", "user32", "version", "ws2_32")
    end

    after_build(function(target)
        local deploys = {
            "E:\\Modlists\\Fallen World Alpha 2\\mods\\FalloutChat",
            "D:\\Games\\ModlistDownloads\\mods\\FalloutChat",
        }
        for _, base in ipairs(deploys) do
            if os.isdir(base) then
                local plugins = path.join(base, "F4SE/Plugins")
                local views   = path.join(base, "PrismaUI_F4/views")
                os.mkdir(plugins)
                os.mkdir(views)
                os.cp(target:targetfile(), plugins)
                local pdb = path.join(target:targetdir(), target:name() .. ".pdb")
                if os.isfile(pdb) then os.cp(pdb, plugins) end
                os.cp(path.join(os.scriptdir(), "assets/views/chat.html"), views)
                local ini = path.join(os.scriptdir(), "FalloutChat.ini")
                if not os.isfile(path.join(plugins, "FalloutChat.ini")) then
                    os.cp(ini, plugins)
                end
                cprint("${bright green}deploy:${clear} %s", base)
            end
        end
    end)
