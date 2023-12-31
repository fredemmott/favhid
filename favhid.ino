// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "HID.h"
#include "EEPROM.h"
#include "protocol.hpp"

#include <avr/wdt.h>

using namespace FAVHID;

#pragma pack(push, 1)

void HandleRequest(MessageType, const char*, size_t);

class MyHIDExt : public HID_ {
  protected:
    virtual uint8_t getShortName(char* out) override {
      static_assert(ISERIAL_MAX_LEN == FAVHID::USB_SERIAL_STRING_LENGTH);
      OpaqueID serial;
      EEPROM.get(0, serial);
      serial.ToUSBSerialString(out, ISERIAL_MAX_LEN);
      return 20;
    }
};

HID_& HIDExt() {
  static HID_* sInstance = nullptr;
  if (!sInstance) {
    sInstance = new MyHIDExt();
  }
  return *sInstance;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  wdt_disable();
  // Speed is ignored on Arduino Micro, but we need to provide one
  Serial.begin(115200);
  // Initialize so we have a serial number in the descriptor
  HIDExt();
  digitalWrite(LED_BUILTIN, 1);
}


struct MessageHeader {
  MessageType type;
  uint8_t shortLength { 0 };
  uint16_t extendedLength { 0 };

  uint16_t getLength() const {
    return shortLength ? shortLength : extendedLength;
  }
};

struct ReportNode {
  uint8_t id {};
  char* report {nullptr};
  uint16_t reportSize;

  ReportNode* next {nullptr};
};

ReportNode* gRootReport = nullptr;

void SendResponse(MessageType type) {
  ShortMessageHeader header;
  header.type = type;
  header.dataLength = 0;
  Serial.write(reinterpret_cast<const char*>(&header), sizeof(header));
}

void SendOKResponse() {
  SendResponse(MessageType::Response_OK);
}

OpaqueID gVolatileConfigID {};

void HandlePushDescriptorMessage(const char* message, size_t length) {
  char* copy = new char[length];
  memcpy(copy, message, length);

  HIDExt().AppendDescriptor(new HIDSubDescriptor(copy, length));
  gVolatileConfigID = {};

  digitalWrite(LED_BUILTIN, 1);

  char buf[7];
  memcpy(buf, "?\x05PUSH", 6);
  buf[0] = static_cast<char>(MessageType::Response_OK);
  buf[6] = static_cast<char>(length);
  Serial.write((char*)buf, 7);
}

void ResetUSB() {
  Serial.end();
  USBCON = (1 << FRZCLK);
  delay(100);
  USBDevice.attach();
  Serial.begin(115200);
}

void HandleReportMessage(const char* message, size_t length) {
  const uint8_t reportID = static_cast<uint8_t>(message[0]);
  const auto report = &message[1];
  const auto reportSize = length - 1;
  
  ReportNode* node { nullptr };
  if (!gRootReport) {
    gRootReport = new ReportNode();
    node = gRootReport;
  } else {
    node = gRootReport;
    while (node->id != reportID && node->next) {
      node = node->next;
    }
    if (node->id != reportID) {
      node->next = new ReportNode();
      node = node->next;
    }
  }

  if (!node->report) {
    node->id = reportID;
    node->reportSize = reportSize;
    node->report = new char[reportSize];
  } else if (node->reportSize != reportSize) {
    SendResponse(MessageType::Response_IncorrectLength);
    return;
  }
  
  memcpy(node->report, report, reportSize);
  const auto sent = HIDExt().SendReport(reportID, report, reportSize);
  if (sent == reportSize + 1) {
    SendOKResponse();
    return;
  }

  struct DebugData {
    uint16_t length;
    uint16_t reportSize;
    int sent;
  };
    
  char buf[sizeof(ShortMessageHeader) + sizeof(DebugData)];
  auto header = reinterpret_cast<ShortMessageHeader*>(buf);
  header->type = MessageType::Response_HIDWriteFailed;
  header->dataLength = sizeof(DebugData);
  auto debug = reinterpret_cast<DebugData*>(buf + sizeof(ShortMessageHeader));
  debug->length = length;
  debug->reportSize = reportSize;
  debug->sent = sent;
  Serial.write(buf, sizeof(buf));
}

void HandleGetSerialNumberMessage() {
  char buf[sizeof(ShortMessageHeader) + SERIAL_SIZE];
  
  auto header = reinterpret_cast<ShortMessageHeader*>(buf);
  header->type = MessageType::Response_OK;
  header->dataLength = SERIAL_SIZE;
  EEPROM.get(0, *reinterpret_cast<OpaqueID*>(buf + sizeof(ShortMessageHeader)));
  Serial.write(buf, sizeof(buf)); 
}

void HandleSetSerialNumberMessage(const char* message, size_t length) {
  if (length != SERIAL_SIZE) {
    SendResponse(MessageType::Response_IncorrectLength);
    return;
  }
  EEPROM.put(0, *reinterpret_cast<const OpaqueID*>(message));

  SendOKResponse();
}

void HandleGetVolatileConfigIDMessage() {
  char buf[sizeof(ShortMessageHeader) + sizeof(OpaqueID)];
  auto header = reinterpret_cast<ShortMessageHeader*>(buf);
  header->type = MessageType::Response_OK;
  header->dataLength = sizeof(OpaqueID);
  memcpy(buf + sizeof(ShortMessageHeader), &gVolatileConfigID, sizeof(OpaqueID));
  Serial.write(buf, sizeof(buf));
}

void HandleSetVolatileConfigIDMessage(const char* message, size_t length) {
  if (length != sizeof(gVolatileConfigID)) {
    SendResponse(MessageType::Response_IncorrectLength);
  }
  memcpy(&gVolatileConfigID, message, length);
  SendOKResponse();
}

void HardReset() {
  wdt_enable(WDTO_15MS);
  while (1) {
    __asm("nop");
  }
}

decltype(millis()) gLastSync {};

void loop() {

  const auto now = millis();
  if ((now - gLastSync) > 100) {
    for (auto node = gRootReport; node; node = node->next) {
      HIDExt().SendReport(node->id, node->report, node->reportSize);
    }
    gLastSync = now;
  }

  if (Serial.available() < 2) {
    return;
  }

  MessageHeader header;
  Serial.readBytes(reinterpret_cast<char*>(&header), 2);

  // Check for 'hello' message
  if (header.type == MessageType::Hello && header.shortLength == 'A') {
    char message[16] = "FA";
    const auto bytesRead = Serial.readBytes(&message[2], 14);
    if (bytesRead == 14 && memcmp(message, "FAVHID" FAVHID_PROTO_VERSION, 16) == 0) {
      Serial.write("ACKVER" FAVHID_PROTO_VERSION);
      Serial.flush();
    } else {
      Serial.write("BADVER" FAVHID_PROTO_VERSION);
      Serial.flush();
    }
    return;
  }

  // TODO: require hello

  if (header.shortLength == 0) {
    Serial.readBytes(reinterpret_cast<char*>(&header.extendedLength), 2);
  }

  char message[header.getLength()];
  Serial.readBytes(message, header.getLength());
  HandleRequest(header.type, message, header.getLength());
}

void HandleRequest(MessageType type, const char* data, size_t size) {
  switch (type) {
    case MessageType::Hello:
      // Handled by special-case
      [[unreachable]];
    case MessageType::PushDescriptor:
      HandlePushDescriptorMessage(data, size);
      return;
    case MessageType::Report:
      HandleReportMessage(data, size);
      return;
    case MessageType::GetSerialNumber:
      HandleGetSerialNumberMessage();
      return;
    case MessageType::SetSerialNumber:
      HandleSetSerialNumberMessage(data, size);
      return;
    case MessageType::ResetUSB:
      ResetUSB();
      return;
    case MessageType::GetVolatileConfigID:
      HandleGetVolatileConfigIDMessage();
      return;
    case MessageType::SetVolatileConfigID:
      HandleSetVolatileConfigIDMessage(data, size);
      return;
    case MessageType::HardReset:
      HardReset();
      return;
    default:
      SendResponse(MessageType::Response_UnhandledRequest);
      return;
  }
}

#pragma pack(pop)