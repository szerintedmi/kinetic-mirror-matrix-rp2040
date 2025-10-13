# Specification: Core Mirror Control Deck

## Goal
Deliver RP2040 firmware that exposes a USB serial control deck capable of driving up to eight DRV8825 stepper channels with reliable motion, autosleep behavior, and on-demand status reporting for laptop-based experimentation.

## User Stories
- As a maker running mirror experiments from a laptop, I want to issue absolute position moves over serial so that each mirror reaches repeatable angles without manual tuning.
- As a hardware tinkerer calibrating drivers, I want explicit sleep and wake commands so that I can safely adjust Vref without power cycling hardware.
- As a developer verifying installations, I want to request homing and status reports so that I can quickly confirm motors respect travel limits and surface any faults.

## Core Requirements
### Functional Requirements
- Provide a text-based serial protocol (USB CDC) with documented verbs: `HELP`, `MOVE`, `HOME`, `STATUS`, `SLEEP`, `WAKE`, and structured error responses.
- Accept absolute move commands per motor channel, with optional speed and acceleration parameters; fallback to firmware defaults (4000 steps/s, 16000 steps/s²) when omitted.
- Track motor position internally and enforce configurable ±1200 full-step soft limits (midpoint at 0) per channel; reject or clip commands that exceed limits.
- Implement a homing routine that leverages the configured travel range to re-establish zero and allows command overrides for approach/backoff distances while retaining default behaviors in code.
- Manage autosleep: wake drivers immediately before motion, provide host-addressable `SLEEP`/`WAKE` verbs, and return motors to sleep as soon as motion completes.
- Drive per-motor SLEEP pins through an SN74HC595 (or equivalent) shift register so eight drivers can be gated independently without exceeding the RP2040's available GPIO.
- Surface structured status on demand, including current position, motion state, sleep state, and last error/fault code.
- Support simultaneous stewardship of eight DRV8825 channels per RP2040, isolating state and error handling per motor.

### Non-Functional Requirements
- Generate deterministic step/dir pulse trains using RP2040 PIO backed by buffered command queues so motion timing is not impacted by main-core parsing.
- Guard serial parsing against buffer overflow, enforce payload length limits, and echo responses with component prefixes per serial-interface standards.
- Favor static buffers and predictable resource usage to remain within RP2040 memory constraints while keeping ~20% headroom for future features.
- Minimize idle power consumption by keeping drivers disabled when motors are not actively moving.

## Visual Design
- No visual assets were supplied; all interactions occur over the documented serial command interface.

## Reusable Components
### Existing Code to Leverage
- `planning/code-examples/StepperMotor.h` and `.cpp`: ESP32 + FastAccelStepper prototype showcasing motion state tracking, homing state machine scaffolding, and autosleep patterns that can inform RP2040 abstractions.
- Existing PlatformIO scaffold (`platformio.ini`, `src/main.cpp`) for Arduino-on-RP2040 builds; reuse environment configuration rather than creating a new project layout.

### New Components Required
- RP2040-specific PIO program and driver glue for multi-channel step/dir generation because the prototype targets ESP32 timers rather than RP2040 PIO.
- Serial command dispatcher and routing layer that maps verbs to per-motor handlers while enforcing payload validation—current project lacks reusable command parsing infrastructure.
- Motor manager module coordinating eight channels, position tracking, and limit enforcement tailored to RP2040 registers and the selected pin map.

## Technical Approach
- **Database:** No traditional database; store default limits, speed, and acceleration as compile-time constants or `constexpr` configuration aligned with board headers. Provide runtime overrides via commands without persisting to flash in this iteration.
- **API:** Implement a lightweight serial API using `<VERB>[:payload]\n` framing. Include `HELP` output listing verbs, require component-prefixed acknowledgements (`CTRL:OK`, `CTRL:ERR_CODE`), and enforce argument validation before queuing motion.
- **Frontend:** No UI component in scope. Document serial usage and example command sequences in firmware README; host-side CLI tooling is explicitly deferred.
- **Testing:** Create host-native unit tests for parsing, motion scheduling math, and limit enforcement. Add hardware-in-loop or smoke scripts (Python) that issue command sequences and assert responses, logging firmware git hash per hardware-validation standard.

## Out of Scope
- Power-aware scheduling, concurrency throttling, and thermal budgeting beyond basic autosleep.
- Multi-node orchestration or network transports (Wi-Fi, RS485, ESP32 masters).
- Host-side CLI or GUI tooling beyond documenting the serial protocol.
- Persisting configuration to flash or supporting dynamic reconfiguration across boots.

## Success Criteria
- Serial `HELP`, `MOVE`, `HOME`, `STATUS`, `SLEEP`, and `WAKE` commands execute reliably with guarded parsing and clear acknowledgements.
- Each RP2040 controls eight motors, enforces ±1200 step limits, and keeps accurate position counters through homing and motion cycles.
- Drivers remain asleep outside active moves, reducing idle draw while allowing calibration via explicit wake commands.
- Focused test suites (host-native plus smoke script) cover command parsing, motion limit enforcement, homing flows, and autosleep transitions, all passing within the expected 16–34 test budget.
