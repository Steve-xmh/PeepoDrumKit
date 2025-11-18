#include "peepodrumkit/chart_editor_main.h"

#if __OS_WINDOWS

#include <Windows.h>
#include <fcntl.h>
#include <io.h>

static void Win32SetupConsoleMagic()
{
    ::SetConsoleOutputCP(CP_UTF8);
}

#endif // __OS_WINDOWS

#ifdef PEEPO_DEBUG

int main(int, const char **)
{
#if __OS_WINDOWS
    Win32SetupConsoleMagic();
#endif // __OS_WINDOWS
    return PeepoDrumKit::EntryPoint();
}

#elif __OS_WINDOWS // PEEPO_DEBUG

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
    return PeepoDrumKit::EntryPoint();
}

#else // __OS_WINDOWS

int main(int, const char **)
{
#if WIN32
    Win32SetupConsoleMagic();
#endif // WIN32
    return PeepoDrumKit::EntryPoint();
}

#endif // __OS_WINDOWS
