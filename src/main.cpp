#ifdef ARDUINO

#include <Arduino.h>
#include <array>
#include <cstddef>
#include <string_view>

#include "boards/Rp2040Pins.hpp"
#include "control/CommandProcessor.hpp"

namespace
{

ctrl::CommandProcessor gCommandProcessor;
std::array<char, ctrl::CommandProcessor::kMaxCommandLength + 1> gBuffer{};
std::size_t gBufferLength = 0;
bool gBufferOverflow = false;
uint32_t gLastServiceMicros = 0;

void emitResponse(const ctrl::CommandProcessor::Response &response)
{
  for (std::size_t i = 0; i < response.count; ++i)
  {
    Serial.println(response.lines[i].data());
  }
}

void flushCommand()
{
  if (gBufferOverflow)
  {
    gBufferOverflow = false;
    gBufferLength = 0;
    Serial.println("CTRL:ERR_PAYLOAD_TOO_LONG");
    return;
  }

  ctrl::CommandProcessor::Response response{};
  std::string_view commandView(gBuffer.data(), gBufferLength);
  gCommandProcessor.processLine(commandView, response);
  emitResponse(response);
  gBufferLength = 0;
}

} // namespace

void setup()
{
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && (millis() - start) < 2000)
  {
    delay(10);
  }
  gCommandProcessor.reset();
  gCommandProcessor.configureShiftRegister(board::rp2040::kShiftRegisterPins);
  gLastServiceMicros = micros();
  Serial.println("CTRL:READY");
}

void loop()
{
  const uint32_t now = micros();
  const uint32_t elapsed = now - gLastServiceMicros;
  gLastServiceMicros = now;
  if (elapsed > 0)
  {
    gCommandProcessor.service(elapsed);
  }

  while (Serial.available() > 0)
  {
    char incoming = static_cast<char>(Serial.read());
    if (incoming == '\r')
    {
      continue;
    }
    if (incoming == '\n')
    {
      flushCommand();
      continue;
    }

    if (gBufferLength >= ctrl::CommandProcessor::kMaxCommandLength)
    {
      gBufferOverflow = true;
      continue;
    }

    gBuffer[gBufferLength++] = incoming;
  }
}

#endif // ARDUINO

#if !defined(ARDUINO) && !defined(PLATFORMIO_UNIT_TESTING) && !defined(UNIT_TEST)
int main()
{
  return 0;
}
#endif
