# Research Notes & Requirements

## Clarifying Q&A

1. **Question:** I assume the first milestone is RP2040 firmware exposing the serial command set to a single host laptop over USB, with no network transport yet—should we scope it that tightly, or do you need early hooks for future masters?  
   **Answer:** yes serial commands only to start with
2. **Question:** I’m thinking the command vocabulary should cover absolute/relative moves, homing, per-motor sleep, and a status poll; should we plan for any additional primitives (e.g., velocity ramps, emergency stop) in this pass?  
   **Answer:** we don't  need relative moves yet. we need sleep / wake command (for driver vref calibration) but the code should automaticall keep motors sleep (no need holding torque as these are actuators) and only wake them while moving. To avoid storing configs the commands should take acceleration and speed params. Based on these two the moving time can be calculated.
3. **Question:** I’m assuming each RP2040 node will juggle up to four DRV8825-driven mirrors; is that the correct target, or should we size the design for a different motor count?  
   **Answer:** each RP2040 can and should be able to handle 8 motors
4. **Question:** For reliable step/dir generation, I’m planning to lean on PIO with a double-buffered queue from the main core—do you prefer a simpler timer-ISR approach to keep firmware more straightforward?  
   **Answer:** yes, PIO!
5. **Question:** I’m assuming we’ll add a host-side CLI shim to send scripted sequences during bring-up; do you want that included now, or should we only document the raw serial protocol?  
   **Answer:** start with serial only, we can add a CLI later if it helps testing / tuning
6. **Question:** For per-motor sleep, should we automatically time out idle motors after a configurable dwell, or only honor explicit SLEEP commands from the host?  
   **Answer:** yes, autosleep by default as soon as possible (also see pt 2 omment above)
7. **Question:** I’m planning to log thermal or motion faults back over serial as structured status codes; do you want a heavier heartbeat/telemetry loop or just on-demand reports for now?  
   **Answer:** logging status code on errors on serial is fine for now, no need heartbeat yet. key metrics / status on demand should be available now
8. **Question:** What should we explicitly keep out of scope for this first control deck deliverable (e.g., power-aware scheduling, multi-node orchestration)?  
   **Answer:** keep out as per my comments above and as you suggest, power aware scheduling, multi-node

**Additional Notes from User:**  
The position should be tracked by the module and motion limits enforced. The number of steps between the two limits can be code parameter for now -1200 <--> +1200 full steps , (0 as mid point). It's neede for homing. Speed and acceleration defaults can be also constant in the code. Speed: 4000 steps/s, acceleration: 16000 steps/s^2 but those need to be possible to override in the serial commands

## Existing Code to Reference

Similar Features Identified:

- Feature: ESP32 FastAccelStepper prototype — Path: `agent-os/specs/2025-10-13-core-mirror-control-deck/planning/code-examples/StepperMotor.h`
- Feature: ESP32 FastAccelStepper prototype implementation — Path: `agent-os/specs/2025-10-13-core-mirror-control-deck/planning/code-examples/StepperMotor.cpp`

## Follow-up Questions

No follow-up questions required at this time.

## Visual Assets

No visual assets provided. (`ls` check returned no image files.)

## Requirements Summary

### Functional Requirements

- RP2040 firmware exposes a USB serial command interface for a single host laptop; no network transports in scope.
- Command set must support absolute positioning moves with per-command speed and acceleration parameters, homing, explicit sleep/wake, status queries, and error reporting; relative moves are out of scope.
- Firmware auto-sleeps motors when idle, wakes them for motion, and honors manual sleep/wake commands for driver calibration workflows.
- Each RP2040 controls up to eight DRV8825 stepper channels with position tracking and enforcement of configured motion limits (-1200 to +1200 full steps, midpoint at 0).
- Homing routine must use the configured ±1200 step travel limits to re-establish a zero reference; implementation can ship with sensible default approach/backoff distances but must allow overrides via serial command parameters using the same speed/acceleration defaults as normal moves.
- Status responses expose key metrics on demand and report faults/errors over serial.

### Non-Functional Requirements

- Utilize RP2040 PIO for deterministic step/dir pulse generation with buffering for smooth motion.
- Default motion profile constants: speed 4000 steps/s, acceleration 16000 steps/s²; allow overrides via serial commands without persisting configuration.
- Emphasize low idle power by ensuring motors sleep immediately after moves and no holding torque is maintained unless commanded.

### Reusability Opportunities

- Review the existing FastAccelStepper-based ESP32 prototype (`planning/code-examples/StepperMotor.{h,cpp}`) for conceptual structure or command patterns that can inform the RP2040 implementation.

### Scope Boundaries

**In Scope:**

- RP2040-side firmware covering serial protocol, motion control, autosleep, homing, limit enforcement, and status/error reporting.

**Out of Scope:**

- Power-aware scheduling, host-side command queueing beyond basic serial scripting, multi-node orchestration, and dedicated CLI tooling (documentation of protocol only for now).

### Technical Considerations

- PIO-based step generation coordinated with main core command parsing.
- Commands accept speed/acceleration parameters to compute move durations without persistent storage.
- Enforce configurable step limits per motor to prevent overtravel; consider compile-time or config constants for ±1200 steps.
- Ensure structured serial responses for error codes and on-demand status without periodic heartbeat traffic.
