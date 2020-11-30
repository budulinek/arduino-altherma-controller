# Daikin Altherma Hybrid and Daikin Altherma LT protocol data format

Daikin P1P2 protocol payload data format for Daikin Altherma Hybrid (perhaps all EHYHB(H/X) models) and Daikin Altherma LT (perhaps all  EHV(H/X) models). Big thanks go to Arnold Niessen for his development of the P1P2 adapter and the P1P2Serial library. This document is based on reverse engineering and assumptions, so there may be mistakes and misunderstandings.

This document describes payload data of few **selected writeable packet types** (packet types 0x35, 0x36 and 0x3A) exchanged between the **main controller (requests)** and the **external controller (responses)**. 

Payload of these packets has specific structure, it contains pairs of **parameter number** (2 bytes) + **parameter value** (1 or 2 bytes). Parameter numbers and values use little endian bytes order. Since the payload length is fixed, each data packet can hold only a limited number of these number-value pairs. Empty space in the payload is filled with 0xFF. 

### Data types

The following data types were observed in the parameter values. Little endian bytes ordering is used in multi-byte data types:

| Data type | Definition                          |
| --------- | ----------------------------------- |
| u8        | unsigned 8-bit integer 0 .. 255     |
| s16       | signed 16-bit integer -32768..32767 |

Explanation of **s16** format: a temperature of 21.5°C  is represented by the value of 215 in little endian format (0xD700). A temperature of  -1°C  is represented by the value of -10 in little endian format (0xF6FF).

Observations show that a few hundred parameters can be exchanged via packet types 0x3X, but only some of them are writeable. The following tables summarize all known writeable parameters for Daikin Altherma LT and Altherma Hybrid:

### Packet type 0x35

| Parameter number | Description              | Data type | Byte: description              |
| ---------------- | ------------------------ | --------- | ------------------------------ |
| 03               | Silent mode              | u8        | 0x00: off<br>0x01: on          |
| 2F               | LWT control              | u8        | 0x00: off<br/>0x01: on         |
| 31               | Room temperature control | u8        | 0x00: off<br/>0x01: on         |
| 3A*              | Heating/cooling          | u8        | 0x01: heating<br>0x02: cooling |
| 40               | DHW control              | u8        | 0x00: off<br/>0x01: on         |
| 48               | DHW boost                | u8        | 0x00: off<br/>0x01: on         |

*not working correctly, use packet type 0x3A, parameter 4E instead

### Packet type 0x36

All temperature values in this table are in 0.1 °C resolution.

| Parameter number | Description                              | Data type | Byte: description |
| ---------------- | ---------------------------------------- | --------- | ----------------- |
| 00               | Room temperature setpoint                | s16       |                   |
| 03               | DHW setpoint                             | s16       |                   |
| 06*              | LWT setpoint (main zone)                 | s16       |                   |
| 08**             | LWT setpoint deviation (main zone)       | s16       |                   |
| 0B*              | LWT setpoint (additional zone)           | s16       |                   |
| 0D**             | LWT setpoint deviation (additional zone) | s16       |                   |

*applies only in Fixed LWT mode

**applies only in Weather dep. LWT mode

### Packet type 0x3A

| Parameter number | Description            | Data type | Byte: description                                            |
| ---------------- | ---------------------- | --------- | ------------------------------------------------------------ |
| 31               | Enable holiday ??      | u8        | ??                                                           |
| 3B               | Decimal delimiter      | u8        | 0x00: dot<br/>0x01: comma                                    |
| 3D               | Flow units             | u8        | 0x00: l/min<br/>0x01: GPM                                    |
| 3F               | Temperature units      | u8        | 0x00: °F<br/>0x01: °C                                        |
| 40               | Energy units           | u8        | 0x00: kWh<br/>0x01: MBtu                                     |
| 4B               | Daylight saving time   | u8        | 0x00: manual<br>0x01: auto                                   |
| 4C               | Silent mode            | u8        | 0x00: auto<br/>0x01: off<br>0x02: on                         |
| 4D               | Silent mode level      | u8        | 0x00: level 1<br/>0x01: level 2<br/>0x02: level 3            |
| 4E               | Operation mode         | u8        | 0x00: heating<br/>0x01: cooling<br/>0x02: auto               |
| 5B               | Holiday                | u8        | 0x00: off<br>0x01: on                                        |
| 5E               | Space heating schedule | u8        | 0x00: Predefined 1<br>0x01: Predefined 2<br>0x02: Predefined 3<br>0x03: User defined 1<br>0x04: User defined 2<br>0x05: User defined 3<br>0x06: No schedule<br> |
| 5F               | Space cooling schedule | u8        | 0x00: Predefined 1<br>0x01: Predefined 2<br>0x02: Predefined 3<br>0x03: User defined 1<br>0x04: No schedule<br/> |
| 64               | DHW schedule           | u8        | 0x00: Predefined 1<br>0x01: Predefined 2<br>0x02: Predefined 3<br>0x03: User defined 1<br>0x04: No schedule |

