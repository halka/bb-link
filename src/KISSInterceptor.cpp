#include "KISSInterceptor.h"
#include <string.h>

static const uint8_t FEND = 0xC0;
static const uint8_t FESC = 0xDB;
static const uint8_t TFEND = 0xDC;
static const uint8_t TFESC = 0xDD;

KISSInterceptor::KISSInterceptor() : pendingFrameSize(0) {}

bool KISSInterceptor::extractExtendedHardwareCommand(uint8_t *buffer, size_t size, extended_hw_cmd_t *cmd)
{
  if (buffer == nullptr || cmd == nullptr || size < 4)
  {
    return false;
  }

  for (size_t start = 0; start + 1 < size; ++start)
  {
    if (buffer[start] != FEND || buffer[start + 1] != CMD_HARDWARE)
      continue;

    for (size_t end = start + 2; end < size; ++end)
    {
      if (buffer[end] == FEND)
        return decodeExtendedHardwareCommand(buffer + start, end - start + 1, cmd);
    }
  }
  return false;
}

void KISSInterceptor::reset()
{
  pendingFrameSize = 0;
}

bool KISSInterceptor::appendPassthrough(
  const uint8_t *data,
  size_t size,
  uint8_t *passthrough,
  size_t passthroughCapacity,
  size_t *passthroughSize,
  kiss_output_event_t *events,
  size_t eventCapacity,
  size_t *eventCount)
{
  if (size == 0)
    return true;
  if (*passthroughSize + size > passthroughCapacity)
    return false;

  if (*eventCount > 0 && events[*eventCount - 1].type == kiss_output_data)
  {
    events[*eventCount - 1].size += size;
  }
  else
  {
    if (*eventCount >= eventCapacity)
      return false;
    events[*eventCount].type = kiss_output_data;
    events[*eventCount].offset = *passthroughSize;
    events[*eventCount].size = size;
    (*eventCount)++;
  }

  memcpy(passthrough + *passthroughSize, data, size);
  *passthroughSize += size;
  return true;
}

bool KISSInterceptor::appendCommand(
  const extended_hw_cmd_t &cmd,
  kiss_output_event_t *events,
  size_t eventCapacity,
  size_t *eventCount)
{
  if (*eventCount >= eventCapacity)
    return false;

  events[*eventCount].type = kiss_output_command;
  events[*eventCount].offset = 0;
  events[*eventCount].size = 0;
  events[*eventCount].command = cmd;
  (*eventCount)++;
  return true;
}

kiss_process_result_t KISSInterceptor::process(
  const uint8_t *buffer,
  size_t size,
  kiss_output_event_t *events,
  size_t eventCapacity,
  size_t *eventCount,
  uint8_t *passthrough,
  size_t passthroughCapacity,
  size_t *passthroughSize)
{
  if (eventCount == nullptr || passthroughSize == nullptr ||
      (size > 0 && buffer == nullptr) ||
      (eventCapacity > 0 && events == nullptr) ||
      (passthroughCapacity > 0 && passthrough == nullptr))
    return kiss_process_output_overflow;

  *eventCount = 0;
  *passthroughSize = 0;

  for (size_t i = 0; i < size; ++i)
  {
    const uint8_t value = buffer[i];

    if (pendingFrameSize == 0)
    {
      if (value == FEND)
      {
        pendingFrame[pendingFrameSize++] = value;
      }
      else if (!appendPassthrough(&value, 1, passthrough, passthroughCapacity, passthroughSize, events, eventCapacity, eventCount))
      {
        return kiss_process_output_overflow;
      }
      continue;
    }

    if (pendingFrameSize >= MAX_FRAME_SIZE)
    {
      if (!appendPassthrough(pendingFrame, pendingFrameSize, passthrough, passthroughCapacity, passthroughSize, events, eventCapacity, eventCount) ||
          !appendPassthrough(&value, 1, passthrough, passthroughCapacity, passthroughSize, events, eventCapacity, eventCount))
        return kiss_process_output_overflow;
      pendingFrameSize = 0;
      continue;
    }

    pendingFrame[pendingFrameSize++] = value;
    if (value != FEND)
      continue;

    if (pendingFrameSize == 2)
    {
      // Consecutive FEND bytes are KISS synchronization, not a data frame.
      pendingFrameSize = 1;
      continue;
    }

    const bool isHardwareFrame = pendingFrameSize >= 3 && pendingFrame[1] == CMD_HARDWARE;
    if (isHardwareFrame)
    {
      extended_hw_cmd_t cmd = {};
      if (decodeExtendedHardwareCommand(pendingFrame, pendingFrameSize, &cmd))
      {
        if (!appendCommand(cmd, events, eventCapacity, eventCount))
        {
          pendingFrameSize = 0;
          return kiss_process_event_overflow;
        }
      }
      // Unknown or malformed hardware commands are reserved control frames and
      // are deliberately not forwarded to the radio.
    }
    else if (!appendPassthrough(pendingFrame, pendingFrameSize, passthrough, passthroughCapacity, passthroughSize, events, eventCapacity, eventCount))
    {
      pendingFrameSize = 0;
      return kiss_process_output_overflow;
    }

    // A KISS FEND can end one frame and begin the next. Keep it until the next
    // byte arrives; duplicate FEND delimiters are valid KISS synchronization.
    pendingFrame[0] = FEND;
    pendingFrameSize = 1;
  }

  return kiss_process_ok;
}

bool KISSInterceptor::decodeExtendedHardwareCommand(const uint8_t *frame, size_t size, extended_hw_cmd_t *cmd)
{
  if (frame == nullptr || cmd == nullptr || size < 4 || frame[0] != FEND || frame[size - 1] != FEND)
    return false;

  uint8_t unescapedBuffer[MAX_FRAME_SIZE];
  size_t unescapedSize = sizeof(unescapedBuffer);
  if (!unescape(const_cast<uint8_t *>(frame), size, unescapedBuffer, &unescapedSize) ||
      unescapedSize < 4 || unescapedBuffer[1] != CMD_HARDWARE)
    return false;

  memset(cmd, 0, sizeof(*cmd));
  switch (unescapedBuffer[2])
  {
  case EXTENDED_HW_CMD_SET_FREQUENCY:
    if (unescapedSize != 8) return false;
    cmd->action = extended_hw_set_frequency;
    cmd->data.uint32 = (static_cast<uint32_t>(unescapedBuffer[3]) << 24) |
                       (static_cast<uint32_t>(unescapedBuffer[4]) << 16) |
                       (static_cast<uint32_t>(unescapedBuffer[5]) << 8) |
                       static_cast<uint32_t>(unescapedBuffer[6]);
    return true;
  case EXTENDED_HW_CMD_SET_BAUD_RATE:
    if (unescapedSize != 5) return false;
    cmd->action = extended_hw_set_baud_rate;
    cmd->data.uint8 = unescapedBuffer[3];
    return true;
  case EXTENDED_HW_CMD_PAIR_WITH_DEVICE:
    if (unescapedSize != 10) return false;
    cmd->action = extended_hw_pair_with_device;
    memcpy(cmd->data.bytes, unescapedBuffer + 3, 6);
    return true;
  case EXTENDED_HW_CMD_SET_RIG_CTRL:
    if (unescapedSize != 5) return false;
    cmd->action = extended_hw_set_rig_ctrl;
    cmd->data.uint8 = unescapedBuffer[3];
    return true;
  case EXTENDED_HW_CMD_RESTORE_FREQUENCY:
    cmd->action = extended_hw_restore_frequency;
    break;
  case EXTENDED_HW_CMD_START_SCAN:
    cmd->action = extended_hw_start_scan;
    break;
  case EXTENDED_HW_CMD_STOP_SCAN:
    cmd->action = extended_hw_stop_scan;
    break;
  case EXTENDED_HW_CMD_CLEAR_PAIRED_DEVICE:
    cmd->action = extended_hw_clear_paired_device;
    break;
  case EXTENDED_HW_CMD_FIRMWARE_VERSION:
    cmd->action = extended_hw_firmware_version;
    break;
  case EXTENDED_HW_CMD_CAPABILITIES:
    cmd->action = extended_hw_capabilities;
    break;
  case EXTENDED_HW_CMD_API_VERSION:
    cmd->action = extended_hw_api_version;
    break;
  case EXTENDED_HW_CMD_GET_PAIRED_DEVICE:
    cmd->action = extended_hw_get_paired_device;
    break;
  case EXTENDED_HW_CMD_FACTORY_RESET:
    cmd->action = extended_hw_factory_reset;
    break;
  default:
    return false;
  }

  return unescapedSize == 4;
}

bool KISSInterceptor::unescape(uint8_t *buffer, size_t size, uint8_t *result, size_t *resultSize)
{
  if (buffer == nullptr || result == nullptr || resultSize == nullptr)
    return false;

  uint8_t *src = buffer;
  uint8_t *dst = result;
  const size_t capacity = *resultSize;
  while(src < buffer + size)
  {
    if (static_cast<size_t>(dst - result) >= capacity)
      return false;

    if (*src == FESC)
    {
      src++;
      if (src >= buffer + size)
        return false;
      if (*src == TFEND)
      {
        *dst = FEND;
      }
      else if (*src == TFESC)
      {
        *dst = FESC;
      }
      else
      {
        // Invalid escape sequence
        return false;
      }
    }
    else
    {
      *dst = *src;
    }
    src++;
    dst++;
  }
  *resultSize = dst - result;
  return true;
}

// Output buffer is assumed to be large enough to hold the escaped data
bool KISSInterceptor::escape(uint8_t *buffer, size_t size, uint8_t *result, size_t *resultSize)
{
  if (buffer == nullptr || result == nullptr || resultSize == nullptr)
    return false;
  uint8_t *src = buffer;
  uint8_t *dst = result;
  size_t dstSize = *resultSize;

  if (dstSize < size * 2 + 2)
  {
    return false;
  }

  *dst = FEND;
  dst++;

  for (size_t i = 0; i < size; i++)
  {
    if (*src == FEND)
    {
      *dst = FESC;
      dst++;
      *dst = TFEND;
    }
    else if (*src == FESC)
    {
      *dst = FESC;
      dst++;
      *dst = TFESC;
    }
    else
    {
      *dst = *src;
    }
    src++;
    dst++;
  }

  *dst = FEND;
  dst++;

  *resultSize = dst - result;
  return true;
}
