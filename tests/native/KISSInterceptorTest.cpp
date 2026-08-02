#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "../../src/bb-link/KISSInterceptor.h"

static extended_hw_cmd_t extract(uint8_t *frame, size_t size)
{
  KISSInterceptor interceptor;
  extended_hw_cmd_t command = {};
  assert(interceptor.extractExtendedHardwareCommand(frame, size, &command));
  return command;
}

static void testAllCommandShapes()
{
  uint8_t restore[] = {0xC0, 0x06, 0xEB, 0xC0};
  uint8_t baud[] = {0xC0, 0x06, 0xF4, 0x01, 0xC0};
  uint8_t start[] = {0xC0, 0x06, 0xEC, 0xC0};
  uint8_t stop[] = {0xC0, 0x06, 0xED, 0xC0};
  uint8_t pair[] = {0xC0, 0x06, 0xEF, 1, 2, 3, 4, 5, 6, 0xC0};
  uint8_t clear[] = {0xC0, 0x06, 0xF0, 0xC0};
  uint8_t firmware[] = {0xC0, 0x06, 0x28, 0xC0};
  uint8_t capabilities[] = {0xC0, 0x06, 0x7E, 0xC0};
  uint8_t api[] = {0xC0, 0x06, 0x7B, 0xC0};
  uint8_t paired[] = {0xC0, 0x06, 0xF1, 0xC0};
  uint8_t rig[] = {0xC0, 0x06, 0xF2, 0x01, 0xC0};
  uint8_t reset[] = {0xC0, 0x06, 0xF3, 0xC0};

  assert(extract(restore, sizeof(restore)).action == extended_hw_restore_frequency);
  assert(extract(baud, sizeof(baud)).data.uint8 == 1);
  assert(extract(start, sizeof(start)).action == extended_hw_start_scan);
  assert(extract(stop, sizeof(stop)).action == extended_hw_stop_scan);
  extended_hw_cmd_t pairCommand = extract(pair, sizeof(pair));
  assert(pairCommand.action == extended_hw_pair_with_device);
  assert(memcmp(pairCommand.data.bytes, pair + 3, 6) == 0);
  assert(extract(clear, sizeof(clear)).action == extended_hw_clear_paired_device);
  assert(extract(firmware, sizeof(firmware)).action == extended_hw_firmware_version);
  assert(extract(capabilities, sizeof(capabilities)).action == extended_hw_capabilities);
  assert(extract(api, sizeof(api)).action == extended_hw_api_version);
  assert(extract(paired, sizeof(paired)).action == extended_hw_get_paired_device);
  assert(extract(rig, sizeof(rig)).data.uint8 == 1);
  assert(extract(reset, sizeof(reset)).action == extended_hw_factory_reset);
}

static void testExtractAtOffset()
{
  KISSInterceptor interceptor;
  uint8_t input[] = {0x55, 0xC0, 0x06, 0xEA, 0x01, 0x02, 0x03, 0x04, 0xC0, 0x66};
  extended_hw_cmd_t command = {};
  assert(interceptor.extractExtendedHardwareCommand(input, sizeof(input), &command));
  assert(command.action == extended_hw_set_frequency);
  assert(command.data.uint32 == 0x01020304UL);
}

static void testRejectsShortAndMalformedFrames()
{
  KISSInterceptor interceptor;
  extended_hw_cmd_t command = {};
  uint8_t delimiterOnly[] = {0xC0};
  uint8_t shortFrequency[] = {0xC0, 0x06, 0xEA, 0x01, 0xC0};
  uint8_t danglingEscape[] = {0xC0, 0x06, 0xEA, 0xDB, 0xC0};
  assert(!interceptor.extractExtendedHardwareCommand(delimiterOnly, sizeof(delimiterOnly), &command));
  assert(!interceptor.extractExtendedHardwareCommand(shortFrequency, sizeof(shortFrequency), &command));
  assert(!interceptor.extractExtendedHardwareCommand(danglingEscape, sizeof(danglingEscape), &command));
}

static void testMultipleCommandsAndPassthrough()
{
  KISSInterceptor interceptor;
  uint8_t input[] = {
    0xC0, 0x06, 0xEB, 0xC0,
    0xC0, 0x00, 0x11, 0x22, 0xC0,
    0xC0, 0x06, 0xEC, 0xC0
  };
  extended_hw_cmd_t commands[4] = {};
  uint8_t passthrough[64] = {};
  size_t commandCount = 0;
  size_t passthroughSize = 0;
  assert(interceptor.process(
    input, sizeof(input), commands, 4, &commandCount,
    passthrough, sizeof(passthrough), &passthroughSize) == kiss_process_ok);
  assert(commandCount == 2);
  assert(commands[0].action == extended_hw_restore_frequency);
  assert(commands[1].action == extended_hw_start_scan);
  const uint8_t expected[] = {0xC0, 0x00, 0x11, 0x22, 0xC0};
  assert(passthroughSize == sizeof(expected));
  assert(memcmp(passthrough, expected, sizeof(expected)) == 0);
}

static void testFragmentedCommand()
{
  KISSInterceptor interceptor;
  uint8_t first[] = {0xC0, 0x06, 0xEA, 0x01, 0x02};
  uint8_t second[] = {0x03, 0x04, 0xC0};
  extended_hw_cmd_t commands[1] = {};
  uint8_t passthrough[16] = {};
  size_t commandCount = 0;
  size_t passthroughSize = 0;

  assert(interceptor.process(
    first, sizeof(first), commands, 1, &commandCount,
    passthrough, sizeof(passthrough), &passthroughSize) == kiss_process_ok);
  assert(commandCount == 0 && passthroughSize == 0);

  assert(interceptor.process(
    second, sizeof(second), commands, 1, &commandCount,
    passthrough, sizeof(passthrough), &passthroughSize) == kiss_process_ok);
  assert(commandCount == 1);
  assert(commands[0].action == extended_hw_set_frequency);
  assert(commands[0].data.uint32 == 0x01020304UL);
  assert(passthroughSize == 0);
}

static void testEscapedCommandPayload()
{
  KISSInterceptor interceptor;
  uint8_t frame[] = {0xC0, 0x06, 0xEA, 0xDB, 0xDC, 0xDB, 0xDD, 0x03, 0x04, 0xC0};
  extended_hw_cmd_t command = {};
  assert(interceptor.extractExtendedHardwareCommand(frame, sizeof(frame), &command));
  assert(command.action == extended_hw_set_frequency);
  assert(command.data.uint32 == 0xC0DB0304UL);
}

static void testEscapeBounds()
{
  KISSInterceptor interceptor;
  uint8_t input[] = {0x01, 0xC0, 0xDB};
  uint8_t tooSmall[4] = {};
  size_t tooSmallSize = sizeof(tooSmall);
  assert(!interceptor.escape(input, sizeof(input), tooSmall, &tooSmallSize));

  uint8_t dangling[] = {0xDB};
  uint8_t result[4] = {};
  size_t resultSize = sizeof(result);
  assert(!interceptor.unescape(dangling, sizeof(dangling), result, &resultSize));
}

static void testProcessBounds()
{
  KISSInterceptor interceptor;
  uint8_t command[] = {0xC0, 0x06, 0xEB, 0xC0};
  uint8_t passthrough[8] = {};
  size_t commandCount = 0;
  size_t passthroughSize = 0;
  assert(interceptor.process(
    command, sizeof(command), nullptr, 0, &commandCount,
    passthrough, sizeof(passthrough), &passthroughSize) == kiss_process_command_overflow);

  interceptor.reset();
  uint8_t data[] = {0x01, 0x02};
  assert(interceptor.process(
    data, sizeof(data), nullptr, 0, &commandCount,
    passthrough, 1, &passthroughSize) == kiss_process_output_overflow);
}

int main()
{
  testExtractAtOffset();
  testAllCommandShapes();
  testRejectsShortAndMalformedFrames();
  testMultipleCommandsAndPassthrough();
  testFragmentedCommand();
  testEscapedCommandPayload();
  testEscapeBounds();
  testProcessBounds();
  return 0;
}
