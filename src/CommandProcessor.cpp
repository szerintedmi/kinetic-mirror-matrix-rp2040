#include "control/CommandProcessor.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <limits>
#include <string_view>

namespace
{

  using ctrl::CommandProcessor;
  using motion::MotionPhase;

  constexpr std::string_view kWhitespace(" \t\r\n");

  std::string_view Trim(std::string_view value)
  {
    while (!value.empty() && kWhitespace.find(value.front()) != std::string_view::npos)
    {
      value.remove_prefix(1);
    }
    while (!value.empty() && kWhitespace.find(value.back()) != std::string_view::npos)
    {
      value.remove_suffix(1);
    }
    return value;
  }

  const char *MotionStateLabel(MotionPhase state)
  {
    switch (state)
    {
    case MotionPhase::Idle:
      return "IDLE";
    case MotionPhase::Moving:
      return "MOVING";
    case MotionPhase::Homing:
      return "HOMING";
    }
    return "UNKNOWN";
  }

  const char *ResponseCodeLabel(CommandProcessor::ResponseCode code)
  {
    switch (code)
    {
    case CommandProcessor::ResponseCode::Ok:
      return "OK";
    case CommandProcessor::ResponseCode::UnknownVerb:
      return "ERR_UNKNOWN_VERB";
    case CommandProcessor::ResponseCode::PayloadTooLong:
      return "ERR_PAYLOAD_TOO_LONG";
    case CommandProcessor::ResponseCode::EmptyCommand:
      return "ERR_EMPTY";
    case CommandProcessor::ResponseCode::VerbTooLong:
      return "ERR_VERB_TOO_LONG";
    case CommandProcessor::ResponseCode::MissingPayload:
      return "ERR_MISSING_PAYLOAD";
    case CommandProcessor::ResponseCode::InvalidChannel:
      return "ERR_INVALID_CHANNEL";
    case CommandProcessor::ResponseCode::ParseError:
      return "ERR_PARSE";
    case CommandProcessor::ResponseCode::InvalidArgument:
      return "ERR_INVALID_ARGUMENT";
    case CommandProcessor::ResponseCode::NotReady:
      return "ERR_NOT_READY";
    case CommandProcessor::ResponseCode::LimitViolation:
      return "ERR_LIMIT";
    case CommandProcessor::ResponseCode::Busy:
      return "ERR_BUSY";
    case CommandProcessor::ResponseCode::DriverFault:
      return "ERR_DRIVER_FAULT";
    }
    return "ERR_UNKNOWN";
  }

  struct CommandHelp
  {
    const char *verb;
    const char *usage;
    const char *description;
  };

  constexpr CommandHelp kCommandHelp[] = {
      {"HELP", "HELP", "List supported verbs and payload formats."},
      {"MOVE", "MOVE:<channel>,<position>[,<speed>[,<accel>]]", "Queue an absolute move with optional speed/accel overrides."},
      {"HOME", "HOME:<channel>[,<travel>[,<backoff>]]", "Initiate the homing routine with optional travel/backoff overrides."},
      {"STATUS", "STATUS[:<channel>]", "Report state, position, and last error for one or all motors."},
      {"SLEEP", "SLEEP:<channel>", "Force a motor channel into low-power sleep."},
      {"WAKE", "WAKE:<channel>", "Wake a motor channel before additional commands."}};

} // namespace

namespace ctrl
{

  CommandProcessor::CommandProcessor()
  {
    reset();
  }

void CommandProcessor::reset()
{
  motorManager_.reset();
  lastResponseCodes_.fill(ResponseCode::Ok);
}

  void CommandProcessor::processLine(std::string_view rawLine, Response &out)
  {
    out.count = 0;

    std::string_view line = Trim(rawLine);
    if (line.empty())
    {
      writeResponsePrefix(out, ResponseCode::EmptyCommand);
      return;
    }

    if (line.size() > kMaxCommandLength)
    {
      writeResponsePrefix(out, ResponseCode::PayloadTooLong);
      return;
    }

    std::size_t colonIndex = line.find(':');
    std::string_view verbView = (colonIndex == std::string_view::npos) ? line : line.substr(0, colonIndex);
    std::string_view payload = (colonIndex == std::string_view::npos) ? std::string_view{} : line.substr(colonIndex + 1);

    verbView = Trim(verbView);
    payload = Trim(payload);

    if (verbView.empty())
    {
      writeResponsePrefix(out, ResponseCode::UnknownVerb);
      return;
    }

  if (verbView.size() > kMaxVerbLength)
  {
    if (colonIndex == std::string_view::npos)
    {
      // Ignore chatter that doesn't follow <VERB>[:payload] framing.
      return;
    }
    writeResponsePrefix(out, ResponseCode::VerbTooLong);
    return;
  }

    char verbBuffer[kMaxVerbLength + 1];
    std::size_t verbLength = verbView.size();
    for (std::size_t i = 0; i < verbLength; ++i)
    {
      verbBuffer[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(verbView[i])));
    }
    verbBuffer[verbLength] = '\0';

    if (std::string_view(verbBuffer) == "HELP")
    {
      handleHelp(out);
      return;
    }

    if (std::string_view(verbBuffer) == "MOVE")
    {
      handleMove(payload, out);
      return;
    }

    if (std::string_view(verbBuffer) == "SLEEP")
    {
      handleSleep(payload, out);
      return;
    }

    if (std::string_view(verbBuffer) == "WAKE")
    {
      handleWake(payload, out);
      return;
    }

    if (std::string_view(verbBuffer) == "STATUS")
    {
      handleStatus(payload, out);
      return;
    }

    if (std::string_view(verbBuffer) == "HOME")
    {
      handleHome(payload, out);
      return;
    }

    writeResponsePrefix(out, ResponseCode::UnknownVerb);
  }

void CommandProcessor::service(uint32_t elapsedMicros)
{
  motorManager_.service(elapsedMicros);
}

void CommandProcessor::configureShiftRegister(const motion::ShiftRegisterPins &pins)
{
  motorManager_.configureShiftRegister(pins);
}

void CommandProcessor::writeResponsePrefix(Response &out, ResponseCode code)
{
  if (out.count >= kMaxResponseLines)
  {
    return;
  }
  std::snprintf(out.lines[out.count].data(), kMaxResponseLineLength, "CTRL:%s", ResponseCodeLabel(code));
  ++out.count;
}

void CommandProcessor::appendLine(Response &out, std::string_view text)
{
  if (out.count >= kMaxResponseLines)
  {
    return;
  }
  std::size_t copyLength = std::min(text.size(), kMaxResponseLineLength - 1);
  auto &buffer = out.lines[out.count];
  buffer.fill('\0');
  for (std::size_t i = 0; i < copyLength; ++i)
  {
    buffer[i] = text[i];
  }
  buffer[copyLength] = '\0';
  ++out.count;
}

void CommandProcessor::appendFormatted(Response &out, const char *format, ...)
{
  if (out.count >= kMaxResponseLines)
  {
    return;
  }
  auto &buffer = out.lines[out.count];
  buffer.fill('\0');

  va_list args;
  va_start(args, format);
  std::vsnprintf(buffer.data(), kMaxResponseLineLength, format, args);
  va_end(args);

  ++out.count;
}

bool CommandProcessor::tokenize(std::string_view payload, std::array<std::string_view, kMaxTokens> &tokens, std::size_t &tokenCount)
{
  tokenCount = 0;
  std::string_view working = Trim(payload);
  if (working.empty())
  {
    return true;
  }

  std::size_t start = 0;
  while (start <= working.size() && tokenCount < kMaxTokens)
  {
    std::size_t comma = working.find(',', start);
    std::size_t end = (comma == std::string_view::npos) ? working.size() : comma;
    std::string_view token = working.substr(start, end - start);
    tokens[tokenCount++] = Trim(token);

    if (comma == std::string_view::npos)
    {
      break;
    }
    start = comma + 1;
  }

  if (tokenCount == kMaxTokens && working.find(',', start) != std::string_view::npos)
  {
    return false;
  }

  return true;
}

  bool CommandProcessor::parseInt(std::string_view token, long &value)
  {
    if (token.empty())
    {
      return false;
    }

    long sign = 1;
    std::size_t index = 0;
    if (token[index] == '+' || token[index] == '-')
    {
      sign = (token[index] == '-') ? -1 : 1;
      ++index;
    }

    if (index >= token.size())
    {
      return false;
    }

    long result = 0;
    for (; index < token.size(); ++index)
    {
      char ch = token[index];
      if (ch < '0' || ch > '9')
      {
        return false;
      }
      result = (result * 10) + (ch - '0');
    }

    value = result * sign;
    return true;
  }

  bool CommandProcessor::parseInt32(std::string_view token, int32_t &value)
  {
    long tmp = 0;
    if (!parseInt(token, tmp))
    {
      return false;
    }
    if (tmp < std::numeric_limits<int32_t>::min() || tmp > std::numeric_limits<int32_t>::max())
    {
      return false;
    }
    value = static_cast<int32_t>(tmp);
    return true;
  }

  bool CommandProcessor::parseOptionalLong(std::string_view token, long &value)
  {
    if (token.empty())
    {
      return true;
    }
    long parsed = 0;
    if (!parseInt(token, parsed))
    {
      return false;
    }
    value = parsed;
    return true;
  }

  void CommandProcessor::handleHelp(Response &out)
  {
    writeResponsePrefix(out, ResponseCode::Ok);
    for (const auto &entry : kCommandHelp)
    {
      appendFormatted(out, "HELP:%s|%s|%s", entry.verb, entry.usage, entry.description);
    }
  }

  void CommandProcessor::handleMove(std::string_view payload, Response &out)
  {
    if (payload.empty())
    {
      writeResponsePrefix(out, ResponseCode::MissingPayload);
      return;
    }

    std::array<std::string_view, kMaxTokens> tokens{};
    std::size_t tokenCount = 0;
    if (!tokenize(payload, tokens, tokenCount) || tokenCount < 2)
    {
      writeResponsePrefix(out, ResponseCode::ParseError);
      return;
    }

    std::size_t channel = 0;
    if (!parseChannel(tokens[0], channel))
    {
      writeResponsePrefix(out, ResponseCode::InvalidChannel);
      return;
    }

    long position = 0;
    if (!parseInt(tokens[1], position))
    {
      writeResponsePrefix(out, ResponseCode::InvalidArgument);
      return;
    }

    int32_t speed = kDefaultSpeedHz;
    int32_t accel = kDefaultAcceleration;

    if (tokenCount >= 3 && !tokens[2].empty())
    {
      if (!parseInt32(tokens[2], speed) || speed <= 0)
      {
        writeResponsePrefix(out, ResponseCode::InvalidArgument);
        return;
      }
    }

    if (tokenCount >= 4 && !tokens[3].empty())
    {
      if (!parseInt32(tokens[3], accel) || accel <= 0)
      {
        writeResponsePrefix(out, ResponseCode::InvalidArgument);
        return;
      }
    }

    motion::TimingEstimate timing{};
    motion::MoveResult result = motorManager_.queueMove(channel, position, speed, accel, timing);

    if (result == motion::MoveResult::Busy)
    {
      writeResponsePrefix(out, ResponseCode::Busy);
      appendLine(out, "MOVE:ERR=BUSY");
      recordResponse(channel, ResponseCode::Busy);
      return;
    }

    if (result == motion::MoveResult::Fault)
    {
      writeResponsePrefix(out, ResponseCode::DriverFault);
      appendLine(out, "MOVE:ERR=DRIVER_FAULT");
      recordResponse(channel, ResponseCode::DriverFault);
      return;
    }

    const auto &state = motorManager_.state(channel);
    writeResponsePrefix(out, ResponseCode::Ok);
    recordResponse(channel, (result == motion::MoveResult::ClippedToLimit) ? ResponseCode::LimitViolation : ResponseCode::Ok);

    appendFormatted(out, "MOVE:CH=%u POS=%ld TARGET=%ld STATE=%s",
                    static_cast<unsigned>(channel),
                    state.position,
                    state.targetPosition,
                    MotionStateLabel(state.phase));
    appendFormatted(out, "MOVE:SPEED=%ld ACC=%ld PLAN_US=%lu STEPS=%lu",
                    static_cast<long>(state.speedHz),
                    static_cast<long>(state.acceleration),
                    static_cast<unsigned long>(timing.totalDurationUs),
                    static_cast<unsigned long>(timing.totalSteps));

    if (result == motion::MoveResult::ClippedToLimit)
    {
      appendLine(out, "MOVE:LIMIT_CLIPPED=1");
    }
  }

  void CommandProcessor::handleSleep(std::string_view payload, Response &out)
  {
    if (payload.empty())
    {
      writeResponsePrefix(out, ResponseCode::MissingPayload);
      return;
    }

    std::size_t channel = 0;
    if (!parseChannel(payload, channel))
    {
      writeResponsePrefix(out, ResponseCode::InvalidChannel);
      return;
    }

    motorManager_.forceSleep(channel);
    recordResponse(channel, ResponseCode::Ok);

    writeResponsePrefix(out, ResponseCode::Ok);
    appendFormatted(out, "SLEEP:CH=%u STATE=SLEEP", static_cast<unsigned>(channel));
  }

  void CommandProcessor::handleWake(std::string_view payload, Response &out)
  {
    if (payload.empty())
    {
      writeResponsePrefix(out, ResponseCode::MissingPayload);
      return;
    }

    std::size_t channel = 0;
    if (!parseChannel(payload, channel))
    {
      writeResponsePrefix(out, ResponseCode::InvalidChannel);
      return;
    }

    motorManager_.forceWake(channel);
    motorManager_.clearFault(channel);
    recordResponse(channel, ResponseCode::Ok);

    writeResponsePrefix(out, ResponseCode::Ok);
    appendFormatted(out, "WAKE:CH=%u STATE=AWAKE", static_cast<unsigned>(channel));
  }

  void CommandProcessor::handleStatus(std::string_view payload, Response &out)
  {
    if (payload.empty())
    {
      writeResponsePrefix(out, ResponseCode::Ok);
      for (std::size_t channel = 0; channel < kMotorCount; ++channel)
      {
        writeStatusForMotor(channel, out);
      }
      return;
    }

    std::array<std::string_view, kMaxTokens> tokens{};
    std::size_t tokenCount = 0;
    if (!tokenize(payload, tokens, tokenCount) || tokenCount == 0 || tokenCount > 1)
    {
      writeResponsePrefix(out, ResponseCode::ParseError);
      return;
    }

    std::size_t channel = 0;
    if (!parseChannel(tokens[0], channel))
    {
      writeResponsePrefix(out, ResponseCode::InvalidChannel);
      return;
    }

    writeResponsePrefix(out, ResponseCode::Ok);
    writeStatusForMotor(channel, out);
  }

  void CommandProcessor::handleHome(std::string_view payload, Response &out)
  {
    if (payload.empty())
    {
      writeResponsePrefix(out, ResponseCode::MissingPayload);
      return;
    }

    std::array<std::string_view, kMaxTokens> tokens{};
    std::size_t tokenCount = 0;
    if (!tokenize(payload, tokens, tokenCount) || tokenCount < 1 || tokenCount > 3)
    {
      writeResponsePrefix(out, ResponseCode::ParseError);
      return;
    }

    std::size_t channel = 0;
    if (!parseChannel(tokens[0], channel))
    {
      writeResponsePrefix(out, ResponseCode::InvalidChannel);
      return;
    }

    motion::HomingRequest request{};
    request.travelRange = motion::MotorManager::kDefaultTravelRange;
    request.backoff = motion::MotorManager::kDefaultBackoff;

    if (tokenCount >= 2 && !tokens[1].empty())
    {
      long travel = request.travelRange;
      if (!parseOptionalLong(tokens[1], travel) || travel <= 0)
      {
        writeResponsePrefix(out, ResponseCode::InvalidArgument);
        return;
      }
      request.travelRange = travel;
    }

    if (tokenCount == 3 && !tokens[2].empty())
    {
      long backoff = request.backoff;
      if (!parseOptionalLong(tokens[2], backoff) || backoff < 0)
      {
        writeResponsePrefix(out, ResponseCode::InvalidArgument);
        return;
      }
      request.backoff = backoff;
    }

    motion::MoveResult result = motorManager_.beginHoming(channel, request);
    if (result == motion::MoveResult::Busy)
    {
      writeResponsePrefix(out, ResponseCode::Busy);
      appendLine(out, "HOME:ERR=BUSY");
      recordResponse(channel, ResponseCode::Busy);
      return;
    }

    if (result == motion::MoveResult::Fault)
    {
      writeResponsePrefix(out, ResponseCode::DriverFault);
      appendLine(out, "HOME:ERR=DRIVER_FAULT");
      recordResponse(channel, ResponseCode::DriverFault);
      return;
    }

    recordResponse(channel, ResponseCode::Ok);
    writeResponsePrefix(out, ResponseCode::Ok);
    appendFormatted(out, "HOME:CH=%u RANGE=%ld BACKOFF=%ld",
                    static_cast<unsigned>(channel),
                    request.travelRange,
                    request.backoff);
  }

  bool CommandProcessor::parseChannel(std::string_view token, std::size_t &channel)
  {
    long parsed = 0;
    if (!parseInt(token, parsed))
    {
      return false;
    }
    if (parsed < 0 || parsed >= static_cast<long>(kMotorCount))
    {
      return false;
    }
    channel = static_cast<std::size_t>(parsed);
    return true;
  }

  CommandProcessor::ResponseCode CommandProcessor::mapFault(motion::FaultCode fault) const
  {
    switch (fault)
    {
    case motion::FaultCode::None:
      return ResponseCode::Ok;
    case motion::FaultCode::LimitClipped:
      return ResponseCode::LimitViolation;
    case motion::FaultCode::DriverFault:
      return ResponseCode::DriverFault;
    case motion::FaultCode::HomingTimeout:
      return ResponseCode::NotReady;
    }
    return ResponseCode::InvalidArgument;
  }

  void CommandProcessor::recordResponse(std::size_t channel, ResponseCode code)
  {
    if (channel >= kMotorCount)
    {
      return;
    }
    lastResponseCodes_[channel] = code;
  }

  void CommandProcessor::writeStatusForMotor(std::size_t channel, Response &out)
  {
    const auto &state = motorManager_.state(channel);
    ResponseCode code = lastResponseCodes_[channel];
    if (state.fault != motion::FaultCode::None)
    {
      code = mapFault(state.fault);
    }
    appendFormatted(out, "STATUS:CH=%u POS=%ld TARGET=%ld STATE=%s SLEEP=%u ERR=%s",
                    static_cast<unsigned>(channel),
                    state.position,
                    state.targetPosition,
                    MotionStateLabel(state.phase),
                    state.asleep ? 1U : 0U,
                    ResponseCodeLabel(code));
  appendFormatted(out, "STATUS:PROFILE CH=%u SPEED=%ld ACC=%ld PLAN_US=%lu",
                  static_cast<unsigned>(channel),
                  static_cast<long>(state.speedHz),
                  static_cast<long>(state.acceleration),
                  static_cast<unsigned long>(state.plannedDurationUs));
}
} // namespace ctrl
