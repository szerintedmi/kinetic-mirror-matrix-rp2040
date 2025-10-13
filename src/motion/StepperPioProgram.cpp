#include "motion/StepperPioProgram.hpp"

#include <cstddef>
#include <cstdint>

#if defined(ARDUINO_ARCH_RP2040) || defined(PICO_RP2040) || defined(PICO_PLATFORM)
#include <hardware/pio_instructions.h>
#include <array>
#endif

namespace motion::pio
{

namespace
{

#if defined(ARDUINO_ARCH_RP2040) || defined(PICO_RP2040) || defined(PICO_PLATFORM)
std::array<uint16_t, 12> BuildStepDirInstructions()
{
  std::array<uint16_t, 12> instructions{};
  instructions[0] = static_cast<uint16_t>(pio_encode_pull(true, false));           // pull block
  instructions[1] = static_cast<uint16_t>(pio_encode_mov(pio_y, pio_osr));         // move delay into Y
  instructions[2] = static_cast<uint16_t>(pio_encode_pull(true, false));           // pull block
  instructions[3] = static_cast<uint16_t>(pio_encode_mov(pio_x, pio_osr));         // move step count into X
  instructions[4] = static_cast<uint16_t>(pio_encode_pull(true, false));           // pull block
  instructions[5] = static_cast<uint16_t>(pio_encode_out(pio_pins, 1));            // update direction pin
  instructions[6] = static_cast<uint16_t>(pio_encode_irq_set(false, 0));           // mark command latched
  instructions[7] = static_cast<uint16_t>(pio_encode_set(pio_pins, 1) | (1u << 5)); // set step high, min hold
  instructions[8] = static_cast<uint16_t>(pio_encode_nop() | (31u << 5));          // delay via sideset
  instructions[9] = static_cast<uint16_t>(pio_encode_set(pio_pins, 0) | (1u << 5)); // set step low, min hold
  instructions[10] = static_cast<uint16_t>(pio_encode_nop() | (31u << 5));         // delay
  instructions[11] = static_cast<uint16_t>(pio_encode_jmp_x_dec(7));               // loop while X > 0
  return instructions;
}
#else
constexpr uint16_t kStepDirProgramInstructions[] = {
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000};

const pio_program kStepDirProgram{
    kStepDirProgramInstructions,
    static_cast<uint8_t>(sizeof(kStepDirProgramInstructions) / sizeof(kStepDirProgramInstructions[0])),
    -1};
#endif

constexpr char kProgramSource[] =
    R"PIO(
.program step_dir
.side_set 1 opt
.wrap_target
pull block            ; delay (Y)
mov y, osr
pull block            ; step count (X)
mov x, osr
pull block            ; direction bit
out pins, 1
set pins, 1           ; STEP high
nop [31]
set pins, 0           ; STEP low
nop [31]
jmp x--, step_dir_loop
.wrap
step_dir_loop:
jmp y--, step_dir_loop
)PIO";

} // namespace

const pio_program &StepDirProgram()
{
#if defined(ARDUINO_ARCH_RP2040) || defined(PICO_RP2040) || defined(PICO_PLATFORM)
  static std::array<uint16_t, 12> instructions = BuildStepDirInstructions();
  static const ::pio_program program{
      instructions.data(),
      static_cast<uint8_t>(instructions.size()),
      -1};
  return program;
#else
  return kStepDirProgram;
#endif
}

std::string_view StepDirProgramSource()
{
  return std::string_view(kProgramSource, sizeof(kProgramSource) - 1);
}

uint32_t DelayTicksFromMicros(uint32_t halfPeriodMicros, uint32_t clockHz)
{
  if (halfPeriodMicros == 0 || clockHz == 0)
  {
    return 0;
  }
  uint64_t ticks = (static_cast<uint64_t>(clockHz) * static_cast<uint64_t>(halfPeriodMicros)) / 1'000'000ULL;
  if (ticks == 0)
  {
    return 1;
  }
  if (ticks > 0xFFFFFFu)
  {
    ticks = 0xFFFFFFu;
  }
  return static_cast<uint32_t>(ticks);
}

} // namespace motion::pio
