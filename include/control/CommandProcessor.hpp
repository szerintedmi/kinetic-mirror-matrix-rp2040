#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ctrl
{

class CommandProcessor
{
public:
  static constexpr std::size_t kMotorCount = 8;
  static constexpr std::size_t kMaxCommandLength = 80;
  static constexpr std::size_t kMaxVerbLength = 8;
  static constexpr std::size_t kMaxResponseLines = 10;
  static constexpr std::size_t kMaxResponseLineLength = 96;
  static constexpr int32_t kDefaultSpeedHz = 4000;
  static constexpr int32_t kDefaultAcceleration = 16000;

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
    NotReady
  };

  enum class MotionState : uint8_t
  {
    Idle = 0,
    Moving,
    Homing
  };

  struct MotorState
  {
    long position = 0;
    long targetPosition = 0;
    int32_t speedHz = kDefaultSpeedHz;
    int32_t acceleration = kDefaultAcceleration;
    MotionState state = MotionState::Idle;
    bool asleep = true;
    ResponseCode lastError = ResponseCode::Ok;
  };

  struct Response
  {
    std::array<std::array<char, kMaxResponseLineLength>, kMaxResponseLines> lines{};
    std::size_t count = 0;
  };

  CommandProcessor();

  void reset();

  void processLine(std::string_view rawLine, Response &out);

  const MotorState &motorState(std::size_t index) const { return motors_[index]; }
  MotorState &motorState(std::size_t index) { return motors_[index]; }

private:
  static constexpr std::size_t kMaxTokens = 4;

  void writeResponsePrefix(Response &out, ResponseCode code);
  void appendLine(Response &out, std::string_view text);
  void appendFormatted(Response &out, const char *format, ...);

  bool tokenize(std::string_view payload, std::array<std::string_view, kMaxTokens> &tokens, std::size_t &tokenCount);
  bool parseInt(std::string_view token, long &value);
  bool parseInt32(std::string_view token, int32_t &value);

  void handleHelp(Response &out);
  void handleMove(std::string_view payload, Response &out);
  void handleSleep(std::string_view payload, Response &out);
  void handleWake(std::string_view payload, Response &out);
  void handleStatus(std::string_view payload, Response &out);
  void handleHome(Response &out);

  bool parseChannel(std::string_view token, std::size_t &channel);
  void writeStatusForMotor(std::size_t channel, Response &out);

  std::array<MotorState, kMotorCount> motors_{};
};

} // namespace ctrl
