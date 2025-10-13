# Product Tech Stack

## Motor Control Layer
- **Microcontroller:** Raspberry Pi RP2040 on each motor node; chosen for dual-core MCU, PIO blocks, and ample GPIO.
- **Firmware Language & Build:** Modern C++ (C++17) built with PlatformIO using the pico-sdk toolchain, while keeping the door open to Arduino-compatible components if specific libraries make life easier.
- **Motion Drivers:** DRV8825 step/dir stepper drivers with SN74HC595 shift registers for per-motor SLEEP gating, per requirements.
- **Pulse Generation:** RP2040 PIO state machines schedule deterministic step pulses while main cores manage command parsing and safety.
- **Configuration Storage:** RP2040 flash (LittleFS) for node IDs, zero offsets, and runtime configuration.

## Control & Scheduling Layer
- **Phase 1 Master Interface:** USB serial command channel from a host laptop (Python or CLI tooling) into each RP2040 for direct experimentation.
- **Command Protocol:** Text-based, AT-style commands over serial that wrap the `SET_ABS`, `SET_REL`, `HOME`, and `SLEEP` primitives—human-readable, low-frequency, and easy to script.
- **Scheduler:** Power-aware queue running on the host side (initial CLI) that enforces concurrency caps and thermal budgets before dispatching to RP2040 nodes.
- **Diagnostics:** Poll-based status word (busy/thermal bits) surfaced via the same serial link and exposed in a CLI health summary.

## Geometry & Planning
- **Shared Math Core:** Portable C++ library (header-only plus static library build) implementing ray geometry, reachability checks, and timing estimates.
- **Host Harness:** PlatformIO-native test target plus a CLI tool (C++ with `fmt`/`CLI11`) to convert wall coordinates into mirror angles for scripting workflows.
- **Data Formats:** Human-readable JSON/YAML scene files describing panels, targets, and timelines.

## Creative Tooling
- **Pattern Sequencer:** CLI helper (Python 3.11 or C++ CLI) that stores cue lists, timing, and replay scripts, reusing the shared math core for validation before we expose the same flow through the web UI.
- **Live Play Hooks:** Rich web client calling ESP32-hosted HTTP/WebSocket endpoints to push target patterns in real time while the scheduler guards power and motion limits.
- **Scene Sandbox Preview:** Browser-first simulator that renders reachability and playback timelines using shared C++ geometry compiled to WebAssembly (via Emscripten) so the logic matches the runner without duplicate code.

## Multi-Island Expansion (Phase 2+)
- **Master Hardware:** ESP32-based Island Master with ESP-IDF (C++) hosting web UI and scheduling logic once serial-host experiments graduate, with the option to pull in Arduino libraries where they accelerate development.
- **Transport Backbone:** Evaluate ESP-NOW, Wi-Fi, or wired RS485 links between masters and RP2040 nodes; select per installation based on interference, distance, and latency requirements. Use protobuf-lite framing for versioned commands and diagnostics regardless of physical layer.
- **Global Coordinator:** Optional ESP32 Global Master running ESP-IDF/PlatformIO orchestrating multiple islands with shared power budgets.

## Tooling & Testing
- **CI & Unit Tests:** PlatformIO native environment for C++ unit tests; `Unity` or `Catch2` as the test framework for the shared math core and scheduler logic.
- **Hardware-in-the-loop:** PlatformIO test runner on RP2040 dev kits validating PIO timing and driver integration.
- **Formatting & Linting:** clang-format with repository-standard config; clang-tidy for static analysis on firmware and shared core.
- **Resource Tracking:** Scripted build metrics (PlatformIO `pio run -t size`) logged per build to enforce resource-management standards.

## Documentation & Ops
- **Configuration Management:** Versioned YAML/JSON configs committed under `config/` with templates reflecting power limits and geometry defaults.
- **Logging & Telemetry:** Start with serial logs captured by the host, persist them for later review, and evolve toward streaming summaries over WebSocket or similar channels as the ESP32 mastering matures.
- **Dev Environment:** macOS/Linux host machines with Python, PlatformIO CLI, CMake toolchain, and the Emscripten SDK for WASM builds when we target browser previews.
