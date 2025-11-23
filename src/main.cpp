#include "peepodrumkit/chart_editor_main.h"
#include "core/core_io.h"

#if _WIN32

#include <Windows.h>
#include <fcntl.h>
#include <io.h>

static void Win32SetupConsoleMagic()
{
    ::SetConsoleOutputCP(CP_UTF8);
}

#endif // _WIN32

#ifdef PEEPO_DEBUG

int main(int argc, const char ** argv)
{
    CommandLine::SetCommandLineSTD(argc, argv);
#if _WIN32
    Win32SetupConsoleMagic();
#endif // _WIN32
    return PeepoDrumKit::EntryPoint();
}

#elif _WIN32 // PEEPO_DEBUG

#include <Windows.h>
static void Win32SetupConsoleMagic()
{
    ::SetConsoleOutputCP(CP_UTF8);
    ::_setmode(::_fileno(stdout), _O_BINARY);
    // TODO: Maybe overwrite the current locale too (?)
}

int WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    Win32SetupConsoleMagic();
    auto cmd = GetCommandLineW();
    int argc = 0;
    auto argv = CommandLineToArgvW(cmd, &argc);
    CommandLine::SetCommandLineSTD(argc, argv);
    return PeepoDrumKit::EntryPoint();
}

#else // _WIN32

int main(int argc, const char ** argv)
{
    CommandLine::SetCommandLineSTD(argc, argv);
#if WIN32
    Win32SetupConsoleMagic();
#endif // WIN32
    return PeepoDrumKit::EntryPoint();
}

#endif // _WIN32
