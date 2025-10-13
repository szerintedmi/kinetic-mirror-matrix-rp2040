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

  TEST_ASSERT_EQUAL_UINT(2, response.count);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", response.lines[0].data());
  TEST_ASSERT_EQUAL_STRING("MOVE:CH=1 POS=120 SPEED=5000 ACC=20000", response.lines[1].data());

  const auto &state = processor.motorState(1);
  TEST_ASSERT_EQUAL_INT32(120, static_cast<int32_t>(state.position));
  TEST_ASSERT_EQUAL_INT32(120, static_cast<int32_t>(state.targetPosition));
  TEST_ASSERT_EQUAL_INT32(5000, state.speedHz);
  TEST_ASSERT_EQUAL_INT32(20000, state.acceleration);
  TEST_ASSERT_FALSE(state.asleep);
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
  TEST_ASSERT_EQUAL_STRING("STATUS:CH=0 POS=222 TARGET=222 STATE=IDLE SLEEP=0 ERR=OK SPEED=4000 ACC=16000",
                           GetLine(response, 1).data());

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
