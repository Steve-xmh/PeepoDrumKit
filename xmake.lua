add_rules("mode.debug", "mode.release")
-- set_policy("package.requires_lock", true)

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
    "plusaes",
    {
        debug = is_mode("debug"),
    })
if not is_os("linux") then
    add_requires(
        "libsoundio",
        {
            debug = is_mode("debug"),
            configs = libsoundio_config
        })
end
add_requires(
    "thorvg v1.0-pre10",
    {
        debug = is_mode("debug"),
        configs = {
            loaders = {"svg"},
            -- log = is_mode("debug"),
        }
    })
add_requires(
    "stb 2025.03.14",
    "gzip-hpp",
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
    set_symbols("debug") -- Keep debug symbols in all build modes
    set_languages("cxxlatest")

    -- project sources
    add_files("src/main.cpp")
    add_files("src/core/*.cpp")
    add_files("src/peepodrumkit/*.cpp")
    add_files("src/audio/*.c", "src/audio/*.cpp")
    add_files("src/imgui/*.cpp")
    add_files("src/imgui/ImGuiColorTextEdit/*.cpp")
    add_files("src/imgui/extension/*.cpp")

    -- project headers
    add_headerfiles("src/core/*.h")
    add_headerfiles("src/peepodrumkit/*.h")
    add_headerfiles("src/audio/*.h")
    add_headerfiles("src/imgui/*.h")
    add_headerfiles("src/imgui/ImGuiColorTextEdit/*.h")
    add_headerfiles("src/imgui/extension/*.h")

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
    
    if is_os("linux") then
        -- Manually specify libsoundio paths for Linux distros without pkg-config
        add_includedirs("/usr/include", "/usr/local/include")
        add_linkdirs("/usr/lib", "/usr/local/lib", "/usr/lib/x86_64-linux-gnu")
        add_links("soundio")
        add_packages("imgui", "dr_libs", "stb", "thorvg", "libsdl3", "icu4c", "plusaes", "gzip-hpp")
        -- Suppress specific warnings on Linux
        add_cxxflags("-fpermissive", "-Wno-changes-meaning")
    else
        add_packages("imgui", "dr_libs", "stb", "thorvg", "libsoundio", "libsdl3", "icu4c", "plusaes", "gzip-hpp")
    end
    
    if is_os("windows") then
        -- add_files("src/imgui/*.hlsl")
        add_files("src_res/Resource.rc")
        add_syslinks("Shlwapi", "Shell32", "Ole32", "dxgi", "d3d11", "ntdll")
        -- add_packages("directxshadercompiler")
        -- add_rules("utils.hlsl2spv", {bin2c = true})
        add_cxxflags("/utf-8") -- force UTF-8 string without u8"" (not default in MSVC++)
    elseif is_os("macosx") then
        add_rules("xcode.application")
        add_files("src/Info.plist")
    end

    if is_os("macosx") then
        destDir = path.join("Contents", "Resources")
        add_installfiles("src_res/PeepoDrumKit.icns", {prefixdir = destDir})
    else
        destDir = "bin"
    end
    assetDir = path.join(destDir, "assets")

    add_installfiles("(locales/*.*)", {prefixdir = destDir})
    add_installfiles("assets*/(*.*)", {prefixdir = assetDir})
    add_installfiles("assets*/(audio/*.*)", {prefixdir = assetDir})
    add_installfiles("assets*/(graphics/*.*)", {prefixdir = assetDir})

target("PeepoDrumKit_test_fumen")
    set_kind("binary")
    set_symbols("debug")
    set_languages("cxxlatest")
    set_default(false)
    add_files("test/fumen_test.cpp")
    add_files("src/core/core_types.cpp")
    add_files("src/core/file_format_fumen.cpp")
    add_includedirs("src")
    add_includedirs("src/core")
    add_packages("stb", "libsdl3", "icu4c")
    if is_mode("debug") then
        add_defines("PEEPO_DEBUG=(1)", "PEEPO_RELEASE=(0)")
    else
        add_defines("PEEPO_DEBUG=(0)", "PEEPO_RELEASE=(1)")
    end