#include <unity.h>

#include "motion/MotorManager.hpp"
#include "motion/StepperPioProgram.hpp"

namespace
{

motion::MotorManager manager;

void fastForwardChannel(std::size_t channel)
{
  const auto &state = manager.state(channel);
  if (state.plannedDurationUs > 0)
  {
    manager.service(state.plannedDurationUs + 10);
  }
}

} // namespace

void setUp()
{
  manager.reset();
}

void tearDown() {}

void test_move_clamps_to_limits()
{
  motion::TimingEstimate timing{};
  auto result = manager.queueMove(0, motion::MotorManager::kDefaultLimit + 500, 4000, 16000, timing);
  TEST_ASSERT_EQUAL(motion::MoveResult::ClippedToLimit, result);

  const auto &state = manager.state(0);
  TEST_ASSERT_EQUAL_INT32(motion::MotorManager::kDefaultLimit, static_cast<int32_t>(state.targetPosition));
  TEST_ASSERT_EQUAL(motion::FaultCode::LimitClipped, state.fault);
  TEST_ASSERT_TRUE(state.limitClipped);
  TEST_ASSERT_GREATER_THAN_UINT32(0, timing.totalSteps);
}

void test_homing_resets_zero_position()
{
  motion::HomingRequest request{};
  request.travelRange = motion::MotorManager::kDefaultTravelRange;
  request.backoff = motion::MotorManager::kDefaultBackoff;

  auto result = manager.beginHoming(1, request);
  TEST_ASSERT_EQUAL(motion::MoveResult::Scheduled, result);

  // First phase (approach)
  fastForwardChannel(1);
  // Second phase (backoff)
  fastForwardChannel(1);
  // Third phase (establish midpoint zero)
  fastForwardChannel(1);

  const auto &state = manager.state(1);
  TEST_ASSERT_EQUAL(motion::MotionPhase::Idle, state.phase);
  TEST_ASSERT_EQUAL_INT32(0, static_cast<int32_t>(state.position));
  TEST_ASSERT_TRUE(state.asleep);
  TEST_ASSERT_EQUAL_UINT32(0, state.plannedDurationUs);
  TEST_ASSERT_FALSE(state.limitClipped);
}

void test_homing_moves_relative_distance()
{
  motion::HomingRequest request{};
  request.travelRange = motion::MotorManager::kDefaultTravelRange;
  request.backoff = 100;

  auto result = manager.beginHoming(4, request);
  TEST_ASSERT_EQUAL(motion::MoveResult::Scheduled, result);

  motion::pio::CommandBuffer buffer{};
  manager.exportCommandBuffer(4, buffer);
  TEST_ASSERT_TRUE_MESSAGE(buffer.occupied[0] || buffer.occupied[1], "First homing stage should occupy a slot");

  uint32_t stage0Steps = buffer.occupied[0] ? buffer.slots[0].stepCount : buffer.slots[1].stepCount;
  TEST_ASSERT_EQUAL_UINT32(request.travelRange, stage0Steps);

  fastForwardChannel(4);
  buffer = motion::pio::CommandBuffer{};
  manager.exportCommandBuffer(4, buffer);
  TEST_ASSERT_TRUE_MESSAGE(buffer.occupied[0] || buffer.occupied[1], "Second homing stage should occupy a slot");
  uint32_t stage1Steps = buffer.occupied[0] ? buffer.slots[0].stepCount : buffer.slots[1].stepCount;
  TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(request.backoff), stage1Steps);

  fastForwardChannel(4);
  buffer = motion::pio::CommandBuffer{};
  manager.exportCommandBuffer(4, buffer);
  TEST_ASSERT_TRUE_MESSAGE(buffer.occupied[0] || buffer.occupied[1], "Third homing stage should occupy a slot");
  uint32_t expectedCenterSteps = static_cast<uint32_t>((request.travelRange / 2) - request.backoff);
  uint32_t stage2Steps = buffer.occupied[0] ? buffer.slots[0].stepCount : buffer.slots[1].stepCount;
  TEST_ASSERT_EQUAL_UINT32(expectedCenterSteps, stage2Steps);

  fastForwardChannel(4);
  const auto &state = manager.state(4);
  TEST_ASSERT_EQUAL(motion::MotionPhase::Idle, state.phase);
  TEST_ASSERT_EQUAL_INT32(0, static_cast<int32_t>(state.position));
  TEST_ASSERT_EQUAL_INT32(0, static_cast<int32_t>(state.targetPosition));
}

void test_autosleep_transitions_after_motion()
{
  motion::TimingEstimate timing{};
  auto result = manager.queueMove(2, 600, 3000, 12000, timing);
  TEST_ASSERT_EQUAL(motion::MoveResult::Scheduled, result);
  TEST_ASSERT_FALSE(manager.state(2).asleep);

  fastForwardChannel(2);

  const auto &state = manager.state(2);
  TEST_ASSERT_TRUE(state.asleep);
  TEST_ASSERT_EQUAL(motion::MotionPhase::Idle, state.phase);
}

void test_step_timing_calculation_matches_trapezoid_profile()
{
  auto timing = motion::MotorManager::ComputeTiming(2400, 4000, 16000);
  TEST_ASSERT_EQUAL_UINT32(2400, timing.totalSteps);
  uint32_t accelDelta = (timing.accelSteps > 500) ? (timing.accelSteps - 500) : (500 - timing.accelSteps);
  TEST_ASSERT_TRUE(accelDelta <= 5);
  TEST_ASSERT_TRUE(timing.cruiseSteps > 0);
  uint32_t durationDelta = (timing.totalDurationUs > 850000) ? (timing.totalDurationUs - 850000) : (850000 - timing.totalDurationUs);
  TEST_ASSERT_TRUE(durationDelta <= 2);
}

void test_fault_blocks_motion_until_cleared()
{
  manager.injectFault(3, motion::FaultCode::DriverFault);
  auto &state = manager.state(3);
  TEST_ASSERT_EQUAL(motion::FaultCode::DriverFault, state.fault);

  motion::TimingEstimate timing{};
  auto result = manager.queueMove(3, 200, 4000, 16000, timing);
  TEST_ASSERT_EQUAL(motion::MoveResult::Fault, result);

  manager.clearFault(3);
  result = manager.queueMove(3, 200, 4000, 16000, timing);
  TEST_ASSERT_EQUAL(motion::MoveResult::Scheduled, result);
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_move_clamps_to_limits);
  RUN_TEST(test_homing_resets_zero_position);
  RUN_TEST(test_homing_moves_relative_distance);
  RUN_TEST(test_autosleep_transitions_after_motion);
  RUN_TEST(test_step_timing_calculation_matches_trapezoid_profile);
  RUN_TEST(test_fault_blocks_motion_until_cleared);
  return UNITY_END();
}
