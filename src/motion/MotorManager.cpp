#include "motion/MotorManager.hpp"
#include "motion/StepperPioProgram.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#if defined(ARDUINO)
#include <Arduino.h>
#endif

// Adapts the ESP32 FastAccelStepper prototype's autosleep and homing sequencing
// to RP2040 expectations by replacing timer-driven ISR nudges with the
// RP2040's double-buffered PIO command slots while the SN74HC595 shift-register
// keeps per-channel sleep control on parity with the original ESP32 design.
namespace motion
{

namespace
{
constexpr uint32_t kMicrosPerSecond = 1'000'000U;
}

MotorManager::MotorManager()
{
  reset();
}

void MotorManager::reset()
{
  for (std::size_t i = 0; i < kMotorCount; ++i)
  {
    motors_[i] = MotorState{};
    motors_[i].speedHz = kDefaultSpeedHz;
    motors_[i].acceleration = kDefaultAcceleration;
    motors_[i].phase = MotionPhase::Idle;
    motors_[i].asleep = true;
    motors_[i].fault = FaultCode::None;
    motors_[i].limitClipped = false;
    motors_[i].plannedDurationUs = 0;

    commandSlots_[i][0] = CommandSlot{};
    commandSlots_[i][1] = CommandSlot{};
    activeSlot_[i] = 0;

    plans_[i] = ActivePlan{};

    sleepRegister_.setChannel(i, true);
  }
  sleepRegister_.apply();
}

MoveResult MotorManager::queueMove(std::size_t channel,
                                   long targetPosition,
                                   int32_t speedHz,
                                   int32_t acceleration,
                                   TimingEstimate &timing)
{
  if (channel >= kMotorCount)
  {
    return MoveResult::Fault;
  }

  auto &motor = motors_[channel];
  if (motor.phase == MotionPhase::Homing)
  {
    return MoveResult::Busy;
  }

  if (motor.fault == FaultCode::DriverFault)
  {
    return MoveResult::Fault;
  }

  // Determine which slot we can use without stalling the double-buffered pipeline.
  uint8_t slotToUse = activeSlot_[channel];
  if (commandSlots_[channel][slotToUse].occupied)
  {
    uint8_t alternate = static_cast<uint8_t>((slotToUse + 1U) % 2U);
    if (commandSlots_[channel][alternate].occupied)
    {
      return MoveResult::Busy;
    }
    slotToUse = alternate;
  }
  activeSlot_[channel] = slotToUse;

  long clamped = std::max(negativeLimit_, std::min(positiveLimit_, targetPosition));
  bool clipped = (clamped != targetPosition);
  uint32_t steps = static_cast<uint32_t>(std::llabs(clamped - motor.position));
  timing = ComputeTiming(steps, speedHz, acceleration);

  return commitMove(channel, clamped, speedHz, acceleration, steps, timing, clipped);
}

MoveResult MotorManager::commitMove(std::size_t channel,
                                    long clampedTarget,
                                    int32_t speedHz,
                                    int32_t acceleration,
                                    uint32_t steps,
                                    TimingEstimate &timing,
                                    bool clipped)
{
  auto &motor = motors_[channel];
  auto &plan = plans_[channel];

  motor.targetPosition = clampedTarget;
  motor.speedHz = speedHz;
  motor.acceleration = acceleration;
  motor.limitClipped = clipped;
  motor.plannedDurationUs = timing.totalDurationUs;

  if (timing.totalSteps == 0 || timing.totalDurationUs == 0)
  {
    motor.position = clampedTarget;
    motor.phase = MotionPhase::Idle;
    motor.asleep = true;
    motor.fault = clipped ? FaultCode::LimitClipped : FaultCode::None;
    plan = ActivePlan{};
    commandSlots_[channel][activeSlot_[channel]].occupied = false;
    updateAutosleep(channel);
    return clipped ? MoveResult::ClippedToLimit : MoveResult::Scheduled;
  }

  plan.active = true;
  plan.homingPhase = false;
  plan.homingStep = 0;
  plan.elapsedUs = 0;
  plan.startPosition = motor.position;
  plan.targetPosition = clampedTarget;
  plan.timing = timing;
  plan.homingRange = 0;
  plan.homingBackoff = 0;

  auto &slot = commandSlots_[channel][activeSlot_[channel]];
  slot = CommandSlot{};
  slot.occupied = true;
  slot.timing = timing;
  slot.stepCount = steps;
  double clampedSpeed = static_cast<double>(std::max<int32_t>(1, speedHz));
  double stepPeriodUs = static_cast<double>(kMicrosPerSecond) / clampedSpeed;
  uint32_t periodUs = static_cast<uint32_t>(std::llround(std::max(1.0, stepPeriodUs)));
  slot.halfPeriodMicros = std::max<uint32_t>(1U, periodUs / 2U);
  slot.directionHigh = (clampedTarget >= plan.startPosition);

  motor.phase = MotionPhase::Moving;
  motor.asleep = false;
  motor.fault = clipped ? FaultCode::LimitClipped : FaultCode::None;
  updateAutosleep(channel);

  return clipped ? MoveResult::ClippedToLimit : MoveResult::Scheduled;
}

MoveResult MotorManager::beginHoming(std::size_t channel, const HomingRequest &request)
{
  if (channel >= kMotorCount)
  {
    return MoveResult::Fault;
  }

  auto &motor = motors_[channel];
  if (motor.phase == MotionPhase::Moving)
  {
    return MoveResult::Busy;
  }

  long range = (request.travelRange == 0) ? kDefaultTravelRange : request.travelRange;
  if (range < 2L)
  {
    return MoveResult::Fault;
  }
  long backoff = (request.backoff == 0) ? kDefaultBackoff : request.backoff;
  if (backoff < 0)
  {
    backoff = 0;
  }
  if (backoff >= range)
  {
    backoff = range - 1;
  }

  uint8_t slotToUse = activeSlot_[channel];
  if (commandSlots_[channel][slotToUse].occupied)
  {
    uint8_t alternate = static_cast<uint8_t>((slotToUse + 1U) % 2U);
    if (commandSlots_[channel][alternate].occupied)
    {
      return MoveResult::Busy;
    }
    slotToUse = alternate;
  }
  activeSlot_[channel] = slotToUse;

  auto &plan = plans_[channel];
  plan = ActivePlan{};
  plan.homingPhase = true;
  plan.homingStep = 0;
  plan.homingRange = range;
  plan.homingBackoff = backoff;

  motor.phase = MotionPhase::Homing;
  motor.asleep = false;
  motor.limitClipped = false;
  motor.fault = FaultCode::None;

  configureHomingStage(channel, plan);
  if (!plan.active)
  {
    motor.position = 0;
    motor.targetPosition = 0;
    motor.phase = MotionPhase::Idle;
    motor.asleep = true;
    motor.plannedDurationUs = 0;
    updateAutosleep(channel);
    return MoveResult::Scheduled;
  }

  motor.plannedDurationUs = plan.timing.totalDurationUs;
  updateAutosleep(channel);
  return MoveResult::Scheduled;
}

void MotorManager::service(uint32_t elapsedMicros)
{
  if (elapsedMicros == 0)
  {
    return;
  }

  for (std::size_t channel = 0; channel < kMotorCount; ++channel)
  {
    auto &plan = plans_[channel];
    auto &motor = motors_[channel];

    if (!plan.active)
    {
      continue;
    }

    uint64_t elapsed = static_cast<uint64_t>(plan.elapsedUs) + elapsedMicros;
    if (elapsed > plan.timing.totalDurationUs)
    {
      elapsed = plan.timing.totalDurationUs;
    }
    plan.elapsedUs = static_cast<uint32_t>(elapsed);

    if (plan.timing.totalDurationUs > 0)
    {
      double progress = static_cast<double>(plan.elapsedUs) / static_cast<double>(plan.timing.totalDurationUs);
      long delta = plan.targetPosition - plan.startPosition;
      long updated = plan.startPosition + static_cast<long>(std::llround(progress * static_cast<double>(delta)));
      motor.position = updated;
    }

    if (plan.elapsedUs >= plan.timing.totalDurationUs)
    {
      motor.position = plan.targetPosition;
      commandSlots_[channel][activeSlot_[channel]].occupied = false;

      if (plan.homingPhase)
      {
        if (plan.homingStep == 0)
        {
          plan.limitRecorded = true;
          plan.homingLimitPosition = motor.position;
        }
        else if (plan.homingStep == 1)
        {
          plan.backoffRecorded = true;
          plan.homingBackoffPosition = motor.position;
        }

        ++plan.homingStep;
        if (plan.homingStep <= 2)
        {
          uint8_t nextSlot = static_cast<uint8_t>((activeSlot_[channel] + 1U) % 2U);
          activeSlot_[channel] = nextSlot;
          configureHomingStage(channel, plan);
          if (plan.active)
          {
            motor.phase = MotionPhase::Homing;
            motor.asleep = false;
            motor.plannedDurationUs = plan.timing.totalDurationUs;
            updateAutosleep(channel);
            continue;
          }
        }

        plan = ActivePlan{};
        motor.position = 0;
        motor.targetPosition = 0;
        motor.phase = MotionPhase::Idle;
        motor.asleep = true;
        motor.limitClipped = false;
        motor.fault = FaultCode::None;
        motor.plannedDurationUs = 0;
        updateAutosleep(channel);
        continue;
      }

      plan = ActivePlan{};
      motor.phase = MotionPhase::Idle;
      motor.position = motor.targetPosition;
      motor.asleep = true;
      motor.plannedDurationUs = 0;
      updateAutosleep(channel);
    }
  }
}

void MotorManager::configureHomingStage(std::size_t channel, ActivePlan &plan)
{
  auto &motor = motors_[channel];

  if (plan.homingStep > 2)
  {
    plan.active = false;
    return;
  }

  plan.startPosition = motor.position;
  switch (plan.homingStep)
  {
  case 0:
    plan.targetPosition = plan.startPosition - plan.homingRange;
    break;
  case 1:
    plan.targetPosition = plan.startPosition + plan.homingBackoff;
    break;
  case 2:
  {
    long limitBase = plan.limitRecorded ? plan.homingLimitPosition : (plan.startPosition - plan.homingBackoff);
    long centerOffset = plan.homingRange / 2L;
    plan.targetPosition = limitBase + centerOffset;
    break;
  }
  default:
    plan.active = false;
    return;
  }

  uint32_t steps = static_cast<uint32_t>(std::llabs(plan.targetPosition - plan.startPosition));
  plan.timing = ComputeTiming(steps, motor.speedHz, motor.acceleration);
  plan.elapsedUs = 0;

  auto &slot = commandSlots_[channel][activeSlot_[channel]];
  slot = CommandSlot{};

  if (steps == 0 || plan.timing.totalDurationUs == 0)
  {
    motor.position = plan.targetPosition;
    motor.targetPosition = plan.targetPosition;
    plan.active = false;
    if (plan.homingStep < 2)
    {
      ++plan.homingStep;
      configureHomingStage(channel, plan);
    }
    return;
  }

  double clampedSpeed = static_cast<double>(std::max<int32_t>(1, motor.speedHz));
  double stepPeriodUs = static_cast<double>(kMicrosPerSecond) / clampedSpeed;
  uint32_t periodUs = static_cast<uint32_t>(std::llround(std::max(1.0, stepPeriodUs)));

  slot.occupied = true;
  slot.timing = plan.timing;
  slot.stepCount = steps;
  slot.halfPeriodMicros = std::max<uint32_t>(1U, periodUs / 2U);
  slot.directionHigh = (plan.targetPosition >= plan.startPosition);

  plan.active = true;
  motor.targetPosition = plan.targetPosition;
  motor.plannedDurationUs = plan.timing.totalDurationUs;
}

void MotorManager::forceSleep(std::size_t channel)
{
  if (channel >= kMotorCount)
  {
    return;
  }

  motors_[channel].phase = MotionPhase::Idle;
  motors_[channel].asleep = true;
  motors_[channel].plannedDurationUs = 0;
  plans_[channel] = ActivePlan{};
  commandSlots_[channel][0] = CommandSlot{};
  commandSlots_[channel][1] = CommandSlot{};
  activeSlot_[channel] = 0;
  updateAutosleep(channel);
}

void MotorManager::forceWake(std::size_t channel)
{
  if (channel >= kMotorCount)
  {
    return;
  }
  motors_[channel].asleep = false;
  updateAutosleep(channel);
}

void MotorManager::injectFault(std::size_t channel, FaultCode fault)
{
  if (channel >= kMotorCount)
  {
    return;
  }

  motors_[channel].fault = fault;
  motors_[channel].phase = MotionPhase::Idle;
  motors_[channel].plannedDurationUs = 0;
  motors_[channel].asleep = true;
  plans_[channel] = ActivePlan{};
  commandSlots_[channel][0] = CommandSlot{};
  commandSlots_[channel][1] = CommandSlot{};
  activeSlot_[channel] = 0;
  updateAutosleep(channel);
}

void MotorManager::clearFault(std::size_t channel)
{
  if (channel >= kMotorCount)
  {
    return;
  }
  motors_[channel].fault = FaultCode::None;
}

const MotorState &MotorManager::state(std::size_t channel) const
{
  return motors_[channel];
}

TimingEstimate MotorManager::ComputeTiming(uint32_t steps, int32_t speedHz, int32_t acceleration)
{
  TimingEstimate timing{};
  timing.totalSteps = steps;
  if (steps == 0 || speedHz <= 0 || acceleration <= 0)
  {
    timing.totalDurationUs = 0;
    return timing;
  }

  double v = static_cast<double>(speedHz);
  double a = static_cast<double>(acceleration);

  double rampSteps = 0.5 * (v * v) / a;
  if (static_cast<double>(steps) >= (2.0 * rampSteps))
  {
    double cruiseSteps = static_cast<double>(steps) - (2.0 * rampSteps);
    double tAccel = v / a;
    double tCruise = cruiseSteps / v;
    double totalSeconds = (2.0 * tAccel) + tCruise;

    timing.accelSteps = static_cast<uint32_t>(std::llround(rampSteps));
    timing.cruiseSteps = static_cast<uint32_t>(std::llround(cruiseSteps));
    timing.totalDurationUs = static_cast<uint32_t>(std::llround(totalSeconds * static_cast<double>(kMicrosPerSecond)));
  }
  else
  {
    double peakVelocity = std::sqrt(static_cast<double>(steps) * a);
    double tAccel = peakVelocity / a;
    double totalSeconds = 2.0 * tAccel;

    timing.accelSteps = steps / 2U;
    timing.cruiseSteps = 0;
    timing.totalDurationUs = static_cast<uint32_t>(std::llround(totalSeconds * static_cast<double>(kMicrosPerSecond)));
  }
  return timing;
}

void MotorManager::markCommandExecuted(std::size_t channel)
{
  if (channel >= kMotorCount)
  {
    return;
  }
  commandSlots_[channel][activeSlot_[channel]] = CommandSlot{};
}

void MotorManager::configureShiftRegister(const ShiftRegisterPins &pins, bool activeHigh)
{
  shiftActiveHigh_ = activeHigh;
  sleepRegister_.configure(pins, activeHigh);
  for (std::size_t i = 0; i < kMotorCount; ++i)
  {
    sleepRegister_.setChannel(i, motors_[i].asleep);
  }
  sleepRegister_.apply();
}

void MotorManager::updateAutosleep(std::size_t channel)
{
  sleepRegister_.setChannel(channel, motors_[channel].asleep);
  sleepRegister_.apply();
}

void MotorManager::exportCommandBuffer(std::size_t channel, pio::CommandBuffer &out) const
{
  if (channel >= kMotorCount)
  {
    return;
  }
  for (std::size_t index = 0; index < 2; ++index)
  {
    const auto &source = commandSlots_[channel][index];
    out.slots[index].stepCount = source.stepCount;
    out.slots[index].delayTicks = source.halfPeriodMicros;
    out.slots[index].directionHigh = source.directionHigh;
    out.occupied[index] = source.occupied;
  }
}

void MotorManager::SleepRegister::configure(const ShiftRegisterPins &pins, bool activeHigh)
{
  pins_ = pins;
  activeHigh_ = activeHigh;
  configured_ = (pins.data != 0 || pins.clock != 0 || pins.latch != 0);

#if defined(ARDUINO)
  if (configured_)
  {
    pinMode(pins_.data, OUTPUT);
    pinMode(pins_.clock, OUTPUT);
    pinMode(pins_.latch, OUTPUT);
  }
#endif
  states_.fill(true);
}

void MotorManager::SleepRegister::setChannel(std::size_t channel, bool asleep)
{
  if (channel >= MotorManager::kMotorCount)
  {
    return;
  }
  states_[channel] = asleep;
}

void MotorManager::SleepRegister::apply()
{
  if (!configured_)
  {
    return;
  }

#if defined(ARDUINO)
  uint8_t pattern = 0;
  for (std::size_t channel = 0; channel < MotorManager::kMotorCount; ++channel)
  {
    bool asleep = states_[channel];
    bool output = activeHigh_ ? asleep : !asleep;
    if (output)
    {
      pattern |= static_cast<uint8_t>(1U << channel);
    }
  }

  digitalWrite(pins_.latch, LOW);
  shiftOut(pins_.data, pins_.clock, MSBFIRST, pattern);
  digitalWrite(pins_.latch, HIGH);
#endif
}

} // namespace motion
