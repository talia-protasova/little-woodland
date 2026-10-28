# Local Linux release

The archive contains the game binary, runtime artwork/fonts/prepared PNG animations,
bundled GLFW, notices and launch instructions. It contains no C++ source, GIF preparation
tools or standalone sound library. Run on Linux x86_64 (the build architecture).

After extracting, launch from any directory:

```sh
/path/to/little-woodland-*/bin/little_woodland
```

Install the target system's OpenGL drivers and system dependencies. On Ubuntu 22.04:

```sh
sudo apt install libgl1 libx11-6 libxrandr2 libxi6 libxcursor1 libxinerama1 libxxf86vm1 libstdc++6 libasound2 libpulse0
```

GLFW is bundled; glibc, libstdc++, the X11 stack, OpenGL and graphics drivers are system
components, not bundled. Build on the oldest supported Linux distribution (currently
Ubuntu 22.04) for a compatible glibc baseline. This archive is not a universal AppImage.
If an audio device/backend is unavailable, gameplay continues with “No audio”.

Click adjacent ordinary tiles to match them. Deliver the letter within 20 moves.
R restarts, M toggles sound, Escape closes. Sound state persists across restarts.
Mute is also available in the upper-right corner of gameplay and both result pages.

A sound-free archive is intentional: optional licensed sounds are not included unless
packaging explicitly enables audio with `--with-audio` and validates all five WAVs and
recorded integrity hashes. The author’s licensing confirmation is in SOURCES.md; original
asset titles and individual URLs are not required release metadata.
An audio-enabled archive incorporates sounds only in the finished binary game. Do not
redistribute its sounds separately, with source files, or as stock resources. See the
included SOURCES.md and third-party notices; sound effects are not original game artwork.
