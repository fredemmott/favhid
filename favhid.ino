// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "HID.h"
#include "EEPROM.h"
#include "protocol.hpp"

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

struct FAVHID {
  uint8_t id { 0 };

  HIDSubDescriptor* descriptor { nullptr };

  uint16_t reportLength { 0 };
  char* report { nullptr };
  
  FAVHID* next {nullptr};
};

FAVHID* rootDevice = nullptr;

void SendResponse(MessageType type) {
  ShortMessageHeader header;
  header.type = type;
  header.dataLength = 0;
  Serial.write(reinterpret_cast<const char*>(&header), sizeof(header));
}

void SendOKResponse() {
  SendResponse(MessageType::Response_OK);
}

void HandlePlugMessage(uint16_t length, char* message) {
  const auto header = reinterpret_cast<PlugDataHeader*>(message);
  const auto descriptor = message + sizeof(PlugDataHeader);
  const auto initialReport = descriptor + header->descriptorLength;

  for (auto device = rootDevice; device; device = device->next) {
    if (device->id == header->id) {
      if (device->descriptor->length != header->descriptorLength) {
        SendResponse(MessageType::Response_ConflictingID);
      }
      if (memcmp(device->descriptor->data, descriptor, header->descriptorLength)) {
        SendResponse(MessageType::Response_ConflictingID);
      }
      SendOKResponse();
      return;
    }
  }

  auto node = new FAVHID();
  node->id = header->id;

  char* descriptorCopy = new char[header->descriptorLength];
  memcpy(descriptorCopy, descriptor, header->descriptorLength);

  node->descriptor = new HIDSubDescriptor(descriptorCopy, header->descriptorLength);

  node->reportLength = header->reportLength;
  node->report = new char[node->reportLength];
  memcpy(node->report, initialReport, node->reportLength);

  if (rootDevice) { 
    FAVHID* parent = rootDevice;
    while (parent && parent->next) {
      parent = parent->next;
    }
    parent->next = node;
  } else {
    rootDevice = node;
  }
  
  HID().AppendDescriptor(node->descriptor);
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
  const auto reportLength = length - 1;

  for (auto device = rootDevice; device; device = device->next) {
    if (device->id != reportID) {
      continue;
    }

    if (device->reportLength != reportLength) {
      SendResponse(MessageType::Response_IncorrectLength);
      return;
    }
   
    memcpy(device->report, report, reportLength);
    if (HID().SendReport(reportID, device->report, reportLength)  == reportLength + 1) {
      SendOKResponse();
    } else {
      SendResponse(MessageType::Response_HIDWriteFailed);
    }
    return;
  }
  SendResponse(MessageType::Response_UnknownID);
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
    for (auto device = rootDevice; device; device = device->next) {
      HID().SendReport(device->id, device->report, device->reportLength);
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
    case MessageType::Plug:
      HandlePlugMessage(header.getLength(), message);
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