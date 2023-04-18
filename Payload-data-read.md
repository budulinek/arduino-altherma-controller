## Read data from P1P2 bus

Listening to the P1/P2 bus should be quite safe. But still, be aware that the bus is powered (15V DC), so avoid short circuits. P1P2 bus uses a master/slave protocol. Communication is asynchronous: master sends a "request" data packet and waits for "response" data packet from a slave (or timeout) before sending new packet. There can only be one master. The master is the main controller (Daikin user interface attached to the unit), all other devices on the bus act as slaves: the heat pump itself and external controllers. Each data packet consists of:

* **Header**
  * 1 byte indicating **direction of the communication**: request from master (0x00), response from slave (0x40) or other (0x80)
  * 1 byte **slave address**: 0x00 heat pump
  * 1 byte **packet type**: the packet type indicates what kind of data is transmitted in the payload. For example, packet type 0x11 (response from the heat pump) contains temperature values (leaving and returning water, outdoor and indoor temperature etc.)
* **Payload**
  * Up to cca 30 bytes of **payload data**. Payload length and content are determined by the header. For example, packet with header 0x400011 (= response from heat pump, packet type 0x11) always has 18 byte payload. First and second bytes of the payload in this particular packet always hold the leaving water temperature. Content (and length) of thepayload may vary depending on the heat pump model.
* **Checksum**
  * 1 byte **CRC checksum** 

There is a large number of packet types observed on the P1P2 bus, each of them with specific payload. For more details about the P1P2 protocol (what we learned so far through reverse engineering) and for a full overview of the packet types identified (empirically observed on the tested heat pumps), please visit https://github.com/Arnold-n/P1P2Serial.

Data packet is forwarded as-it-is (Header + Payload) as raw HEX. 

## Daikin Altherma Hybrid and Daikin Altherma LT protocol data format

Daikin P1P2 protocol payload data format for Daikin Altherma Hybrid (perhaps all EHYHB(H/X) models) and Daikin Altherma LT (perhaps all  EHV(H/X) models). Big thanks go to Arnold Niessen for his development of the P1P2 adapter and the P1P2Serial library. Most of the information in these tables is based on his observation of the P1P2 protocol. This document is based on reverse engineering and assumptions, so there may be mistakes and misunderstandings.

This document describes payload data of few **selected readable packet types** (packet types 0x10 - 0x16 and 0xB8) exchanged between the **main controller (requests)** and the **heat pump (responses)**. For a more complete overview see Arnold's documentation here: https://github.com/Arnold-n/P1P2Serial/tree/master/doc

### Data types

The following data types were observed in the payload. Bytes are ordered ordered in a "normal" way, i.e. big endian:

| Data type | Definition                                                   |
| --------- | ------------------------------------------------------------ |
| flag8     | byte composed of 8 single-bit flags                          |
| u8        | unsigned 8-bit integer 0 .. 255                              |
| u16       | unsigned 16-bit integer 0..65535                             |
| u24       | unsigned 24-bit integer 0..16777215                          |
| f8.8      | signed fixed point value : 1 sign bit, 7 integer bit, 8 fractional bits (two’s compliment, see explanation below) |
| f8/8      | Daikin-style fixed point value: 1st byte is value before comma, and 2nd byte is first digit after comma (see explanation below) |
| s-abs4    | Daikin-style temperature deviation: bit 4 is sign bit, bits 0-3 is absolute value of deviation |

Explanation of **f8.8** format: a temperature of 21.5°C in f8.8 format is represented by the 2-byte value in 1/256th of a unit as 1580 hex  (1580hex = 5504dec, dividing by 256 gives 21.5). A temperature of  -5.25°C in f8.8 format is represented by the 2-byte value FAC0 hex  (FAC0hex = - (10000hex-FAC0hex) = - 0540hex = - 1344dec, dividing by 256 gives -5.25).

Explanation of **f8/8** format: a temperature of 21.5°C in f8/8 format is represented by the byte value of 21 (0x15) followed by the byte value  of 5 (0x05). So far this format was only detected for the setpoint  temperature, which is usually not negative, so we don't know how  negative numbers are stored in this format.

# Packet type 0x10

### Packet type 0x10: request

Header: 0x000010

| Data byte | Description                      | Data type | Bit: description                                             |
| --------- | -------------------------------- | --------- | ------------------------------------------------------------ |
| 0         | Climate                          | flag8     | 0: power (off/on)                                            |
| 1         | Climate status                   | flag8     | 0: heating (off/on)<br/>1: cooling (off/on)<br/>7: status (standby / running) |
| 2         | DHW tank                         | flag8     | 0: power (off/on)                                            |
| 3         | ?                                |           |                                                              |
| 4         | ?                                |           |                                                              |
| 5         | ?                                |           |                                                              |
| 6         | ?                                |           |                                                              |
| 7         | Target room temperature          | u8        |                                                              |
| 8         | ?                                |           |                                                              |
| 9         | Heating/cooling operation mode ? | flag8     | 5: ?<br/>6: ?                                                |
| 10        | Quiet mode                       | flag8     | 2: quiet mode (off/on)                                       |
| 11        | ?                                |           |                                                              |
| 12        | ??                               | flag8     | 3: ?                                                         |
| 13        | ?                                |           |                                                              |
| 14        | ?                                |           |                                                              |
| 15        | ??                               | flag8     | 0: ?<br/>1: ?<br/>2: ?<br/>3: ?                              |
| 16        | ?                                |           |                                                              |
| 17        | DHW tank mode                    | flag8     | 1: booster (off/on)<br/>6: operation (off/on)                |
| 18-19     | DHW target temperature           | f8/8      |                                                              |

### Packet type 0x10: response

Header: 0x400010

| Data  byte | Description             | Data  type | Bit:  description                                            |
| ---------- | ----------------------- | ---------- | ------------------------------------------------------------ |
| 0          | LWT control             | flag8      | 0: LWT control (off/on)                                      |
| 1          | ??                      | flag8      | 7: ?                                                         |
| 2          | Valves                  | flag8      | 0: heating (off/on)<br>1: cooling (off/on)<br/>4: ?<br/>5: main zone (off/on)<br/>6: additional zone (off/on)<br/>7: DHW tank (off/on) |
| 3          | DHW control             | flag8      | 0: DHW control (off/on)<br/>4: DHW boost (off/on)            |
| 4-5        | DHW target temperature  | f8/8       |                                                              |
| 6          | ??                      | flag8      | 0: ?<br/>1: ?<br/>2: ?<br/>3: ?                              |
| 7          | ?                       |            |                                                              |
| 8          | Target room temperature | u8         |                                                              |
| 9          | ?                       |            |                                                              |
| 10         | Space operation mode    | flag8      | 1: ?<br/>3: ?<br/>4: ?<br/>5: auto (off/on)<br/>6: ?         |
| 11         | Quiet mode              | flag8      | 2: quiet mode (off/on)                                       |
| 12-16      | ?                       |            |                                                              |
| 17         | Operation mode          | flag8      | 0: DHW boost (off/on)<br>1: Defrost (off/on)<br>6: DHW (off/on) |
| 18         | Pump and compressor     | flag8      | 0: compressor (off/on)<br/>3: pump (off/on)                  |
| 19         | DHW ?                   | flag8      | 1: ?<br/>0: mode??                                           |

# Packet type 0x11

### Packet type 0x11: request

Header: 0x000011

| Data  byte | Description             | Data  type | Bit:  description |
| ---------- | ----------------------- | ---------- | ----------------- |
| 0-1        | Actual room temperature | f8.8       |                   |
| 2          | ?                       |            |                   |
| 3          | ?                       |            |                   |
| 4          | ?                       |            |                   |
| 5          | ?                       |            |                   |
| 6          | ?                       |            |                   |
| 7          | ?                       |            |                   |

### Packet type 0x11: response

Header: 0x400011

| Data byte | Description                                                  | Data type | Bit: description |
| --------- | ------------------------------------------------------------ | --------- | ---------------- |
| 0-1       | Leaving water temperature                                    | f8.8      |                  |
| 2-3       | DHW temperature                                              | f8.8      |                  |
| 4-5       | Outside temperature 1 (raw; in 0.5 degree resolution)        | f8.8      |                  |
| 6-7       | Returning water temperature                                  | f8.8      |                  |
| 8-9       | Mid-way temperature (heat exchanger)                         | f8.8      |                  |
| 10-11     | Refrigerant temperature                                      | f8.8      |                  |
| 12-13     | Actual room temperature                                      | f8.8      |                  |
| 14-15     | External temperature sensor (if connected);<br>otherwise  Outside temperature 2 derived from external unit sensor, but  stabilized; it does not change during defrosts; it also adds extra  variations not in raw outside temperature, perhaps due to averaging over samples | f8.8      |                  |
| 16-17     | ?                                                            |           |                  |

# Packet type 0x12

### Packet type 0x12: request

Header: 0x000012

| Data  byte | Description                                                  | Data type | Bit: description                  |
| ---------- | ------------------------------------------------------------ | --------- | --------------------------------- |
| 0          | new  hour value                                              | flag8     | 1:  new-hour or restart indicator |
| 1          | day  of week (0=Monday, 6=Sunday)                            | u8        |                                   |
| 2          | time  - hours                                                | u8        |                                   |
| 3          | time  - minutes                                              | u8        |                                   |
| 4          | date  - year (0x13 = 2019)                                   | u8        |                                   |
| 5          | date  - month                                                | u8        |                                   |
| 6          | date  - day of month                                         | u8        |                                   |
| 7-11       | ?                                                            | u8        |                                   |
| 12         | upon  restart: 1x 00; 1x 01; then 41; a single value of 61 triggers an immediate  restart | flag8     | 1:  ?<br>5: reboot<br/>6: ?       |
| 13         | once  00, then 04                                            | flag  8   | 2:  ?                             |
| 14         | ?                                                            |           |                                   |

### Packet type 0x12: response

Header: 0x400012

| Data byte | Description                                               | Data type | Bit: description                                             |
| --------- | --------------------------------------------------------- | --------- | ------------------------------------------------------------ |
| 0         | ??                                                        |           |                                                              |
| 1         | ??                                                        |           |                                                              |
| 2-9       | ?                                                         |           |                                                              |
| 9         | External thermostat main                                  | flag8     | 1: main B - cooling (off/on)<br>2: main A - heating (off/on) |
| 10        | kWh preference input(s)<br>External thermostat additional | flag8     | 4: preference kWh input<br>7: additional B - cooling (off/on) |
| 11        | Current limit in 0.1 A ??                                 | u8        |                                                              |
| 12        | operating mode                                            | flag8     | 0: heat pump? 6: gas?  7: DHW active2 ?                      |
| 13        | ?                                                         |           |                                                              |
| 14-29     | ?                                                         |           |                                                              |


# Packet type 0x13

### Packet type 0x13: request

Header: 0x000013

| Data byte | Description                        | Data type | Bit: description |
| --------- | ---------------------------------- | --------- | ---------------- |
| 0-1       | ?                                  |           |                  |
| 2         | first package 0x00 instead of 0xD0 | flag8 ?   |                  |

### Packet type 0x13: response

Header: 0x400013

| Data byte       | Description                                                  | Data type   | Bit: description |
| --------------- | ------------------------------------------------------------ | ----------- | ---------------- |
| 0               | DHW target temperature<br>(one packet delayed/from/via boiler?/ 0 in first packet after restart) | u8 / f8.8 ? |                  |
| 1               | ?                                                            |             |                  |
| 2               | ??                                                           |             |                  |
| 3               | ??                                                           |             |                  |
| 4-6             | ?                                                            |             |                  |
| 7               | no flow                                                      |             | FF = no flow     |
| 8-9             | flow (in 0.1 l/min)                                          | u16         |                  |
| 10-11           | software version inner unit                                  | u16         |                  |
| 12-13           | software version outer unit                                  | u16         |                  |
| EHV only:    14 | ??                                                           | u8          |                  |
| EHV only:    15 | ??                                                           | u8          |                  |

# Packet type 0x14

### Packet type 0x14: request

Header: 0x000014

| Data byte | Description                                               | Data  type | Bit:  description |
| --------- | --------------------------------------------------------- | ---------- | ----------------- |
| 0-1       | Target LWT main zone                                      | f8/8       |                   |
| 2         | ??                                                        |            |                   |
| 3         | ?                                                         |            |                   |
| 4         | ?? first package 0x37 instead  of 0x2D (55 instead of 45) | u8         |                   |
| 5         | ?                                                         |            |                   |
| 6         | ?? first package 0x37 instead  of 0x07                    | u8         |                   |
| 7         | ?                                                         |            |                   |
| 8         | delta-T                                                   | s-abs4     |                   |
| 9         | ?? night/eco related mode ?  7:00: 02 9:00: 05            |            |                   |
| 10-11     | ?                                                         |            |                   |
| 12        | first package 0x37 instead of  0x00                       |            |                   |
| 13-14     | ?                                                         |            |                   |

### Packet type 0x14: response

Header: 0x400014

| Data  byte | Description                          | Data  type | Bit:  description |
| ---------- | ------------------------------------ | ---------- | ----------------- |
| 0-1        | Target LWT main zone (desired)       | f8/8       |                   |
| 2-3        | ??                                   | f8/8       |                   |
| 4-5        | Target LWT additional zone (desired) | f8/8       |                   |
| 6          | ?                                    |            |                   |
| 7          | ?                                    |            |                   |
| 8          | ? LWT offset                         |            |                   |
| 9          | ?                                    |            |                   |
| 10-14      |                                      |            |                   |
| 15-16      | Target LWT main zone (actual)        | f8/8       |                   |
| 17-18      | Target LWT additional zone (actual)  | f8/8       |                   |

# Packet type 0x15

### Packet type 0x15: request

Header: 0x000015

| Data byte | Description                | Data type | Bit: description |
| --------- | -------------------------- | --------- | ---------------- |
| 0         | ?                          |           |                  |
| 1-2       | operating mode? 7:30: 01D6 |           |                  |
| 3         | ?                          |           |                  |
| 4         | ??                         |           |                  |
| 5         | ??                         |           |                  |

### Packet type 0x15: response

Header: 0x400015

| Data byte | Description                                      | Data type | Bit: description |
| --------- | ------------------------------------------------ | --------- | ---------------- |
| 0-1       | ?                                                |           |                  |
| 2-3       | Refrigerant temperature in 0.5 degree resolution | f8.8      |                  |
| 4-5       | ?                                                |           |                  |
| 6*        | parameter number (see table bellow)              | u8        |                  |
| 7*        | (part of parameter or parameter value?)          | u8        |                  |
| 8*        | ??                                               | u8        |                  |

*observed on Daikin Altherma LT (EHVH/EHVX) heat pumps, missing on Daikin Altherma Hybrid (EHYHBX)

| Parameter number | Parameter Value | Description        | Data type | Bit: description |
| ---------------- | --------------- | ------------------ | --------- | ---------------- |
| 05               | 96              | 15.0 degree  ?     | u8div10   |                  |
| 08               | 73,78,7D        | 11.5, 12.0, 12.5 ? | u8div10   |                  |
| 0A               | C3,C8           | 19.5, 20.0         | u8div10   |                  |
| 0C               | 6E,73,78        | 11.0, 11.5, 12.0   | u8div10   |                  |
| 0E               | B9,BE           | 18.5, 19.0         | u8div10   |                  |
| 0F               | 68,6B           | 10.4, 10.7         | u8div10   |                  |
| 10               | 05              | 0.5                | u8div10   |                  |
| 19               | 01              | 0.1                | u8div10   |                  |
| others           | 00              | ??                 | u8        |                  |


# Packet type 0x16

observed on Daikin Altherma LT (EHVH/EHVX) heat pumps, missing on Daikin Altherma Hybrid (EHYHBX)

### Packet type 0x16: request

Header: 0x000016

| Data byte | Description       | Data type | Bit: description |
| --------- | ----------------- | --------- | ---------------- |
| 0-1       | ?                 |           |                  |
| 2-3       | room temperature? | f8.8      |                  |
| 4-15      | ?                 |           |                  |

### Packet type 0x16: response

Header: 0x400016

| Data byte | Description                      | Data type | Bit: description |
| --------- | -------------------------------- | --------- | ---------------- |
| 0         | Current in 0.1 A                 | u8div10   |                  |
| 1         | Power input in 0.1 kW            | u8div10   |                  |
| 2         | ?                                |           |                  |
| 3         | ?                                |           |                  |
| 4         | ?                                |           |                  |
| 5         | ?                                |           |                  |
| 6         | Heating/cooling output in 0.1 kW | u8div10   |                  |
| 7         | DHW output in 0.1 kW             | u8div10   |                  |
| 8         | ??                               |           |                  |

# Packet type 0xB8

These packets contain counters for energy consumed and produced, operating hours and number of starts. Communication in packet type B8 is specific. In request, master specifies data type it would like to receive. Slave (heat pump) responds with the requested data type.

### Packet type 0xB8: request

Header: 0x0000B8

| Data byte | Description | Data type | Byte: description                                            |
| --------- | ----------- | --------- | ------------------------------------------------------------ |
| 0         | Data type   | u8        | 0x00: energy consumed (kWh)<br>0x01: energy produced (kWh)<br>0x02: pump and compressor hours <br>0x03: backup heater hours <br>0x04: compressor starts <br>0x05: gas boiler hours |

### Packet type 0xB8: response

Header: 0x4000B8

#### Data type 0x00

| Data byte | Description                                  | Data type | Bit: description |
| --------- | -------------------------------------------- | --------- | ---------------- |
| 0         | Data type 0x00                               | u8        |                  |
| 1-3       | energy consumed for heating (backup heater?) | u24       |                  |
| 4-6       | energy consumed for DHW (backup heater?)     | u24       |                  |
| 7-9       | energy consumed for heating (compressor?)    | u24       |                  |
| 10-12     | energy consumed for cooling                  | u24       |                  |
| 13-15     | energy consumed for DHW (compressor?)        | u24       |                  |
| 16-18     | energy consumed total                        | u24       |                  |

#### Data type 0x01

| Data byte | Description                 | Data type | Bit: description |
| --------- | --------------------------- | --------- | ---------------- |
| 0         | Data type 0x01              | u8        |                  |
| 1-3       | energy produced for heating | u24       |                  |
| 4-6       | energy produced for cooling | u24       |                  |
| 7-9       | energy produced for DHW     | u24       |                  |
| 10-12     | energy produced total       | u24       |                  |

#### Data type 0x02

| Data byte | Description                        | Data type | Bit: description |
| --------- | ---------------------------------- | --------- | ---------------- |
| 0         | Data type 0x02                     | u8        |                  |
| 1-3       | number of pump hours               | u24       |                  |
| 4-6       | number of compressor hours heating | u24       |                  |
| 7-9       | number of compressor hours cooling | u24       |                  |
| 10-12     | number of compressor hours DHW     | u24       |                  |

#### Data type 0x03

| Data byte | Description                      | Data type | Bit: description |
| --------- | -------------------------------- | --------- | ---------------- |
| 0         | Data type 0x03                   | u8        |                  |
| 1-3       | backup heater1 hours for heating | u24       |                  |
| 4-6       | backup heater1 hours for DHW     | u24       |                  |
| 7-9       | backup heater2 hours for heating | u24       |                  |
| 10-12     | backup heater2 hours for DHW     | u24       |                  |
| 13-15     | ?                                | u24       |                  |
| 16-18     | ?                                | u24       |                  |

#### Data type 0x04

| Data byte | Description                 | Data type | Bit: description |
| --------- | --------------------------- | --------- | ---------------- |
| 0         | Data type 0x04              | u8        |                  |
| 1-3       | ?                           | u24       |                  |
| 4-6       | ?                           | u24       |                  |
| 7-9       | ?                           | u24       |                  |
| 10-12     | number of compressor starts | u24       |                  |

#### Data type 0x05

| Data byte | Description                            | Data type | Bit: description |
| --------- | -------------------------------------- | --------- | ---------------- |
| 0         | Data type 0x05                         | u8        |                  |
| 1-3       | number of gas boiler hours for heating | u24       |                  |
| 4-6       | number of gas boiler hours for DHW     | u24       |                  |
| 7-9       | ?                                      | u24       |                  |
| 10-12     | ?                                      | u24       |                  |
| 13-15     | number of gas boiler hours total       | u24       |                  |
| 16-18     | ?                                      | u24       |                  |

