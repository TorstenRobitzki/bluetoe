@section Bootloader-Protocol

GATT
====
The Bootloader is one service containing three characteristics. The UUID of the service is 7D295F4D-2850-4F57-B595-837F5753F8A9. The Control Point characteristic receives commands and requests and sends responds by ATT notifications. The UUID of the Control Point characeteristic is 7D295F4D-2850-4F57-B595-837F5753F8A9. The Data characteristic is used to receive data. The UUID of the Data characteristic is 7D295F4D-2850-4F57-B595-837F5753F8AA. The Progress characetistic is used to give feedback about the used buffer sizes. The UUID of the Progress characteristic is 7D295F4D-2850-4F57-B595-837F5753F8AB.

The characteristics has following properties:

Characteristic | Properties        | UUID
---------------|-------------------|--------------------------------------
Control Point  | Write, Notify     | 7D295F4D-2850-4F57-B595-837F5753F8A9
Data           | Write, Write Without Response | 7D295F4D-2850-4F57-B595-837F5753F8AA
Progress       | Notify            | 7D295F4D-2850-4F57-B595-837F5753F8AB

Procedures
==========

The bootloader implements following procedures:

Procedure   | Description                                 | opcode | expected response code |
------------|---------------------------------------------|-------:|-----------------------:|
Get Version | Read a version string from the user handler |      0 |                      0 |
Get CRC     | Calculate crc over specific area            |      1 |                      1 |
Get Sizes   | Returns maschine specific values            |      2 |                      2 |
Start Flash | Start to flash a memory region              |      3 |                      3 |
Stop Flash  | Stop to flash a memory region               |      4 |                      4 |
Flush       | Flush written data to flash memory          |      5 |                      5 |
Start       | Start a programm at a specific address      |      6 |                    n/a |
Reset       | Resets the bootloader                       |      7 |                    n/a |

A client starts a procedure by sending an ATT Writing Request with the opcode to the Control Point, followed by the parameters that are required for the procedure. If the procedure starts successfully, the Bootloader will response with an ATT Write Response. The procedure will end by the reply of the bootloader that is send with an ATT Notification.

If there was something wrong with the start of the Control Point procedure, the bootloader will response immediately with an ATT Error Response and an error code denoting the reason for the failure. If both, bootloader and bootloader client are implemented without error, an ATT Error Response is not expected.

A write to the Control Point will result in either an ATT Error Response, or in an ATT Notification. A Control Point procedure is active as long as a notification is not send out by the bootloader. There should be only one active Control Point procedured at any time. That means a bootloader client has to wait for the end of a procedure before starting the next procedure.

All parameters longer than one octet are encoded in little endian.

Get Version
-----------
The procedure is started by writing the opcode to the Control Point.

Request Fields     | Length | Value |
-------------------|-------:|------:|
Opcode             | 1      | 0     |

The response is an ATT Notification, with the response code, followed by a variable length version string.

Response Fields    | Length   | Value   |
-------------------|---------:|--------:|
Response Code      | 1        | 0       |
Version String     | variable | Version |

Get CRC
-------
The procedure is started by writing the opcode, followed by the start- and end-address of the range, to the Control Point. The size of addresses depend on the target architectur and can be inquired, using the Get Sizes procedure.

Request Fields     | Length | Value |
-------------------|-------:|------:|
Opcode             | 1      | 1     |
Start-Address      | sizeof( std::uint8_t* ) | first byte of the range |
End-Address        | sizeof( std::uint8_t* ) | first byte behind the range |

The response is an ATT Notification, containing the response code and the 32 bit checksum.

Response Fields    | Length   | Value   |
-------------------|---------:|--------:|
Response Code      | 1        | 1       |
Checksum           | 4        | calculated checksum over the given range |

### CRC Algorithm
The used CRC algorithm should be adler32.

Get Sizes
---------
The procedure is started by writing the opcode to the Control Point.

Request Fields     | Length | Value |
-------------------|-------:|------:|
Opcode             | 1      | 2     |

The response is an ATT Notification, containing certain machine dependen values and buffer sizes.

Response Fields    | Length   | Value   |
-------------------|---------:|--------:|
Response Code      | 1        | 2       |
Address-Size       | 1        | sizeof( std::uint8_t* ) |
Page-Size          | 4        | Size of a Page |
Page Buffers       | 4        | Number of pages the bootloader can buffer |

The Address-Size is what the expression sizeof( std::uint8_t* ) evaluates to in the bootloader. It's used where ever an address have to be communicted between bootloader and bootloader client. The page size is the size of a single flash page. The number of Page Buffers denontes the amount of data the bootloader can store, before the client have to wait for buffers to become free.

Start Flash
-----------
The Start Flash Procedure takes an address as the first and only parameter. As the address size of the target plattform is out of the scope of the bootloader implementation, the size is equal to sizeof( std::uint8_t* ) on the bootloaders target device. The address denotes the target address of the memory contents that follows. The start address must be within the flashable ranges of the bootloader.

Request Fields     | Length | Value |
-------------------|-------:|------:|
Opcode             | 1      | 3     |
Start Address      | sizeof( std::uint8_t* ) | address of the range to be flashed |

The bootloader will response with a notification that contains the response code, followed by the current ATT MTU size of the connection, the amount of data that can currently be received by the bootloader and a checksum that is calculated over the received address.

The MTU is send for the case, that the clients BLE API does not give access to that values. The knowlage of the value allows a client to write data in packets as big as possible (MTU-3). A client can start to write the data to be flashed directly after the Start Flash procedure was started (even before the ATT Response was received). If the client has no access to the connections MTU size and not received the Control Point procedures response, it should assume an MTU of 23.

Response Fields    | Length   | Value   |
-------------------|---------:|--------:|
Response Code      | 1        | 3       |
MTU                | 1        | >= 23   |
Checksum           | 4        | crc(Start Address) |

The Bootloader is now in flash mode and will receive data and flash it into memory, as long as data is send to the Data characteristic or until a new control point procedure is received. The bootloader will flush data, when the received data reached the end of a page or when the Flush procedure is executed. A bootloader client shall not force a flush of data (neither implicit nor explicit) before it received the CRC of the start address to make sure, that the bootloader understood the start address correct.

### Buffer Management
The bootloader client has to take care that the bootloaders buffers do not overflow. The number of pages and the size of a page have to be known to the bootloader client or have to be inquired, using the Get Sizes procedure. Writing data into the middle of a page buffer will allocate the part of the buffer before the start address too.

The bootloader will send ATT Notifications using the Progress characteristic, to commucate the current free buffer sizes.

When leaving the flash mode by executing an other Control Point procedure, the next procedure will yield an error, if there was unflashed data.

### Writing parts of a page
To flash just a part of a page, set the start address to the desired start address. The bootloader will fill the used page buffer with the content before the start address with the content of the flash before the start address. This will result in unchanged flash content before the start address.

If the range to be flashed does not end at the end address of a page (addres % page-size != 0), the Flush procedure have to be invoked. The bootloader will fill the page buffer with the content of the flash behind the actual address. This makes sure, the flash gets not changed at the end of the page.

Stop Flash
----------

The procedure is started by writing the opcode to the Control Point. The bootloader client should execute the procedure, when it suspects that an error occured to reset the bootloader. Timeouts or checksum errors might be reasons to execute this procedure.

Request Fields     | Length | Value |
-------------------|-------:|------:|
Opcode             | 1      | 4     |

The response is an ATT Notification, with the response code. When the response is received, the

Response Fields    | Length   | Value   |
-------------------|---------:|--------:|
Response Code      | 1        | 4       |

Flush
-----

The procedure is started by writing the opcode to the Control Point. The bootloader must be in flash mode, when executing the procedure and there must be data in the last page buffer that was not automatically flashed (because the last bytes where not at the end of a page). After executing the Flush procedure, the bootloader leaves the flash mode.

Request Fields     | Length | Value |
-------------------|-------:|------:|
Opcode             | 1      | 5     |

The response is an ATT Notification, with the response code. When the response is received, the

Response Fields    | Length   | Value                                                                                        |
-------------------|---------:|---------------------------------------------------------------------------------------------:|
Response Code      | 1        | 5                                                                                            |
Checksum           | 4        | Checksum over Start-Address and all data received since the last Start Flash procedure start |
Consecutive        | 2        | Consecutive number, that is reseted to 0 with the start of the Start Flash procedure and is incremented after a block became free  |

Start
-----

The procedure is started by writing the opcode, followed by the start address to the Control Point.

Request Fields     | Length | Value |
-------------------|-------:|------:|
Opcode             | 1      | 6     |
Start-Address      | sizeof( std::uint8_t* ) | first byte of the range |

It is not expected that the bootloader will response with a notification, but instead reset the device and branch to the given address.

Reset
-----

The procedure is started by writing the opcode to the Control Point.

Request Fields     | Length | Value |
-------------------|-------:|------:|
Opcode             | 1      | 7     |

It is not expected that the bootloader will response with a notification, but instead reset the device.

Data
====

There is no layout for the data to be written. Data should be written with an ATT Write Request. The bootloader client have to make sure, that the bootloader can store the send data, by knowing the bootloaders buffer sizes and by observice Progress notifications. The bootloader should try to utillize the connection as much as possible and should send the data in packages that are at most MTU -3 in size.

The bootloader will response with an ATT Write Response.

The Bootloader must be in flash mode by executing the Start Flash procedure.

Progress
========

The bootloader will send progress notifications to inform the client about buffers that became free.

Notification Fields | Length | Value |
--------------------|-------:|------:|
Checksum            | 4      | Checksum over Start-Address and all data received since the last Start Flash procedure start |
Consecutive         | 2      | Consecutive number, that is reseted to 0 with the start of the Start Flash procedure and is incremented after a block became free  |
MTU                 | 1      | >= 23   |

The Consecutive number will overrun with every 65536th freed block. The purpose of the number is to alow the client to align the received values with the blocks send and the checksum that was calculated for that block. The bootloader shall notify a progress PDU when a buffer becomes free. A client can expect that the data of the freed buffer was flashed successfully.

A client that received a progress PDU shall start to send more data.

A client can use the checksum to detect transmission errors, but the client is not required to do so.

If the data that was send with the write to the Data characteristic (identified by the consecutive number), spans over the end of the current block, the checksum is calculated only till the end of the block.

A client should write the next data to the Data characteristic with a length of MTU -3 at max.


