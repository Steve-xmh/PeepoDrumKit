# Peepo Drum Kit

Fork of PeepoDrumKit with large refractor for Official Taiko Charting & Modding.

<img width="1392" height="944" alt="A screenshot of the application" src="https://github.com/user-attachments/assets/f695d018-1839-4ff9-adbf-ef7b4dc4c16b" />

## Features

- Support direct reading and writing of fumen (FumenV2) chart files used in Official Taiko Game.
- Cross-platform via using SDL3 with native rendering backend (Tested on Windows and macOS, maybe works on Linux?)
- Low latency WASAPI share mode output when the mix format is in 16bit 44100Hz 2ch.
- ...Maybe more for official charting i guess xd

## Build

```bash
xmake
xmake build
```

## Credits

- [samyuu/PeepoDrumKit](https://github.com/samyuu/PeepoDrumKit)
- [0auBSQ/PeepoDrumKit](https://github.com/0auBSQ/PeepoDrumKit)
- [vivaria/tja2fumen](https://github.com/vivaria/tja2fumen)
