# FAVHID

Fred's Arduino Virtual HID (FAVHID) is a sketch (program) for the Arduino Micro that accepts USB HID descriptors and reports over the USB-serial interface, and reflects them as actual HID devices and reports.

This allows it to be used as a more flexible alternative to vJoy.

## Setting up a device

1. Load this sketch to your Arduino Micro using the Arduino IDE
2. Download and build the [FAVHID-Client repository](https://github.com/fredemmott/favhid-client) repository
3. Unplug any other arduinos
4. Run the `randomize-serialize-number` program from FAVHID-Client

## Creating and using virtual joysticks

Either implement the protocol below, or use FAVHID-Client.

## Protocol

The current protocol version is `2023111702` - `yyyymmddxx`. See [protocol.hpp](protocol.hpp) for specifics.

### Initiation

1. The client opens the USB-serial connection, and sends `FAVHID<version>`

2. The Arduino reponds with either:
   - `ACKVER<version>` with the same version number, if compatible
   - `BADVER<version>` with the supported version number, if
   not compatible

Examples:

```
> FAVHID2023110702
< ACKVER2023110702

> FAVHID2023110601
< BADVER2023110702
```

### Message Format

#### Requests

All requests are preceded by a header; this is either:

```cpp
struct ShortMessageHeader {
  uint8_t messageType;
  uint8_t dataLength;
};
```

... or ...

```cpp
struct LongMessageHeader {
    uint8_t messageType;
    const uint8_t reserved {0};
    uint16_t dataLength;
};
```

The second byte of the structure can be used to identify whether a short or long header is being used; messages with no data need to use the long message header.

#### Responses

Responses always use the `ShortMessageHeader` layout, even if there is no data.

If no response data is defined for a particular request, the response *may* include debugging data that *must* be ignored.

All requests will receive a response, which should usually be `Response_OK`.

### `PushDescriptor` Request

Adds an additional an HID descriptor to the device; this will not take effect until `ResetUSB()` is called.

Data:

```
char descriptor[/* length varies */];
```

The message data length *must* be the length of the descriptor.

Calling this function will erase the 'volatile config ID'.

### `Report` Request

Asks for the Arduino to relay a HID report to the PC.

Data:

```
uint8_t reportID;
char report[/* length varies */];
```

The message data length *must* be the the length of the report + 1, to include space for the report ID.

### `ResetUSB` Request

Asks the Arduino to reset its' USB stack, without fully rebooting the device.

This is generally needed in order for the operating system to recognize descriptors that have been pushed.

### `HardReset` Request

Asks the Arduino to fully reboot, erasing all RAM including pushed descriptors.

### `SetVolatileConfigID` Request

Stores a 16-byte ID in memory; this ID will be cleared when descriptors are pushed or the device is rebooted.

This ID can be used to detect whether or not a `HardReset()` is required when restarting a client (feeder).

The ID should be unique to the configuration, e.g. a random GUID/UUID that is changed whenever the client's HID descriptors are changed.

### `GetVolatileConfigID` Request

If the response type is Response_OK, the response is 16-bytes, and contains the ID that was set with `SetVolatileConfigID`, or 16 null bytes if none is set or it has been erased.

### `SetSerialNumber` Request

Stores a 16-byte ID in non-volatile memory (EEPROM); this can be used to identify and select between multiple Arduinos all running the same firmware.

This should generally only be called once per device, and set to a random value.

### `GetSerialNumber` Request

If the response type is Response_OK, the response is 16-bytes, and contains the ID that was set with `SetSerialNumber`, or 16 bytes with undefined values if none has been set.

### `Response_OK` Response

Request succeeded.

### `Response_IncorrectLength` Response

The request failed because the data length did not meet requirements.

### `Response_HIDWriteFailed` Response

The request failed because the USB host rejected the writes.

### `Response_UnhandledRequest` Response

The message type in the header was not recognized.

## ID and Serial Number formats

These can be arbitrary data, but are compatible with UUIDs/GUIDs; random- or hash-based UUIDs are the recommended format.

## License

Copyright (c) 2023 Fred Emmott.

Permission to use, copy, modify, and/or distribute this software for any purpose
with or without fee is hereby granted, provided that the above copyright notice
and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED “AS IS” AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.
