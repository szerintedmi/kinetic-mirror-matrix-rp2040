#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace motion
{

namespace pio
{
struct CommandBuffer;
}

enum class MotionPhase : uint8_t
{
  Idle = 0,
  Moving,
  Homing
};

enum class FaultCode : uint8_t
{
  None = 0,
  LimitClipped,
  DriverFault,
  HomingTimeout
};

enum class MoveResult : uint8_t
{
  Scheduled = 0,
  ClippedToLimit,
  Busy,
  Fault
};

struct TimingEstimate
{
  uint32_t totalSteps = 0;
  uint32_t accelSteps = 0;
  uint32_t cruiseSteps = 0;
  uint32_t totalDurationUs = 0;
};

struct HomingRequest
{
  long travelRange = 0;
  long backoff = 0;
};

struct MotorState
{
  long position = 0;
  long targetPosition = 0;
  int32_t speedHz = 0;
  int32_t acceleration = 0;
  MotionPhase phase = MotionPhase::Idle;
  bool asleep = true;
  FaultCode fault = FaultCode::None;
  bool limitClipped = false;
  uint32_t plannedDurationUs = 0;
};

struct ShiftRegisterPins
{
  uint8_t data = 0;
  uint8_t clock = 0;
  uint8_t latch = 0;
};

class MotorManager
{
public:
  static constexpr std::size_t kMotorCount = 8;
  static constexpr long kDefaultLimit = 1200;
  static constexpr long kDefaultTravelRange = kDefaultLimit * 2;
  static constexpr long kDefaultBackoff = 50;
  static constexpr int32_t kDefaultSpeedHz = 4000;
  static constexpr int32_t kDefaultAcceleration = 16000;

  MotorManager();

  void reset();

  MoveResult queueMove(std::size_t channel,
                       long targetPosition,
                       int32_t speedHz,
                       int32_t acceleration,
                       TimingEstimate &timing);

  MoveResult beginHoming(std::size_t channel, const HomingRequest &request);

  void service(uint32_t elapsedMicros);

  void forceSleep(std::size_t channel);
  void forceWake(std::size_t channel);

  void injectFault(std::size_t channel, FaultCode fault);
  void clearFault(std::size_t channel);

  const MotorState &state(std::size_t channel) const;

  static TimingEstimate ComputeTiming(uint32_t steps, int32_t speedHz, int32_t acceleration);

  void markCommandExecuted(std::size_t channel);

  void configureShiftRegister(const ShiftRegisterPins &pins, bool activeHigh = false);

  void exportCommandBuffer(std::size_t channel, pio::CommandBuffer &out) const;

private:
  struct CommandSlot
  {
    bool occupied = false;
    TimingEstimate timing{};
    uint32_t stepCount = 0;
    uint32_t halfPeriodMicros = 0;
    bool directionHigh = true;
  };

  struct ActivePlan
  {
    bool active = false;
    bool homingPhase = false;
    uint8_t homingStep = 0;
    bool limitRecorded = false;
    bool backoffRecorded = false;
    uint32_t elapsedUs = 0;
    long startPosition = 0;
    long targetPosition = 0;
    long homingRange = 0;
    long homingBackoff = 0;
    long homingLimitPosition = 0;
    long homingBackoffPosition = 0;
    TimingEstimate timing{};
  };

  class SleepRegister
  {
  public:
    SleepRegister() = default;

    void configure(const ShiftRegisterPins &pins, bool activeHigh);
    void setChannel(std::size_t channel, bool asleep);
    void apply();

  private:
    bool configured_ = false;
    bool activeHigh_ = false;
    ShiftRegisterPins pins_{};
    std::array<bool, MotorManager::kMotorCount> states_{};
  };

  MoveResult commitMove(std::size_t channel,
                        long clampedTarget,
                        int32_t speedHz,
                        int32_t acceleration,
                        uint32_t steps,
                        TimingEstimate &timing,
                        bool clipped);

  void configureHomingStage(std::size_t channel, ActivePlan &plan);
  void updateAutosleep(std::size_t channel);

  std::array<MotorState, kMotorCount> motors_{};
  std::array<std::array<CommandSlot, 2>, kMotorCount> commandSlots_{};
  std::array<uint8_t, kMotorCount> activeSlot_{};
  std::array<ActivePlan, kMotorCount> plans_{};
  SleepRegister sleepRegister_{};
  bool shiftActiveHigh_ = false;
  long positiveLimit_ = kDefaultLimit;
  long negativeLimit_ = -kDefaultLimit;
};

} // namespace motion
