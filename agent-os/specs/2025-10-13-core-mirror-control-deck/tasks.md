# Task Breakdown: Core Mirror Control Deck

## Overview

Total Tasks: 21
Assigned roles: api-engineer, testing-engineer

## Task List

### Firmware Serial Interface

#### Task Group 1: Command Surface & Parser

**Assigned implementer:** api-engineer  
**Dependencies:** None

- [x] 1.0 Establish serial command surface
  - [x] 1.1 Write 4 focused host-native tests covering `HELP`, absolute `MOVE` parameter validation (speed/accel overrides), `SLEEP`/`WAKE` toggles, and structured `STATUS` replies (2-8 tests max).
  - [x] 1.2 Review `planning/code-examples/StepperMotor.{h,cpp}` to capture reusable motion state and driver wake/sleep patterns.
  - [x] 1.3 Define command table and shared verb documentation aligned with `<VERB>[:payload]` format and echo prefixes.
  - [x] 1.4 Implement robust parser with payload length guards, argument coercion to motion profiles, and consistent error codes.
  - [x] 1.5 Wire status/error reporting paths so faults and acknowledgements return `CTRL:CODE`-style responses and update firmware README with command help output.
  - [x] 1.6 Run only the tests from 1.1 and confirm all pass.

**Acceptance Criteria:**

- Command verbs documented via `HELP` and parsed without buffer overruns.
- `MOVE` commands accept absolute targets plus optional speed/accel fields; missing fields fall back to defaults.
- Sleep/wake commands operate per motor channel and reflect state in responses.
- Status queries report position, motion state, and last error code on demand.
- Documentation shipped alongside firmware lists verbs, payload formats, and reuse decisions traced to the ESP32 prototype.

### Motion Control Core

#### Task Group 2: Motor Manager & PIO Execution

**Assigned implementer:** api-engineer  
**Dependencies:** Task Group 1

- [x] 2.0 Implement motion engine for eight motors
  - [x] 2.1 Write 5 focused motion-control tests (host-simulated) verifying limit enforcement, homing zeroing, autosleep transitions, step timing calculations, and fault handling (2-8 tests max).
  - [x] 2.2 Map homing and autosleep behaviors from the ESP32 prototype into RP2040 abstractions, noting any divergences in a design doc comment.
  - [x] 2.3 Author PIO program (and buffering glue) generating deterministic step/dir waves using compile-time buffers and double-queued command slots.
  - [x] 2.4 Build motor manager tracking per-channel state, enforcing ±1200 step limits, and computing move durations from speed/accel inputs.
  - [x] 2.5 Implement homing routine that leverages configurable travel range and defaults, ensuring overrides from serial parameters are honored.
  - [x] 2.6 Integrate autosleep: wake on motion demand, sleep immediately after motion completion, expose explicit SLEEP/WAKE verbs for calibration, and drive per-motor SLEEP lines through an SN74HC595 (or equivalent) so eight motors retain independent control without exhausting RP2040 GPIO.
  - [x] 2.7 Run only the tests from 2.1 and ensure they pass.

**Acceptance Criteria:**

- PIO pulse generation meets requested speed/accel profiles without jitter that would violate timing budget.
- Each RP2040 instance controls eight motors with isolated state, respecting configured limits and reporting violations.
- Homing establishes midpoint zero and surfaces success/error codes over serial.
- Autosleep reliably gates DRV8825 sleep pins via the SN74HC595 outputs without leaving motors energized after moves, preserving independent control across all eight channels within RP2040 GPIO limits.
- Notes in code or README explain how RP2040 implementation draws from the ESP32 FastAccelStepper prototype while acknowledging MCU-specific differences.

### Feature Validation

#### Task Group 3: Integration & Hardware Validation

**Assigned implementer:** testing-engineer  
**Dependencies:** Task Group 1, Task Group 2

- [ ] 3.0 Validate end-to-end behavior
  - [ ] 3.1 Write 4-6 integration tests or scripted sequences that issue HELP, MOVE, HOME, STATUS, and SLEEP/WAKE flows, asserting serial responses and position updates (2-8 tests max).
  - [ ] 3.2 Extend `scripts/smoke_test.py` (or add new) to cycle through homing, motion bounds, and fault injection, logging firmware git hash and config version.
  - [ ] 3.3 Update hardware validation checklist to include autosleep verification, limit enforcement, and error-report review, referencing reused prototype behaviors where applicable.
  - [ ] 3.4 Capture a short reuse summary noting how the ESP32 prototype informed verification scripts for future auditors.
  - [ ] 3.5 Run only the tests from 1.1, 2.1, and 3.1 to confirm feature readiness; do not execute unrelated suites.

**Acceptance Criteria:**

- Integration tests cover the full command flow from serial issuance to motor state updates and status reporting.
- Smoke script automates homing/motion cycles and captures logs for lab operators following standards.
- Hardware checklist reflects new behaviors and can be followed without IDE access.
- Reuse summary and checklist clearly state which prototype concepts were adopted or adjusted.
- Combined focused tests from groups 1-3 pass consistently on native or hardware-in-loop targets.
