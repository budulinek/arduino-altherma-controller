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

The following data-types were observed or suspected in the payload data:

| Data type     | Definition                           |
|---------------|:-------------------------------------|
| flag8         | byte composed of 8 single-bit flags  |
| c8            | ASCII character byte                 |
| s8            | signed 8-bit integer -128 .. 127     |
| u8            | unsigned 8-bit integer 0 .. 255      |
| u6            | unsigned 6-bit integer 0 .. 63       |
| u2            | unsigned 2-bit integer 0 .. 3 or 00 .. 11 |
| s16           | signed 16-bit integer -32768..32767  |
| u16           | unsigned 16-bit integer 0 .. 65535   |
| u24           | unsigned 24-bit integer              |
| u32           | unsigned 32-bit integer              |
| u16hex        | unsigned 16-bit integer output as hex 0x0000-0xFFFF    |
| u24hex        | unsigned 24-bit integer          |
| u32hex        | unsigned 32-bit integer          |      
| f8.8 (code: f8_8)  | signed fixed point value : 1 sign bit, 7 integer bit, 8 fractional bits (two’s compliment, see explanation below) |
| f8/8 (code: f8s8)  | Daikin-style fixed point value: 1st byte is value before comma, and 2nd byte is first digit after comma (see explanation below) |
| s-abs4        | Daikin-style temperature deviation: bit 4 is sign bit, bits 0-3 is absolute value of deviation |
| sfp7          | signed floating point value: 1 sign bit, 4 mantissa bits, 2 exponent bits (used for field settings) |
| u8div10       | unsigned 8-bit integer 0 .. 255, to be divided by 10    |
| u16div10      | unsigned 16-bit integer 0 .. 65535, to be divided by 10 |
| t8            | schedule moment in 10 minute increments from midnight (0=0:00, 143=23:50) |
| d24           | day in format YY MM DD                 |

Explanation of f8.8 format: a temperature of 21.5°C in f8.8 format is represented by the 2-byte value in 1/256th of a unit as 1580 hex (1580hex = 5504dec, dividing by 256 gives 21.5). A temperature of -5.25°C in f8.8 format is represented by the 2-byte value FAC0 hex (FAC0hex = - (10000hex-FACOhex) = - 0540hex = - 1344dec, dividing by 256 gives -5.25).

Explanation of f8/8 format: a temperature of 21.5°C in f8/8 format is represented by the byte value of 21 (0x15) followed by the byte value of 5 (0x05). So far this format was only detected for the setpoint temperature, which is usually not negative, so we don't know how negative numbers are stored in this format.

# Packet types 10-1F form communication package between main controller and heat pump

Packet types 10-16 are part of the regular communication pattern between main controller and heat pump.

## Packet type 10 - operating status

### Packet type 10: request

Header: 000010

|Byte(:bit)| Hex value observed | Description              | Data type
|:---------|:-------------------|:-------------------------|:-
|0:0       | 0/1                | Heat pump (off/on)       | bit
|0:other   | 0                  | ?                        | bit
|1:7       | 0/1                | Heat pump (off/on)       | bit
|1:0       | 0/1                | 1=Heating mode           | bit
|1:1       | 0/1                | 1=Cooling mode           | bit
|1:0       | 1                  | Operating mode gas?      | bit
|1:other   | 0                  | Operating mode?          | bit
|2:1       | 0/1                | DHW tank power (off/on)  | bit
|2:0       | 0/1                | DHW (off/on)             | bit
|2:other   | 0                  | ?                        | bit
|     3    | 00                 | ?                        |
|     4    | 00                 | ?                        |
|     5    | 00                 | ?                        |
|     6    | 00                 | ?                        |
|   7-8    | 13 05              | Target room temperature  | f8.8
| 9:6      | 0/1                | ?                        | bit
| 9:5      | 0/1                | Heating/Cooling automatic mode | bit
| 9:others | 0                  | ?                        | bit
|10:2      | 0/1                | Quiet mode (off/on)      | bit
|10:others | 0                  | ?                        | bit
|    11    | 00                 | ?                        |
|12:3      | 1                  | ?                        | bit
|12:others | 0                  | ?                        | bit
|    13    | 00                 | ?                        |
|    14    | 00                 | ?                        |
|    15    | 0F                 | ?                        | flag8/bits?
|    16    | 00                 | ?                        |
|17:6      | 0/1                | operation (off/on)       | bit
|17:1      | 0/1                | booster (off/on)         | bit
|17:others | 0                  | ?                        | bit
|    18    | 3C                 | DHW target temperature   | u8 / f8.8?
|    19    | 00                 | fractional part byte 18? |

### Packet type 10: response

Header: 400010

|Byte(:bit)| Hex value observed | Description              | Data type
|:---------|:-------------------|:-------------------------|:-
| 0:0      | 0/1                | Heating power (off/on)   | bit
| 0:other  | 0                  | ?                        | bit
| 1:7      | 0/1                | Operating mode gas?      | bit
| 1:0      | 0                  | Operating mode?          | bit
| 1:other  | 0                  | Operating mode?          | bit
| 2:7      | 0/1                | DHW tank power (off/on)  | bit
| 2:6      | 0/1                | Additional zone (off/on) | bit
| 2:5      | 0/1                | Main zone (off/on)       | bit
| 2:4      | 0/1                | ?                        | bit
| 2:3      | 0                  | ?                        | bit
| 2:2      | 0                  | ?                        | bit
| 2:1      | 0/1                | cooling (off/on)         | bit
| 2:0      | 0/1                | heating (off/on)         | bit
| 3:4      | 0/1                | **DHW boost (off/on)**     | bit
| 3:0      | 0/1                | **DHW (off/on)**           | bit
| 3:others | 0                  | ?                        | bit
| 4        | 3C                 | DHW target temperature| u8 / f8.8?
| 5        | 00                 | +fractional part?        |
| 6        | 0F                 | ?                        | u8 / flag8?
| 7        | 00                 | ?
| 8        | 14                 | Target room temperature  | u8 / f8.8?
| 9        | 00                 | ?
|10        | 1A                 | ?
|11:2      | 0/1                | Quiet mode (off/on)      | bit
|11:1      | 0/1                | **?? (end of disinfection)** | bit
|11:0      | 0/1                | **Disinfection mode (off/on)** | bit
|11        | 0                  | ?                        | bit
|12        | 00                 | Error code part 1        | u8
|13        | 00                 | Error code part 2        | u8
|14        | 00                 | Error subcode            | u8
|15-17     | 00                 | ?
|17:1      | 0/1                | Defrost operation        | bit
|18:3      | 0/1                | Circ.pump (off/on)       | bit
|18:1      | 0/1                | **Backup Heater step 1 DHW (off/on)** | bit
|18:0      | 0/1                | Compressor (off/on)      | bit
|18:other  | 0                  | ?                        | bit
|19:2      | 0/1                | DHW mode                 | bit
|19:1      | 0/1                | gasboiler active1 (off/on) | bit
|19:other  | 0                  | ?                        | bit

Error codes: tbd, HJ-11 is coded as 024D2C, 89-2 is coded as 08B908, 89-3 is coded as 08B90C.

## Packet type 11 - temperatures

### Packet type 11: request

Header: 000011

| Byte nr | Hex value observed | Description             | Data type
|:--------|:-------------------|:------------------------|:-
|     0-1 | XX YY              | Actual room temperature | f8.8
|     2   | 00                 | ?
|     3   | 00                 | ?
|     4   | 00                 | ?
|     5   | 00                 | ?
|     6   | 00                 | ?
|     7   | 00                 | ?

### Packet type 11: response

Header: 400011

| Byte nr | Hex value observed | Description                                           | Data type
|:--------|:-------------------|:------------------------------------------------------|:-
|   0-1   | XX YY              | LWT temperature                                       | f8.8
|   2-3   | XX YY              | DHW temperature tank (if present)                     | f8.8
|   4-5   | XX YY              | Outside temperature (raw; in 0.5 degree resolution)   | f8.8
|   6-7   | XX YY              | RWT                                                   | f8.8
|   8-9   | XX YY              | Mid-way temperature heat pump - gas boiler            | f8.8
|  10-11  | XX YY              | Refrigerant temperature                               | f8.8
|  12-13  | XX YY              | Actual room temperature                               | f8.8
|  14-15  | XX YY              | External outside temperature sensor (if connected); otherwise Outside temperature 2 derived from external unit sensor, but stabilized; does not change during defrosts; perhaps averaged over time | f8.8
|  16-19  | 00                 | ?

## Packet type 12 - Time, date and status flags

### Packet type 12: request

Header: 000012

| Byte(:bit) | Hex value observed            | Description                      | Data type
|:-----------|:------------------------------|:---------------------------------|:-
|     0:1    | 0/1                           | pulse at start each new hour     | bit
|     0:other| 0                             | ?                                | bit
|     1      | 00-06                         | day of week (0=Monday, 6=Sunday) | u8
|     2      | 00-17                         | time - hours                     | u8
|     3      | 00-3B                         | time - minutes                   | u8
|     4      | 13-16                         | date - year (16 = 2022)          | u8
|     5      | 01-0C                         | date - month                     | u8
|     6      | 01-1F                         | date - day of month              | u8
|     7-11   | 00                            | ?                                |
|    12:6    | 0/1                           | restart process indicator ?      | bit
|    12:5    | 0/1                           | restart process indicator ?      | bit
|    12:0    | 0/1                           | restart process indicator ?      | bit
|    12:other| 0                             | ?                                | bit
|    13:2    | 0/1                           | once 0, then 1                   | bit
|    13:other| 0                             | ?                                | bit
|    14      | 00                            | ?                                |

Byte 12 has following pattern upon restart: 1x 00; 1x 01; then 41. A single value of 61 triggers an immediate heat pump restart.

### Packet type 12: response

Header: 400012

| Byte(:bit) | Hex value observed | Description           | Data type
|:-----------|:-------------------|:----------------------|:-
|     0      | 40                 | ?
|     1      | 40                 | ?
|    2-9     | 00                 | ?
|    10:4    | 0/1                | kWh preference input  | bit
|    10:other| 0                  |                       | bit
|    11      | 00/7F              | once 00, then 7F      | u8
|    12      |                    | operating mode        | flag8
|    12:7    | 0/1                | DHW active2           | bit
|    12:6    | 0/1                | gas? (depends on DHW on/off and heating on/off) | bit
|    12:0    | 0/1                | heat pump?            | bit
|    12:other| 0                  | ?                     | bit
|    13:2    | 1                  | ?                     | bit
|    13:other| 0                  | ?                     | bit
|    14-19   | 00                 | ?

## Packet type 13 - software version, DHW target temperature and flow

### Packet type 13: request

Header: 000013

| Byte(:bit) | Hex value observed | Description                        | Data type
|:-----------|:-------------------|:-----------------------------------|:-
|    0-1     | 00                 | ?
|   2:4      | 0/1                | ?                                  | bit
|   2:5,3-0  | 0                  | ?                                  | bit
|   2:7-6    | 00/01/10/11        | modus ABS / WD / ABS+prog / WD+dev | bit2

### Packet type 13: response

Header: 400013

| Byte(:bit) | Hex value observed | Description                 | Data type
|:-----------|:-------------------|:----------------------------|:-
|     0      | 3C                 | DHW target temperature (one packet delayed/from/via boiler?/ 0 in first packet after restart) | u8 / f8.8 ?
|     1      | 00                 |  +fractional part?
|     2      | 01                 | ?
|     3      | 40/D0              | ?
|   3:5-0    | 0                  | ?                                  | bit
|   3:7-6    | 00/01/10/11        | modus ABS / WD / ABS+prog / WD+dev | bit2
|   4-7      | 00                 | ?
|    8-9     | **FFFC/FFFD/**0000-010E  | flow (in 0.1 l/min) (EHV/EHYHB only. Zero on EJHA) | **s16div10 (negative if pump stops, probably bad calibration)**
|    10-11   | xxxx               | software version inner unit | u16
|    12-13   | xxxx               | software version outer unit | u16
|EHV only: 14| 00                 | ?                           | u8
|EHV only: 15| 00                 | ?                           | u8

The DHW target temperature is one packet delayed - perhaps this is due to communication with the Intergas gas boiler and confirms the actual gas boiler setting?

## Packet type 14 - LWT target temperatures, temperature deviation, ..

### Packet type 14: request

Header: 000014

| Byte(:bit) | Hex value observed | Description                      | Data type
|:-----------|:-------------------|:---------------------------------|:-
|   0-1      | 27 00              | LWT setpoint Heating Main zone   | f8.8 or f8/8?
|   2-3      | 12 00              | LWT setpoint Cooling Main zone   | f8.8 or f8/8?
|   4-5      | 27 00              | LWT setpoint Heating Add zone    | f8.8 or f8/8?
|   6-7      | 12 00              | LWT setpoint Cooling Add zone    | f8.8 or f8/8?
|     8      | 00-0A,10-1A        | LWT deviation Heating Main zone  | s-abs4
|     9      | 00-0A,10-1A        | LWT deviation Cooling Main zone  | s-abs4
|    10      | 00-0A,10-1A        | LWT deviation Heating Add zone   | s-abs4
|    11      | 00-0A,10-1A        | LWT deviation Cooling Add zone   | s-abs4
|   12       | 00/37              | first package 37 instead of 00   | u8
|   13-14    | 00                 | ?

### Packet type 14: response

Header: 400014

| Byte(:bit) | Hex value observed | Description               | Data type
|:-----------|:-------------------|:--------------------------|:-
|     0-14   | XX                 | echo of 000014-{00-14}    |
|   15-16    | 1C-24 00-09        | Target LWT Main zone in 0.1 degree (based on outside temperature in 0.5 degree resolution)| f8/8?
|   17-18    | 1C-24 00-09        | Target LWT Add zone  in 0.1 degree (based on outside temperature in 0.5 degree resolution)| f8/8?

## Packet type 15 - temperatures, operating mode

### Packet type 15: request

Header: 000015

| Byte(:bit) | Hex value observed    | Description                     | Data type
|:-----------|:----------------------|:--------------------------------|:-
|     0      | 00                    | ?
|     1-2    | 01/09/0A/0B 54/F4/C4/D6/F0 | schedule-induced operating mode? | flag8,flag8?
|     3      | 00                    | ?
|     4      | 03                    | ?
|     5      | 20/52                 | ?

### Packet type 15: response

Header: 400015

| Byte(:bit) | Hex value observed    | Description                       | Data type
|:-----------|:----------------------|:----------------------------------|:-
|    0-1     | 00                    | Refrigerant temperature?          | f8.8?
|    2-3     | FD-FF,00-08 00/80     | Refrigerant temperature (in 0.5C) | f8.8
|    4-5     | 00                    | Refrigerant temperature?          | f8.8?
|EHV only: 6 | 00-19                 | parameter number                  | u8/u16
|EHV only: 7 | 00                    | (part of parameter or value?)     |
|EHV only: 8 | XX                    | ?                                 | s16div10_LE?

EHV is the only model for which we have seen use of the <parameter,value> mechanism in the basic packet types.
The following parameters have been observed in this packet type on EHV model only:

| Parameter number | Parameter Value               | Description
|:-----------------|:------------------------------|:-
|EHV: 05           | 96                            | 15.0 degree?
|EHV: 08           | 73,78,7D                      | 11.5, 12.0, 12.5 ?
|EHV: 0A           | C3,C8                         | 19.5, 20.0
|EHV: 0C           | 6E,73,78                      | 11.0, 11.5, 12.0
|EHV: 0E           | B9,BE                         | 18.5, 19.0
|EHV: 0F           | 68,6B                         | 10.4, 10.7
|EHV: 10           | 05                            |  0.5
|EHV: 19           | 01                            |  0.1
|EHV: others       | 00                            | ?

These parameters are likely s16div10.

## Packet type 16 - temperatures

Only observed on EHV\* and EJHA\* heat pumps.

### Packet type 16: request

Header: 000016

| Byte(:bit) | Hex value observed | Description       | Data type
|:-----------|:-------------------|:------------------|:-
|    0-1     | 00                 | ?
|    2-3     | 32 14              | room temperature? | f8.8?
|    4-15    | 00                 | ?

### Packet type 16: response

Header: 400016

| Byte(:bit) | Hex value observed | Description                      | Data type
|:-----------|:-------------------|:---------------------------------|:-
|    0       |                    | **Current in 0.1 A**             | u8div10
|    1       |                    | **Power input in 0.1 kW**       | u8div10
|    0-7     | 00                 | ?                                |
|    6       |                    | **Heating/cooling output in 0.1 kW** | u8div10
|    7       |                    | **DHW output in 0.1 kW**         | u8div10
|    8       | E6                 | ?                                |

# Packet types A1 and B1 communicate text data

## Packet type A1

Used for name of product?

### Packet type A1: request

Header: 0000A1

| Byte nr       | Hex value observed            | Description           | Data type
|:--------------|:------------------------------|:----------------------|:-
|     0         | 00                            | ?
|  1-15         | 30                            | ASCII '0'             | c8
| 16-17         | 00                            | ASCII '\0'            | c8

### Packet type A1: response

Header: 4000A1

| Byte nr       | Hex value observed            | Description           | Data type
|:--------------|:------------------------------|:----------------------|:-
|     0         | 00                            | ?
|  1-15         | 00                            | ASCII '\0' (missing) name outside unit | c8
| 16-17         | 00                            | ASCII '\0'            | c8

## Packet type B1 - heat pump name

Product name.

### Packet type B1: request

Header: 0000B1

| Byte nr       | Hex value observed            | Description           | Data type
|:--------------|:------------------------------|:----------------------|:-
|     0         | 00                            | ?
|  1-15         | 30                            | ASCII '0'             | c8
| 16-17         | 00                            | ASCII '\0'            | c8

### Packet type B1: response

Header: 4000B1

| Byte nr       | Hex value observed            | Description           | Data type
|:--------------|:------------------------------|:----------------------|:-
|     0         | 00                            | ?
|  1-12         | XX                            | ASCII "EHYHBH08AAV3" name inside unit | c8
| 13-17         | 00                            | ASCII '\0'            | c8

# Packet type B8 - counters, #hours, #starts, electricity used, energy produced

Counters for energy consumed and operating hours. The main controller specifies which data type it would like to receive. The heat pump responds with the requested data type counters. A B8 package is only transmitted by the main controller after a manual menu request for these counters. P1P2Monitor can insert B8 requests to poll these counters, but this violates the rule that an auxiliary controller should not act as main controller. But if timed carefully, it works.

### Packet type B8: request

Header: 0000B8

| Byte nr       | Hex value observed            | Description           | Data type
|:--------------|:------------------------------|:----------------------|:-
|     0         | XX                            | data type requested <br> 00: energy consumed <br> 01: energy produced <br> 02: pump and compressor hours <br> 03: backup heater hours <br> 04: compressor starts <br> 05: boiler hours and starts | u8

### Packet type B8: response

Header: 4000B8

#### Data type 00

| Byte nr       | Hex value observed            | Description                           | Data type
|:--------------|:------------------------------|:--------------------------------------|:-
|     0         | 00                            | data type 00 : energy consumed (kWh)  | u8
|   1-3         | XX XX XX                      | by backup heater for heating          | u24
|   4-6         | XX XX XX                      | by backup heater for DHW              | u24
|   7-9         | 00 XX XX                      | by compressor for heating             | u24
| 10-12         | XX XX XX                      | by compressor for cooling             | u24
| 13-15         | XX XX XX                      | by compressor for DHW                 | u24
| 16-18         | XX XX XX                      | total                                 | u24

#### Data type 01

| Byte nr       | Hex value observed            | Description                          | Data type
|:--------------|:------------------------------|:-------------------------------------|:-
|     0         | 01                            | data type 01 : energy produced (kWh) | u8
|   1-3         | XX XX XX                      | for heating                          | u24
|   4-6         | XX XX XX                      | for cooling                          | u24
|   7-9         | XX XX XX                      | for DHW                              | u24
| 10-12         | XX XX XX                      | total                                | u24

On EJHA\* all counters for type 01 are always zero.

#### Data type 02

| Byte nr       | Hex value observed            | Description                    | Data type
|:--------------|:------------------------------|:-------------------------------|:-
|     0         | 02                            | data type 02 : operating hours | u8
|   1-3         | XX XX XX                      | pump hours                     | u24
|   4-6         | XX XX XX                      | compressor for heating         | u24
|   7-9         | XX XX XX                      | compressor for cooling         | u24
| 10-12         | XX XX XX                      | compressor for DHW             | u24

#### Data type 03

| Byte nr       | Hex value observed            | Description                    | Data type
|:--------------|:------------------------------|:-------------------------------|:-
|     0         | 03                            | data type 03 : operating hours | u8
|   1-3         | XX XX XX                      | backup heater1 for heating     | u24
|   4-6         | XX XX XX                      | backup heater1 for DHW         | u24
|   7-9         | XX XX XX                      | backup heater2 for heating     | u24
| 10-12         | XX XX XX                      | backup heater2 for DHW         | u24
| 13-15         | XX XX XX                      | ?                              | u24
| 17-18         | XX XX XX                      | ?                              | u24

#### Data type 04

| Byte nr       | Hex value observed            | Description                 | Data type
|:--------------|:------------------------------|:----------------------------|:-
|     0         | 04                            | data type 04                | u8
|   1-3         | XX XX XX                      | ?                           | u24
|   4-6         | XX XX XX                      | ?                           | u24
|   7-9         | XX XX XX                      | ?                           | u24
| 10-12         | XX XX XX                      | number of compressor starts | u24

#### Data type 05

| Byte nr       | Hex value observed            | Description                               | Data type
|:--------------|:------------------------------|:------------------------------------------|:-
|    0          | 05                            | data type 05 : gas boiler in hybrid model | u8
|  1-3          | XX XX XX                      | boiler operating hours for heating        | u24
|  4-6          | XX XX XX                      | boiler operating hours for DHW            | u24
|  7-9          | XX XX XX                      | gas usage for heating (unit tbd)          | u24
| 10-12         | XX XX XX                      | gas usage for heating (unit tbd)          | u24
| 13-15         | XX XX XX                      | number of boiler starts                   | u24
| 16-18         | XX XX XX                      | gas usage total (unit tbd)                | u24

Internal gas metering seems only supported on newer models, not on the AAV3.
