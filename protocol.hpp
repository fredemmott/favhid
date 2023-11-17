// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once
#pragma pack(push, 1)

#ifdef FAVHID_CLIENT
#include <cinttypes>
#endif

namespace FAVHID {

#define FAVHID_PROTO_VERSION "2023111701"
constexpr uint8_t SERIAL_SIZE = 16;

enum class MessageType : uint8_t {
  Hello = 'F',
  RESERVED_Invalid = 0,
  PushDescriptor,
  Report,
  SetSerialNumber,
  GetSerialNumber,

  Response_OK = 128,
  Response_IncorrectLength,
  Response_HIDWriteFailed,

  Response_UnhandledCommand = 255,
};

struct ShortMessageHeader {
    MessageType type;
    uint8_t dataLength {};
};

struct LongMessageHeader {
    MessageType type;
    const uint8_t reserved { 0 };
    uint16_t dataLength;

    LongMessageHeader& operator=(const LongMessageHeader& other) {
        this->type = other.type,
        this->dataLength = other.dataLength;
        return *this;
    }
};

#pragma pack(pop)

}