#pragma once

#include <array>
#include <cstdint>

#include "motion/MotorManager.hpp"

namespace board::rp2040
{

    // STEP/DIR assignments for eight DRV8825 channels.
    inline constexpr std::array<uint8_t, motion::MotorManager::kMotorCount> kStepPins = {
        15, 17, 21, 22, 23, 24, 25, 26};

    inline constexpr std::array<uint8_t, motion::MotorManager::kMotorCount> kDirPins = {
        14, 18, 20, 4, 6, 27, 12, 13};

    // SN74HC595 shift register control lines (data, clock, latch).
    inline constexpr motion::ShiftRegisterPins kShiftRegisterPins{
        18, // SER
        19, // SRCLK
        20  // RCLK
    };

} // namespace board::rp2040
