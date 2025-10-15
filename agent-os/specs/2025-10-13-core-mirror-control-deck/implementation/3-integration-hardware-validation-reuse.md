## Reuse Summary – Integration & Hardware Validation

- **ESP32 Autosleep Pattern:** The smoke script's autosleep confirmation step mirrors the ESP32 FastAccelStepper prototype procedure that toggled driver enable pins around motion cycles. We now verify the RP2040 SN74HC595 sleep gating by watching `SLEEP` state transitions during the +/- limit sweep.
- **Prototype Homing Flow:** Homing validation reuses the three-stage approach/backoff/center sequence defined in `planning/code-examples/StepperMotor.cpp`. The integration tests and smoke runner both wait for status transitions that match that finite-state machine.
- **Fault Surfacing Checks:** Limit-clip handling is validated by the same overtravel injection used on the ESP32 bench rig. The script asserts that `MOVE:LIMIT_CLIPPED=1` and `ERR=ERR_LIMIT` echo the prototype’s structured error pattern, easing comparisons for auditors.
- **GPIO Mapping Reference:** Hardware documentation now points to `boards/Rp2040Pins.hpp`, keeping the RP2040 STEP/DIR and SN74HC595 assignments visible to lab techs just like the annotated pin maps we shipped with the ESP32 prototype fixtures.
