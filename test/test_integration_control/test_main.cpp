#include <cstdint>
#include <string_view>

#include <unity.h>

#include "control/CommandProcessor.hpp"
#include "motion/MotorManager.hpp"

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

void ProcessLine(const char *line, ctrl::CommandProcessor::Response &response)
{
  response.count = 0;
  processor.processLine(line, response);
}

bool Contains(std::string_view haystack, std::string_view needle)
{
  return haystack.find(needle) != std::string_view::npos;
}

} // namespace

void setUp()
{
  processor.reset();
}

void tearDown() {}

void test_help_flow_reports_all_required_verbs()
{
  ctrl::CommandProcessor::Response response{};
  ProcessLine("HELP", response);

  TEST_ASSERT_GREATER_THAN_UINT32(0, response.count);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", GetLine(response, 0).data());

  bool hasMove = false;
  bool hasHome = false;
  bool hasStatus = false;
  bool hasSleep = false;
  bool hasWake = false;

  for (std::size_t index = 1; index < response.count; ++index)
  {
    std::string_view line = GetLine(response, index);
    hasMove |= Contains(line, "HELP:MOVE");
    hasHome |= Contains(line, "HELP:HOME");
    hasStatus |= Contains(line, "HELP:STATUS");
    hasSleep |= Contains(line, "HELP:SLEEP");
    hasWake |= Contains(line, "HELP:WAKE");
  }

  TEST_ASSERT_TRUE_MESSAGE(hasMove, "HELP response missing MOVE entry");
  TEST_ASSERT_TRUE_MESSAGE(hasHome, "HELP response missing HOME entry");
  TEST_ASSERT_TRUE_MESSAGE(hasStatus, "HELP response missing STATUS entry");
  TEST_ASSERT_TRUE_MESSAGE(hasSleep, "HELP response missing SLEEP entry");
  TEST_ASSERT_TRUE_MESSAGE(hasWake, "HELP response missing WAKE entry");
}

void test_move_status_cycle_reaches_target()
{
  ctrl::CommandProcessor::Response response{};
  ProcessLine("MOVE:0,300", response);

  TEST_ASSERT_EQUAL_STRING("CTRL:OK", GetLine(response, 0).data());
  TEST_ASSERT_TRUE(Contains(GetLine(response, 1), "MOVE:CH=0"));
  TEST_ASSERT_TRUE(Contains(GetLine(response, 1), "TARGET=300"));

  auto duration = processor.motorState(0).plannedDurationUs;
  TEST_ASSERT_GREATER_THAN_UINT32(0, duration);

  processor.service(duration + 50);

  const auto &state = processor.motorState(0);
  TEST_ASSERT_EQUAL_INT32(300, static_cast<int32_t>(state.position));
  TEST_ASSERT_EQUAL(motion::MotionPhase::Idle, state.phase);
  TEST_ASSERT_TRUE(state.asleep);

  ProcessLine("STATUS:0", response);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", GetLine(response, 0).data());
  std::string_view status = GetLine(response, 1);
  TEST_ASSERT_TRUE(Contains(status, "STATUS:CH=0"));
  TEST_ASSERT_TRUE(Contains(status, "POS=300"));
  TEST_ASSERT_TRUE(Contains(status, "STATE=IDLE"));
  TEST_ASSERT_TRUE(Contains(status, "SLEEP=1"));
  TEST_ASSERT_TRUE(Contains(status, "ERR=OK"));
  std::string_view profile = GetLine(response, 2);
  TEST_ASSERT_TRUE(Contains(profile, "STATUS:PROFILE"));
  TEST_ASSERT_TRUE(Contains(profile, "SPEED=4000"));
  TEST_ASSERT_TRUE(Contains(profile, "ACC=16000"));
}

void test_sleep_wake_flow_reflected_in_status()
{
  ctrl::CommandProcessor::Response response{};

  ProcessLine("WAKE:3", response);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", GetLine(response, 0).data());
  TEST_ASSERT_EQUAL_STRING("WAKE:CH=3 STATE=AWAKE", GetLine(response, 1).data());
  TEST_ASSERT_FALSE(processor.motorState(3).asleep);

  ProcessLine("STATUS:3", response);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", GetLine(response, 0).data());
  TEST_ASSERT_TRUE(Contains(GetLine(response, 1), "SLEEP=0"));

  ProcessLine("SLEEP:3", response);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", GetLine(response, 0).data());
  TEST_ASSERT_EQUAL_STRING("SLEEP:CH=3 STATE=SLEEP", GetLine(response, 1).data());
  TEST_ASSERT_TRUE(processor.motorState(3).asleep);

  ProcessLine("STATUS:3", response);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", GetLine(response, 0).data());
  TEST_ASSERT_TRUE(Contains(GetLine(response, 1), "SLEEP=1"));
}

void test_home_sequence_completes_and_resets_origin()
{
  ctrl::CommandProcessor::Response response{};
  ProcessLine("HOME:1", response);

  TEST_ASSERT_EQUAL_STRING("CTRL:OK", GetLine(response, 0).data());
  TEST_ASSERT_TRUE(Contains(GetLine(response, 1), "HOME:CH=1"));

  for (int guard = 0; guard < 10; ++guard)
  {
    const auto &state = processor.motorState(1);
    if (state.phase == motion::MotionPhase::Idle && state.plannedDurationUs == 0)
    {
      break;
    }
    uint32_t elapsed = state.plannedDurationUs > 0 ? state.plannedDurationUs + 100 : 1000;
    processor.service(elapsed);
  }

  const auto &state = processor.motorState(1);
  TEST_ASSERT_EQUAL(motion::MotionPhase::Idle, state.phase);
  TEST_ASSERT_EQUAL_INT32(0, static_cast<int32_t>(state.position));
  TEST_ASSERT_TRUE(state.asleep);

  ProcessLine("STATUS:1", response);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", GetLine(response, 0).data());
  std::string_view status = GetLine(response, 1);
  TEST_ASSERT_TRUE(Contains(status, "POS=0"));
  TEST_ASSERT_TRUE(Contains(status, "STATE=IDLE"));
  TEST_ASSERT_TRUE(Contains(status, "ERR=OK"));
}

void test_move_beyond_limits_reports_clipping()
{
  ctrl::CommandProcessor::Response response{};
  ProcessLine("MOVE:4,2000", response);

  TEST_ASSERT_EQUAL_STRING("CTRL:OK", GetLine(response, 0).data());
  TEST_ASSERT_TRUE(Contains(GetLine(response, 1), "MOVE:CH=4"));
  TEST_ASSERT_TRUE(Contains(GetLine(response, 1), "TARGET=1200"));

  bool sawClipLine = false;
  for (std::size_t i = 0; i < response.count; ++i)
  {
    if (Contains(GetLine(response, i), "MOVE:LIMIT_CLIPPED=1"))
    {
      sawClipLine = true;
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(sawClipLine, "Expected limit clipped marker in response");

  auto duration = processor.motorState(4).plannedDurationUs;
  TEST_ASSERT_GREATER_THAN_UINT32(0, duration);
  processor.service(duration + 50);

  const auto &state = processor.motorState(4);
  TEST_ASSERT_EQUAL_INT32(motion::MotorManager::kDefaultLimit, static_cast<int32_t>(state.position));
  TEST_ASSERT_TRUE(state.limitClipped);
  TEST_ASSERT_EQUAL(ctrl::CommandProcessor::ResponseCode::LimitViolation, processor.lastResponse(4));
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_help_flow_reports_all_required_verbs);
  RUN_TEST(test_move_status_cycle_reaches_target);
  RUN_TEST(test_sleep_wake_flow_reflected_in_status);
  RUN_TEST(test_home_sequence_completes_and_resets_origin);
  RUN_TEST(test_move_beyond_limits_reports_clipping);
  return UNITY_END();
}
