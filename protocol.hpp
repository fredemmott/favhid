// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once
#pragma pack(push, 1)

#define FAVHID_PROTO_VERSION "2023111601"
constexpr uint8_t SERIAL_SIZE = 16;

enum class MessageType : uint8_t {
  Hello = 'F',
  RESERVED_Invalid = 0,
  Plug = 1,
  RESERVED_Unplug = 2,
  Report = 3,
  RESERVED_ResetUSB = 4,
  RESERVED_GetDescriptor = 5,
  SetSerialNumber = 6,
  GetSerialNumber = 7,

  Response_OK = 128,
  Response_UnknownID = 129,
  Response_ConflictingID = 130,
  Response_IncorrectLength = 131,
  Response_HIDWriteFailed = 132,

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

struct PlugDataHeader {
    uint8_t id { 0x00 };
    uint16_t descriptorLength {};
    uint16_t reportLength {};
};

#pragma pack(pop)