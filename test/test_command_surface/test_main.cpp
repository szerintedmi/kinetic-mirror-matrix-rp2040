#include <string_view>

#include <unity.h>

#include "control/CommandProcessor.hpp"

namespace
{

  ctrl::CommandProcessor processor;

  std::string_view GetLine(const ctrl::CommandProcessor::Response &response, std::size_t index)
  {
    if (index >= response.count)
    {
      return std::string_view{};
    }
    return std::string_view(response.lines[index].data());
  }

} // namespace

void setUp()
{
  processor.reset();
}

void tearDown() {}

void test_help_lists_known_verbs()
{
  ctrl::CommandProcessor::Response response{};
  processor.processLine("help", response);

  TEST_ASSERT_GREATER_THAN_UINT32(0, response.count);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", response.lines[0].data());

  bool hasMove = false;
  bool hasStatus = false;
  for (std::size_t i = 1; i < response.count; ++i)
  {
    std::string_view line(response.lines[i].data());
    if (line.find("HELP:MOVE") != std::string_view::npos)
    {
      hasMove = true;
    }
    if (line.find("HELP:STATUS") != std::string_view::npos)
    {
      hasStatus = true;
    }
  }

  TEST_ASSERT_TRUE_MESSAGE(hasMove, "HELP output should reference MOVE verb");
  TEST_ASSERT_TRUE_MESSAGE(hasStatus, "HELP output should reference STATUS verb");
}

void test_move_applies_speed_and_accel_overrides()
{
  ctrl::CommandProcessor::Response response{};
  processor.processLine("MOVE:1,120,5000,20000", response);

  TEST_ASSERT_EQUAL_UINT(3, response.count);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", response.lines[0].data());
  std::string_view detail(GetLine(response, 1));
  TEST_ASSERT_NOT_EQUAL(std::string_view::npos, detail.find("MOVE:CH=1"));
  TEST_ASSERT_NOT_EQUAL(std::string_view::npos, detail.find("POS=0"));
  TEST_ASSERT_NOT_EQUAL(std::string_view::npos, detail.find("TARGET=120"));
  TEST_ASSERT_NOT_EQUAL(std::string_view::npos, detail.find("STATE=MOVING"));
  std::string_view timing(GetLine(response, 2));
  TEST_ASSERT_NOT_EQUAL(std::string_view::npos, timing.find("SPEED=5000"));
  TEST_ASSERT_NOT_EQUAL(std::string_view::npos, timing.find("ACC=20000"));
  TEST_ASSERT_NOT_EQUAL(std::string_view::npos, timing.find("PLAN_US="));

  const auto &state = processor.motorState(1);
  TEST_ASSERT_EQUAL_INT32(0, static_cast<int32_t>(state.position));
  TEST_ASSERT_EQUAL_INT32(120, static_cast<int32_t>(state.targetPosition));
  TEST_ASSERT_EQUAL_INT32(5000, state.speedHz);
  TEST_ASSERT_EQUAL_INT32(20000, state.acceleration);
  TEST_ASSERT_FALSE(state.asleep);
  TEST_ASSERT_EQUAL(motion::MotionPhase::Moving, state.phase);
  TEST_ASSERT_GREATER_THAN_UINT32(0, state.plannedDurationUs);
}

void test_sleep_wake_toggle_persists_state()
{
  ctrl::CommandProcessor::Response response{};

  processor.processLine("WAKE:2", response);
  TEST_ASSERT_EQUAL_UINT(2, response.count);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", GetLine(response, 0).data());
  TEST_ASSERT_EQUAL_STRING("WAKE:CH=2 STATE=AWAKE", GetLine(response, 1).data());
  TEST_ASSERT_FALSE(processor.motorState(2).asleep);

  response.count = 0;
  processor.processLine("SLEEP:2", response);
  TEST_ASSERT_EQUAL_UINT(2, response.count);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", GetLine(response, 0).data());
  TEST_ASSERT_EQUAL_STRING("SLEEP:CH=2 STATE=SLEEP", GetLine(response, 1).data());
  TEST_ASSERT_TRUE(processor.motorState(2).asleep);

  response.count = 0;
  processor.processLine("SLEEP:2", response);
  TEST_ASSERT_EQUAL_UINT(2, response.count);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", GetLine(response, 0).data());
  TEST_ASSERT_EQUAL_STRING("SLEEP:CH=2 STATE=SLEEP", GetLine(response, 1).data());
  TEST_ASSERT_TRUE(processor.motorState(2).asleep);
}

void test_status_reports_structured_channel_data()
{
  ctrl::CommandProcessor::Response response{};
  processor.processLine("MOVE:0,222", response);

  response.count = 0;
  processor.processLine("STATUS:0", response);

  TEST_ASSERT_EQUAL_STRING("CTRL:OK", GetLine(response, 0).data());
  std::string_view status(GetLine(response, 1));
  TEST_ASSERT_NOT_EQUAL(std::string_view::npos, status.find("STATUS:CH=0"));
  TEST_ASSERT_NOT_EQUAL(std::string_view::npos, status.find("POS=0"));
  TEST_ASSERT_NOT_EQUAL(std::string_view::npos, status.find("TARGET=222"));
  TEST_ASSERT_NOT_EQUAL(std::string_view::npos, status.find("STATE=MOVING"));
  TEST_ASSERT_NOT_EQUAL(std::string_view::npos, status.find("SLEEP=0"));
  TEST_ASSERT_NOT_EQUAL(std::string_view::npos, status.find("ERR=OK"));
  TEST_ASSERT_GREATER_THAN_UINT32(0, processor.motorState(0).plannedDurationUs);

  std::string_view profile(GetLine(response, 2));
  TEST_ASSERT_NOT_EQUAL(std::string_view::npos, profile.find("STATUS:PROFILE"));
  TEST_ASSERT_NOT_EQUAL(std::string_view::npos, profile.find("SPEED=4000"));
  TEST_ASSERT_NOT_EQUAL(std::string_view::npos, profile.find("ACC=16000"));
  TEST_ASSERT_NOT_EQUAL(std::string_view::npos, profile.find("PLAN_US="));

  response.count = 0;
  processor.processLine("STATUS:9", response);
  TEST_ASSERT_EQUAL_STRING("CTRL:ERR_INVALID_CHANNEL", GetLine(response, 0).data());
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_help_lists_known_verbs);
  RUN_TEST(test_move_applies_speed_and_accel_overrides);
  RUN_TEST(test_sleep_wake_toggle_persists_state);
  RUN_TEST(test_status_reports_structured_channel_data);
  return UNITY_END();
}
