# NES Revolution

**Open-source NES emulator for Android — project foundation**

NES Revolution aims to combine accurate NES hardware emulation with a modern Android presentation layer. The project prioritizes deterministic emulation, low input latency, stable frame pacing, and optional visual/audio enhancements.

> Current status: **foundation only**. This repository does not yet contain a playable emulator. CPU, PPU, APU, Android UI, rendering, audio output, and complete mapper support are planned, not implemented.

## Design principles

- Keep the emulated NES clock independent from the Android display refresh rate.
- Treat input capture time and the NES controller's serial-read timing as separate concepts.
- Preserve original hardware behavior in the accuracy-focused mode.
- Make enhancements opt-in and never silently change game logic.
- Avoid blocking the emulation/audio critical paths with file I/O, UI work, or resource loading.
- Keep the core portable C++20; Android is a frontend, not a dependency of the emulator core.
- No cloud service or account is required for local play.

## Target architecture

| Module | Responsibility | Status |
|---|---|---|
| Core types | Shared timing and hardware types | Initial |
| Cartridge | iNES header parsing and mapper metadata | Initial |
| Controller/input | Timestamped host events and NES serial port | Initial |
| CPU (Ricoh 2A03) | 6502 instruction execution and cycle timing | Planned |
| PPU | Scanline/dot timing, sprites, background and registers | Planned |
| APU | NES audio channels and frame sequencer | Planned |
| Mappers | Cartridge banking and IRQ behavior | Mapper 0 validation planned |
| Android frontend | Kotlin, JNI, lifecycle and file picker | Planned |
| Presentation | OpenGL ES/Vulkan, frame pacing and optional shaders | Planned |
| Audio backend | Low-latency Android audio via Oboe | Planned |
| Diagnostics | Tests, profiling and compatibility reports | Planned |

## Refresh rate is not emulation speed

A 90/120/144 Hz display must not make an NTSC or PAL game run faster. The emulation core will advance using the selected console-region clock. The frontend will present the latest completed frame according to Android's display cadence. Repeated frames are acceptable; optional interpolation may be explored later, but it must not feed back into emulated state.

144 Hz is a **presentation target**, not a promise that the NES produces 144 unique game frames per second. Android scheduling, display mode availability, GPU load and thermal policy remain outside the application's absolute control.

## Input latency model

Host input is timestamped as soon as the platform delivers it. The NES controller still exposes button state through its original serial protocol when the emulated CPU reads the controller port. Events are never applied retroactively to CPU reads that have already happened. The queue must preserve press and release edges and must not turn a short tap into an indefinitely held button.

## Build the portable core

Requirements: CMake 3.22+ and a C++20 compiler.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The first tests exercise the cartridge header parser and controller event path. They are foundation tests, not a compatibility test suite.

## Repository layout

- `core/`: portable emulator library and tests.
- `android/`: Android application (to be added).
- `docs/`: architecture notes and development roadmap.

## License

NES Revolution is licensed under the **MIT License**. See [LICENSE](LICENSE) for the complete license text. The copyright notice is attributed to Sfj3861, 2026.

## Legal note

NES Revolution will not bundle commercial game ROMs, BIOS files, or copyrighted game assets. Users are responsible for complying with applicable law when supplying their own game data.

## Roadmap

1. Portable core foundation and automated tests.
2. CPU bus, RAM and 2A03 instruction/cycle implementation.
3. PPU timing and rendering.
4. APU and audio backend.
5. Mapper framework and cartridge compatibility.
6. Android application, input devices and display integration.
7. Frame pacing, performance telemetry, rewind and optional enhancements.
8. Accuracy, compatibility, thermal and device testing.
