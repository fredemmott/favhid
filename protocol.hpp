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
constexpr uint8_t FIRST_AVAILABLE_REPORT_ID = 3;
constexpr char USB_STRING_DESCRIPTOR_CHARS[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
constexpr uint8_t USB_SERIAL_STRING_LENGTH = 20; // including null byte

/** Can be used for serial numbers or config IDs.
 *
 * If you are on Windows, this is compatible with a GUID.
 *
 * Everyone else: I'm sorry :p
 */
struct OpaqueID {
  unsigned long Data1 {};
  unsigned short Data2 {};
  unsigned short Data3 {};
  unsigned char Data4[8] {};

  inline bool operator==(const OpaqueID& other) const {
    return memcmp(this, &other, sizeof(OpaqueID)) == 0;
  }

  inline bool IsZero() const {
    return *this == OpaqueID {};
  }

  inline void ToUSBSerialString(char* out, size_t size) const {
    static_assert(USB_SERIAL_STRING_LENGTH == 20);
    if (size != USB_SERIAL_STRING_LENGTH) {
      return;
    }
    constexpr char prefix[] = "FAVHID#";
    constexpr auto fixedBytes = sizeof(prefix) - 1;
    memcpy(out, prefix, fixedBytes);

    // Save one byte for null
    const auto serialBytes = 19 - fixedBytes;
    const auto bytes = reinterpret_cast<const uint8_t*>(this);
    for (int i = 0; i < serialBytes; ++i) {
      out[i + fixedBytes] = USB_STRING_DESCRIPTOR_CHARS[bytes[i] % sizeof(USB_STRING_DESCRIPTOR_CHARS)];
    }
    out[19] = 0;
  }

#ifdef FAVHID_CLIENT
  std::string HumanReadable() const;
  std::string ToUSBSerialString() const;

  void Randomize();
  static OpaqueID Random();
#endif

};
static_assert(sizeof(OpaqueID) == SERIAL_SIZE);

enum class MessageType : uint8_t {
  Hello = 'F',
  RESERVED_Invalid = 0,
  // Data: raw HID descriptor
  PushDescriptor,
  // Data: { uint8_t reportID, char[] report }
  Report,
  // Returned data: OpaqueID
  GetSerialNumber,
  // Data: OpaqueID
  SetSerialNumber,
  // Returned data: OpaqueID
  GetVolatileConfigID,
  // Data: OpaqueID
  SetVolatileConfigID,
  // No data
  ResetUSB,
  // No data
  HardReset,

  Response_OK = 128,
  Response_IncorrectLength,
  Response_HIDWriteFailed,

  Response_UnhandledRequest = 255,
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