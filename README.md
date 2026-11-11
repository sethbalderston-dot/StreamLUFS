# Stream LUFS (VST3)

Master-bus loudness meter for streaming. Measures per ITU-R BS.1770-4 / EBU R128:
Integrated, Short-term (3 s), Momentary (400 ms), Loudness Range, True Peak (4x oversampled), Peak/Loudness ratio.
Compares against Spotify, Apple Music, YouTube, Amazon, Tidal, Deezer, SoundCloud and EBU R128, and tells you
what to adjust. Audio passes through untouched. Stereo only.

## Build (needs CMake 3.22+, a C++17 compiler, internet for the first configure)
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
The .vst3 lands in `build/StreamLUFS_artefacts/Release/VST3/` and is auto-copied to your system VST3 folder.
- Windows: Visual Studio 2022 (Desktop C++). macOS: Xcode command line tools. Linux: JUCE deps (see .github/workflows/build.yml).
- No toolchain? Push this folder to a GitHub repo; the Actions workflow builds Windows/macOS/Linux .vst3 files.
- macOS Apple Silicon/Intel universal: add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`.

## Use
Insert LAST on the master bus (after the limiter). Hit Reset, play the whole track, read Integrated.
Click a platform row to see its guidance. Edit targets in `Source/Platforms.h`.

## Verify the maths (no JUCE needed)
    g++ -std=c++17 -ISource test/test_meter.cpp -o t && ./t
