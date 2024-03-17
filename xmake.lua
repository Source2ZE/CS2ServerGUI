add_rules("mode.debug", "mode.release")
add_repositories("custom-repo repo")
add_requires("imgui", {debug = is_mode("debug"), configs = {dx9 = true, win32 = true}})
add_requires("nlohmann_json")
add_requires("imgui-file-dialog", {debug = is_mode("debug")})


includes("@builtin/xpack")

--local SDK_PATH = os.getenv("HL2SDKCS2")
local SDK_PATH = "G:/hl2sdk-experimental/hl2sdk"
local MM_PATH = os.getenv("MMSOURCE112")

target("CS2ServerGUI")
    set_kind("shared")
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_packages("imgui", "nlohmann_json", "imgui-file-dialog")
    --set_symbols("hidden")

    add_files({
        SDK_PATH.."/tier1/convar.cpp",
        SDK_PATH.."/public/tier0/memoverride.cpp",
        SDK_PATH.."/tier1/generichash.cpp",
        SDK_PATH.."/entity2/entitysystem.cpp",
        SDK_PATH.."/entity2/entityidentity.cpp",
        SDK_PATH.."/entity2/entitykeyvalues.cpp",
        SDK_PATH.."/tier1/keyvalues3.cpp",
    })

    add_links({
        SDK_PATH.."/lib/public/win64/tier0.lib",
        SDK_PATH.."/lib/public/win64/tier1.lib",
        SDK_PATH.."/lib/public/win64/interfaces.lib",
        SDK_PATH.."/lib/public/win64/mathlib.lib",
    })

    add_linkdirs({
        "vendor/funchook/lib/Release",
    })

    add_links({
        "funchook",
        "distorm",
        "d3d9"
    })

    add_links("psapi");
    add_files("src/utils/plat_win.cpp");

    add_includedirs({
        "src",
        "vendor/funchook/include",
        "vendor",
        -- sdk
        SDK_PATH,
        SDK_PATH.."/thirdparty/protobuf-3.21.8/src",
        SDK_PATH.."/common",
        SDK_PATH.."/game/shared",
        SDK_PATH.."/game/server",
        SDK_PATH.."/public",
        SDK_PATH.."/public/engine",
        SDK_PATH.."/public/mathlib",
        SDK_PATH.."/public/tier0",
        SDK_PATH.."/public/tier1",
        SDK_PATH.."/public/entity2",
        SDK_PATH.."/public/game/server",
        -- metamod
        MM_PATH.."/core",
        MM_PATH.."/core/sourcehook",
    })

    add_defines({
        "COMPILER_MSVC",
        "COMPILER_MSVC64",
        "WIN32",
        "WINDOWS",
        "CRT_SECURE_NO_WARNINGS",
        "CRT_SECURE_NO_DEPRECATE",
        "CRT_NONSTDC_NO_DEPRECATE",
        "_MBCS",
        "META_IS_SOURCE2"
    })

    set_languages("cxx20")