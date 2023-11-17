// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "HID.h"
#include "EEPROM.h"
#include "protocol.hpp"

using namespace FAVHID;

#pragma pack(push, 1)

void setup() {
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);
  // Speed is ignored on Arduino Micro, but we need to provide one
  Serial.begin(115200);
  digitalWrite(LED_BUILTIN, 0);
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

void HandlePushDescriptorMessage(uint16_t length, char* message) {
  char* copy = new char[length];
  memcpy(copy, message, length);

  HID().AppendDescriptor(new HIDSubDescriptor(copy, length));
  digitalWrite(LED_BUILTIN, 1);
  SendOKResponse();
  delay(100);

  USBCON = (1 << FRZCLK);
  delay(100);
  USBDevice.attach();
}

void HandleReportMessage(uint16_t length, char* message) {
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
  if (HID().SendReport(reportID, report, reportSize) == reportSize + 1) {
    SendOKResponse();
  } else {
    SendResponse(MessageType::Response_HIDWriteFailed);
  }
}

void HandleGetSerialNumberMessage() {
  char buf[sizeof(ShortMessageHeader) + SERIAL_SIZE];
  struct Data { uint8_t v[SERIAL_SIZE]; };

  auto header = reinterpret_cast<ShortMessageHeader*>(buf);
  header->type = MessageType::Response_OK;
  header->dataLength = SERIAL_SIZE;
  EEPROM.get(0, *reinterpret_cast<Data*>(buf + sizeof(ShortMessageHeader)));
  Serial.write(buf, sizeof(buf)); 
}

void HandleSetSerialNumberMessage(uint16_t length, const char* message) {
  if (length != SERIAL_SIZE) {
    SendResponse(MessageType::Response_IncorrectLength);
    return;
  }
  struct Data { uint8_t v[SERIAL_SIZE]; };
  EEPROM.put(0, *reinterpret_cast<const Data*>(message));

  SendOKResponse();
}

decltype(millis()) gLastSync {};

void loop() {

  const auto now = millis();
  if ((now - gLastSync) > 100) {
    for (auto node = gRootReport; node; node = node->next) {
      HID().SendReport(node->id, node->report, node->reportSize);
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
  switch (header.type) {
    case MessageType::Hello:
      // Handled above
      [[unreachable]];
    case MessageType::PushDescriptor:
      HandlePushDescriptorMessage(header.getLength(), message);
      return;
    case MessageType::Report:
      HandleReportMessage(header.getLength(), message);
      return;
    case MessageType::GetSerialNumber:
      HandleGetSerialNumberMessage();
      return;
    case MessageType::SetSerialNumber:
      HandleSetSerialNumberMessage(header.getLength(), message);
      return;
    default:
      SendResponse(MessageType::Response_UnhandledCommand);
      return;
  }

}

#pragma pack(pop)