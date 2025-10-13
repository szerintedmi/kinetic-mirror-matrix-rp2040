### Serial Command Processor Core
**Location:** `src/CommandProcessor.cpp`, `include/control/CommandProcessor.hpp`

Implemented a stateless-friendly parser that accepts `<VERB>[:payload]` commands, validates payload length, normalizes verbs, and executes per-channel actions for `MOVE`, `SLEEP`, `WAKE`, and `STATUS`. Maintains motor state (position, target, speed, acceleration, sleep flag, last error) using fixed-size arrays sized for eight channels and returns structured responses prefixed with `CTRL:`. `HELP` output is generated from a shared descriptor table, ensuring the firmware README and runtime help stay aligned. `HOME` is parsed and reported as `ERR_NOT_READY` to preserve the verb contract until the motion engine lands.

**Rationale:** Keeps command parsing self-contained, unit-testable, and free of Arduino dependencies so higher layers can integrate the same logic in firmware or host simulators. Static buffer usage adheres to MCU RAM predictability requirements.

### Arduino Serial Frontend
**Location:** `src/main.cpp`

Wrapped the Arduino entry point in `#ifdef ARDUINO` and wired a simple line buffer that feeds commands into the shared parser, handles overflow, and emits multi-line responses. Startup announces readiness and resets parser state so host tools have a known baseline.

**Rationale:** Separating MCU-specific I/O from parsing logic keeps the command processor portable for host-native tests while satisfying the RP2040 serial framing requirements.

### Command Surface Documentation
**Location:** `docs/firmware/README.md`

Published the `HELP` verb output along with a tabular summary of verbs, payload formats, response codes, and defaults. Noted that `HOME` currently returns `ERR_NOT_READY` pending Task Group 2 integration.

**Rationale:** Provides a single authoritative reference for developers and lab operators, mirroring the runtime `HELP` output per the spec.

## Database Changes (if applicable)

### Migrations
- None

### Schema Impact
Not applicable.

## Dependencies (if applicable)

### New Dependencies Added
- None

### Configuration Changes
- Added `[env:native]` to `platformio.ini` with `-std=gnu++17` and `test_build_src` to enable host-native testing, while extending the RP2040 environment to the same C++ standard.

## Testing

### Test Files Created/Updated
- `test/test_command_surface/test_main.cpp` - Verifies HELP coverage, MOVE overrides, SLEEP/WAKE state transitions, and STATUS responses.

### Test Coverage
- Unit tests: ✅ Complete
- Integration tests: ❌ None
- Edge cases covered: Payload length overflow handling, invalid channel rejection, default MOVE parameters.

### Manual Testing Performed
None (host-native automated coverage only).

## User Standards & Preferences Compliance

### Global Coding Style
**File Reference:** `agent-os/standards/global/coding-style.md`

**How Your Implementation Complies:** Modules remain small and focused (`CommandProcessor` encapsulates parsing; Arduino glue only handles serial I/O), with descriptive naming and no dead code.

**Deviations (if any):** None.

### Global Resource Management
**File Reference:** `agent-os/standards/global/resource-management.md`

**How Your Implementation Complies:** Leveraged `std::array` and fixed-size buffers for command and response handling, eliminating dynamic allocation and safeguarding against overflow.

**Deviations (if any):** None.

### Backend Hardware Abstraction
**File Reference:** `agent-os/standards/backend/hardware-abstraction.md`

**How Your Implementation Complies:** Exposed the command processor as a platform-agnostic component under `include/control/`, keeping board-specific serial handling isolated in `src/main.cpp`.

**Deviations (if any):** None.

### Testing Unit Testing
**File Reference:** `agent-os/standards/testing/unit-testing.md`

**How Your Implementation Complies:** Added targeted Unity tests focused on parser behavior with deterministic inputs, executed only the new suite per instructions.

**Deviations (if any):** None.

## Integration Points (if applicable)

### APIs/Endpoints
- Not applicable (serial command deck only).

### External Services
- None.

### Internal Dependencies
- Future motion manager (Task Group 2) will consume the motor state maintained by `CommandProcessor`.

## Known Issues & Limitations

### HOME Command Pending
1. **HOME Command Pending**
   - Description: `HOME` currently returns `CTRL:ERR_NOT_READY`.
   - Impact: Homing cannot be invoked until Task Group 2 implements the motion manager.
   - Workaround: None; task is scheduled for the next group.
   - Future Consideration: Wire to the motion engine and update tests once available.

## Performance Considerations
Command parsing uses fixed-size loops and buffers sized under 1 KB. Status responses cap at nine lines to bound serial output time.

## Security Considerations
Validated payload lengths, channel ranges, and numeric arguments before mutating state, preventing buffer overruns and malformed commands from affecting firmware.

## Dependencies for Other Tasks
- Task Group 2 (Motor Manager & PIO Execution) relies on the command table and motor state structure established here.

## Notes
- Parser behavior references `planning/code-examples/StepperMotor` patterns for sleep/wake state tracking to ease future integration with the motion manager.
