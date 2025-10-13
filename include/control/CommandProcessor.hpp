#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "motion/MotorManager.hpp"

namespace ctrl
{

class CommandProcessor
{
public:
  static constexpr std::size_t kMotorCount = motion::MotorManager::kMotorCount;
  static constexpr std::size_t kMaxCommandLength = 80;
  static constexpr std::size_t kMaxVerbLength = 8;
  static constexpr std::size_t kMaxResponseLines = 10;
  static constexpr std::size_t kMaxResponseLineLength = 96;
  static constexpr int32_t kDefaultSpeedHz = motion::MotorManager::kDefaultSpeedHz;
  static constexpr int32_t kDefaultAcceleration = motion::MotorManager::kDefaultAcceleration;

  using MotionState = motion::MotionPhase;
  using MotorState = motion::MotorState;

  enum class ResponseCode : uint8_t
  {
    Ok = 0,
    UnknownVerb,
    PayloadTooLong,
    EmptyCommand,
    VerbTooLong,
    MissingPayload,
    InvalidChannel,
    ParseError,
    InvalidArgument,
    NotReady,
    LimitViolation,
    Busy,
    DriverFault
  };

  struct Response
  {
    std::array<std::array<char, kMaxResponseLineLength>, kMaxResponseLines> lines{};
    std::size_t count = 0;
  };

  CommandProcessor();

  void reset();

  void processLine(std::string_view rawLine, Response &out);

  const MotorState &motorState(std::size_t index) const { return motorManager_.state(index); }
  ResponseCode lastResponse(std::size_t index) const { return lastResponseCodes_[index]; }

private:
  static constexpr std::size_t kMaxTokens = 4;

  void writeResponsePrefix(Response &out, ResponseCode code);
  void appendLine(Response &out, std::string_view text);
  void appendFormatted(Response &out, const char *format, ...);

  bool tokenize(std::string_view payload, std::array<std::string_view, kMaxTokens> &tokens, std::size_t &tokenCount);
  bool parseInt(std::string_view token, long &value);
  bool parseInt32(std::string_view token, int32_t &value);
  bool parseOptionalLong(std::string_view token, long &value);

  void handleHelp(Response &out);
  void handleMove(std::string_view payload, Response &out);
  void handleSleep(std::string_view payload, Response &out);
  void handleWake(std::string_view payload, Response &out);
  void handleStatus(std::string_view payload, Response &out);
  void handleHome(std::string_view payload, Response &out);

  bool parseChannel(std::string_view token, std::size_t &channel);
  ResponseCode mapFault(motion::FaultCode fault) const;
  void recordResponse(std::size_t channel, ResponseCode code);
  void writeStatusForMotor(std::size_t channel, Response &out);

  motion::MotorManager motorManager_{};
  std::array<ResponseCode, kMotorCount> lastResponseCodes_{};
};

} // namespace ctrl

