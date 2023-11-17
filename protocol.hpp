// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once
#pragma pack(push, 1)

#ifdef FAVHID_CLIENT
#include <cinttypes>
#include <cstring>
#include <string>
#endif

namespace FAVHID {

#define FAVHID_PROTO_VERSION "2023111702"
constexpr uint8_t SERIAL_SIZE = 16;

/** Can be used for serial numbers or config IDs.
 *
 * If you are on Windows, this is compatible with a GUID.
 *
 * Everyone else: I'm sorry :p
 */
struct OpaqueID {
  inline bool operator==(const OpaqueID& other) const {
    return memcmp(this, &other, sizeof(OpaqueID)) == 0;
  }

  inline bool IsZero() const {
    return *this == OpaqueID {};
  }

#ifdef FAVHID_CLIENT
  std::string HumanReadable() const;

  void Randomize();
  static OpaqueID Random();
#endif

  unsigned long Data1 {};
  unsigned short Data2 {};
  unsigned short Data3 {};
  unsigned char Data4[8] {};
};
static_assert(sizeof(OpaqueID) == SERIAL_SIZE);

enum class MessageType : uint8_t {
  Hello = 'F',
  RESERVED_Invalid = 0,
  PushDescriptor,
  Report,
  SetSerialNumber,
  GetSerialNumber,
  GetVolatileConfigID,
  SetVolatileConfigID,
  ResetUSB,
  HardReset,

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
  const uint8_t reserved {0};
  uint16_t dataLength;

  LongMessageHeader& operator=(const LongMessageHeader& other) {
    this->type = other.type, this->dataLength = other.dataLength;
    return *this;
  }
};

#pragma pack(pop)

}// namespace FAVHID