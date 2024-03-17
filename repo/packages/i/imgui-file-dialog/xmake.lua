package("imgui-file-dialog")
    set_homepage("https://github.com/aiekick/ImGuiFileDialog")
    set_description("File Dialog for Dear ImGui")
    set_license("MIT")

    add_urls("https://github.com/aiekick/ImGuiFileDialog/archive/refs/tags/$(version).tar.gz",
             "https://github.com/aiekick/ImGuiFileDialog.git")

    add_versions("v0.6.7", "f73e29fca08163fdcbd1e58fb7b67c7e56f5fa2e")
    if is_plat("mingw") then
        add_configs("shared", {description = "Build shared binaries.", default = false, type = "boolean", readonly = true})
    end

    add_deps("imgui")
    if is_plat("windows") then
        add_deps("dirent")
    end

    on_install("windows", "linux", "macosx", "mingw", "android", function (package)
        local configs = {}
        io.writefile("xmake.lua", [[
            add_requires("imgui")
            if is_plat("windows") then
                add_requires("dirent")
                add_packages("dirent")
            end
            add_rules("mode.debug", "mode.release")
            target("imgui-file-dialog")
                set_kind("$(kind)")
                set_languages("c++11")
                add_files("ImGuiFileDialog.cpp")
                add_headerfiles("ImGuiFileDialog.h", "ImGuiFileDialogConfig.h")
                add_packages("imgui")
                if is_plat("windows") and is_kind("shared") then
                    add_rules("utils.symbols.export_all", {export_classes = true})
                end
        ]])
        import("package.tools.xmake").install(package, configs)
    end)