add_rules("mode.debug", "mode.release")
set_policy("package.requires_lock", true)

add_requires(
    "imgui 1.92.0-docking",
    {
        debug = is_mode("debug"),
        configs = {
            sdl3 = true,
            sdl3_gpu = true,
        }
    })

libsoundio_config = {}

if is_os("windows") then
    libsoundio_config.wasapi = true
elseif is_os("macosx") then
    libsoundio_config.coreaudio = true
elseif is_os("linux") then
    libsoundio_config.alsa = true
    libsoundio_config.jack = true
    libsoundio_config.pulseaudio = true
end
    
add_requires(
    "libsoundio",
    {
        debug = is_mode("debug"),
        configs = libsoundio_config
    })
add_requires(
    "stb 2025.03.14",
    "thorvg v1.0-pre10",
    "libsoundio",
    "libsdl3",
    "icu4c"
    )

if is_os("windows") then -- Process Resource File (Icon)
    rule("resource")
        set_extensions(".rc", ".res")
        on_build_file(function (target, sourcefile, opt)
            import("core.project.depend")
            
            local targetfile = target:objectfile(sourcefile)
            depend.on_changed(function ()
                os.vrunv("windres", {sourcefile, "-o", targetfile})
            end, {files = sourcefile})
        end)
end
    
target("PeepoDrumKit")
    set_kind("binary")
    set_languages("cxxlatest")
    add_files("src/main.cpp")
    add_files("src/core/*.cpp")
    add_files("src/peepodrumkit/*.cpp")
    add_files("src/audio/*.cpp")
    add_files("src/audio/*.c")
    add_files("src/imgui/*.cpp")
    add_files("src/imgui/ImGuiColorTextEdit/*.cpp")
    add_files("src/imgui/extension/*.cpp")
    
    add_defines("IMGUI_USER_CONFIG=\"imgui/peepodrumkit_imconfig.h\"")

    if not is_mode("debug") then
        set_optimize("fastest")
        set_policy("build.optimization.lto", true)
    end

    if is_mode("debug") then
        add_defines("PEEPO_DEBUG=(1)", "PEEPO_RELEASE=(0)")
    else
        add_defines("PEEPO_DEBUG=(0)", "PEEPO_RELEASE=(1)")
    end
    add_includedirs("src")
    add_includedirs("src/core")
    add_includedirs("src/peepodrumkit")
    add_includedirs("libs")
    add_packages("imgui", "dr_libs", "stb", "thorvg", "libsoundio", "libsdl3", "icu4c")
    if is_os("windows") then
        -- add_files("src/imgui/*.hlsl")
        add_files("src_res/Resource.rc")
        add_syslinks("Shlwapi", "Shell32", "Ole32", "dxgi", "d3d11", "ntdll")
        -- add_packages("directxshadercompiler")
        -- add_rules("utils.hlsl2spv", {bin2c = true})
    end
    
    after_build(function (target)
        os.cp("$(projectdir)/locales", target:targetdir() .. "/")
        os.cp("$(projectdir)/assets", target:targetdir() .. "/")
    end)