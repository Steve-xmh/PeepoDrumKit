add_rules("mode.debug", "mode.release")
set_policy("package.requires_lock", true)

add_requires(
    "imgui 1.92.0-docking",
    {
        debug = is_mode("debug"),
        configs = {
            glfw_opengl3 = true,
        }
    })
add_requires(
    "stb 2025.03.14",
    "thorvg v1.0-pre10",
    "libsoundio",
    "glfw"
    )

target("PeepoDrumKit")
    set_kind("binary")
    set_languages("cxxlatest")
    add_files("src/core/*.cpp")
    add_files("src/peepodrumkit/*.cpp")
    add_files("src/audio/*.cpp")
    add_files("src/audio/*.c")
    add_files("src/imgui/*.cpp")
    add_files("src/imgui/ImGuiColorTextEdit/*.cpp")
    add_files("src/imgui/extension/*.cpp")
    
    add_defines("IMGUI_USER_CONFIG=\"imgui/peepodrumkit_imconfig.h\"")
    if is_mode("debug") then
        add_defines("PEEPO_DEBUG=(1)", "PEEPO_RELEASE=(0)")
    else
        add_defines("PEEPO_DEBUG=(0)", "PEEPO_RELEASE=(1)")
    end
    add_includedirs("src")
    add_includedirs("src/core")
    add_includedirs("src/peepodrumkit")
    add_includedirs("libs")
    add_packages("imgui", "dr_libs", "stb", "thorvg", "libsoundio", "glfw")
    if is_os("windows") then
        -- add_files("src/imgui/*.hlsl")
        add_syslinks("Shlwapi", "Shell32", "Ole32", "dxgi", "d3d11")
        -- add_packages("directxshadercompiler")
        -- add_rules("utils.hlsl2spv", {bin2c = true})
    end
    
    after_build(function () 
        os.cp("$(projectdir)/locales", "$(builddir)/$(plat)/$(arch)/$(mode)/")
        os.cp("$(projectdir)/assets", "$(builddir)/$(plat)/$(arch)/$(mode)/")
    end)