#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#if defined(ARDUINO_ARCH_RP2040) || defined(PICO_RP2040) || defined(PICO_PLATFORM)
#include <hardware/pio.h>
#endif

namespace motion::pio
{

struct StepperCommand
{
  uint32_t stepCount = 0;
  uint32_t delayTicks = 0;
  bool directionHigh = true;
};

struct CommandBuffer
{
  std::array<StepperCommand, 2> slots{};
  std::array<bool, 2> occupied{};
};

#if defined(ARDUINO_ARCH_RP2040) || defined(PICO_RP2040) || defined(PICO_PLATFORM)
using ::pio_program;
#else
struct pio_program
{
  const uint16_t *instructions;
  uint8_t length;
  int8_t origin;
};
#endif

constexpr uint32_t kDefaultPioClockHz = 125'000'000U;

const pio_program &StepDirProgram();
std::string_view StepDirProgramSource();
uint32_t DelayTicksFromMicros(uint32_t halfPeriodMicros, uint32_t clockHz = kDefaultPioClockHz);

} // namespace motion::pio
