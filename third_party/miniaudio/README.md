# miniaudio

Upstream: https://github.com/mackron/miniaudio
Version: 0.11.23
Revision: f40cf03f80cdb7e741d43e53b7e706e8c1394bcf
License: MIT or public domain (choose MIT; see LICENSE).

Vendored unchanged, compiled once in src/audio/Miniaudio.cpp. Only WAV decoding and
Linux PulseAudio/ALSA playback are used. FLAC, MP3, encoding, generation, the resource
manager, node graph and engine are disabled. Backends load their system libraries
dynamically, so ALSA/PulseAudio development headers are not required.
