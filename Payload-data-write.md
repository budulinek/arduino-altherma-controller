## Write data to P1P2 bus

Arduino adapter communicates with the main controller (user interface), rather than the heat pump itself. Also, the program goes to great lengths to avoid bus collision with packets sent by other devices on the bus (by the heat pump, main controller, other external controllers). Yet be aware: hic sunt leones and you are on your own. **No guarantees, use this program at your own risk.** Remember that you can damage or destroy your (expensive) heat pump. Be careful and watch for errors in the web interface. I also recommend reading documentation provided by the author of the library: https://github.com/Arnold-n/P1P2Serial

Arduino acts as an external controller. It waits for a handshake packet (type 0x30) from the main Daikin controller (= user interface attached at the indoor unit), replies with response (packet type 0x30), and in the next round of request-response sends new data (commands) to the main controller.

These writeable packets have a specific structure:

* **Header**
  * 1 byte indicating **direction of the communication**: request from master (0x00), response from slave (0x40)
  * 1 byte **slave address**: 0xF0, 0xF1 external controller(s)
  * 1 byte **packet type**: the packet type indicates what kind of data is transmitted in the payload
* **Payload**
  * 2 bytes (little endian) **parameter number**
  * 1 or 2 bytes (little endian) **parameter value**
  * more parameter number-value pairs can be sent until the payload is full
  * payload has a fixed length, so 0xFF is used to fill the empty space 
* **Checksum**
  * 1 byte **CRC checksum** 

The program forms the packet itself, all you need to do is send the command (via UDP) using this format:

`<packet type><parameter number><parameter value>`

Remember that both parameter number and value use **<ins>little endian bytes order</ins>**.

Here are few examples of commands you can send via UDP or Serial:

`35400001`	= turn DHW on<br>
`35`: packet type 0x35<br>
`4000`: parameter number 40<br>
`01`: parameter value 1

`360300D601`	= set DHW setpoint to 47°C<br>
`36`: packet type 0x36<br>
`0300`: parameter number 03<br>
`D601`: parameter value 01D6 HEX = 470 DEC

`360800F6FF`	= set LWT setpoint deviation to -1°C<br>
`36`: packet type 0x36<br>
`0800`: parameter number 03<br>
`F6FF`: parameter value FFF6 HEX = -10 DEC

The P1P2 bus is much slower than UDP or Serial, therefore incoming commands are temporarily stored in a queue (circular buffer).

## Daikin Altherma Hybrid and Daikin Altherma LT protocol data format

Daikin P1P2 protocol payload data format for Daikin Altherma Hybrid (perhaps all EHYHB(H/X) models) and Daikin Altherma LT (perhaps all  EHV(H/X) models). Big thanks go to Arnold Niessen for his development of the P1P2 adapter and the P1P2Serial library. This document is based on reverse engineering and assumptions, so there may be mistakes and misunderstandings.

This document describes payload data of few **selected writeable packet types** (packet types 0x35, 0x36 and 0x3A) exchanged between the **main controller (requests)** and the **external controller (responses)**. 

Payload of these packets has specific structure, it contains pairs of **parameter number** (2 bytes) + **parameter value** (1 or 2 bytes). Parameter numbers and values use little endian bytes order. Since the payload length is fixed, each data packet can hold only a limited number of these number-value pairs. Empty space in the payload is filled with 0xFF. 

### Data types

The following data types were observed in the parameter values. Little endian bytes ordering is used in multi-byte data types:

| Data type   | Definition (read)                     | Definition (write)                     |
| ----------- | ------------------------------------- | ------------------------------------- |
| u8          | unsigned 8-bit integer 0 .. 255       | |
| s16         | signed 16-bit integer -32768..32767   | |
| u8div2min16 | unsigned 8-bit integer 0 .. 255, divide by 2 and deduct 16 | multiply by 2 and add 32 |


Explanation of **s16** format: a temperature of 21.5°C  is represented by the value of 215 in little endian format (0xD700). A temperature of  -1°C  is represented by the value of -10 in little endian format (0xF6FF).

Observations show that a few hundred parameters can be exchanged via packet types 0x3X, but only some of them are writeable. The following tables summarize all known writeable parameters for Daikin Altherma LT and Altherma Hybrid:

### Packet type 0x35

| Parameter number | Description              | Applies for | Data type | Byte: description              |
| ---------------- | ------------------------ | ----------- | --------- | ------------------------------ |
| 03               | Quiet mode               |             | u8        | 0x00: off<br>0x01: on         |
| 2F               | Climate control          | LWT mode    | u8        | 0x00: off<br/>0x01: on         |
| 31               | Climate control          | RT mode     | u8        | 0x00: off<br/>0x01: on         |
| 3A*              | Heating/cooling          |             | u8        | 0x01: heating<br>0x02: cooling |
| 36               | Defrost request          |             | u8        | 0x00: off<br>0x01: on (request) |
| 40               | DHW control              |             | u8        | 0x00: off<br/>0x01: on         |
| 48               | DHW boost                |             | u8        | 0x00: off<br/>0x01: on         |
| 56               | Weather dependent / Fixed mode | LWT mode | u8        | 0x00: fixed<br/>0x01: weather dep.<br>0x02: fixed+scheduled<br>0x03: weather dep.+scheduled         |

*not working correctly, use packet type 0x3A, parameter 4E instead

### Packet type 0x36

All temperature values in this table are in 0.1 °C resolution.

| Parameter number | Description                              | Applies for | Data type | Byte: description |
| ---------------- | ---------------------------------------- | ----------- | --------- | ----------------- |
| 00               | Room heating setpoint                    | RT mode     | s16       |                   |
| 01               | Room cooling setpoint                    | RT mode     | s16       |                   |
| 03               | DHW setpoint                             |             | s16       |                   |
| 06               | LWT heating setpoint (main zone)         | LWT - Fixed mode | s16       |                   |
| 07               | LWT cooling setpoint (main zone)         | LWT - Fixed mode | s16       |                   |
| 08               | LWT heating deviation (main zone)        | LWT - WD mode | s16       |                   |
| 09               | LWT cooling deviation (main zone)        | LWT - WD mode | s16       |                   |
| 0B               | LWT heating setpoint (additional zone)   | LWT - Fixed mode | s16       |                   |
| 0C               | LWT cooling setpoint (additional zone)   | LWT - Fixed mode | s16       |                   |
| 0D               | LWT heating deviation (additional zone)  | LWT - WD mode | s16       |                   |
| 0E               | LWT cooling deviation (additional zone)  | LWT - WD mode | s16       |                   |



### Packet type 0x3A

| Parameter number | Description            | Data type | Byte: description                                            |
| ---------------- | ---------------------- | --------- | ------------------------------------------------------------ |
| 00               | 12h/24h time format    | u8        | 0x00: 12h time format<br>0x01: 24h time format               |
| 31               | Enable holiday ??      | u8        | ??                                                           |
| 39               | Preset LWT deviation heating comfort | u8div2min16 |                                    |
| 3A               | Preset LWT deviation heating eco     | u8div2min16 |                                    |
| 3B               | Decimal delimiter      | u8        | 0x00: dot<br/>0x01: comma                                    |
| 3D               | Flow units             | u8        | 0x00: l/min<br/>0x01: GPM                                    |
| 3F               | Temperature units      | u8        | 0x00: °F<br/>0x01: °C                                        |
| 40               | Energy units           | u8        | 0x00: kWh<br/>0x01: MBtu                                     |
| 45               | Preset room cooling comfort | u8div2min16 |                                    |
| 46               | Preset room cooling eco     | u8div2min16 |                                    |
| 47               | Preset room heating comfort | u8div2min16 |                                    |
| 48               | Preset room heating eco     | u8div2min16 |                                    |
| 49               | Preset mode            | u8        | 0x00: schedule<br>0x01: eco<br>0x02: comfort                                   |
| 4B               | Daylight saving time   | u8        | 0x00: manual<br>0x01: auto                                   |
| 4C               | Quiet mode             | u8        | 0x00: auto<br/>0x01: always off<br>0x02: on                         |
| 4D               | Quiet mode level       | u8        | 0x00: level 1<br/>0x01: level 2<br/>0x02: level 3 (most silent)           |
| 4E               | Operation mode         | u8        | 0x00: heating<br/>0x01: cooling<br/>0x02: auto               |
| 5B               | Holiday                | u8        | 0x00: off<br>0x01: on                                        |
| 5E               | Heating schedule       | u8        | 0x00: Predefined 1<br>0x01: Predefined 2<br>0x02: Predefined 3<br>0x03: User defined 1<br>0x04: User defined 2<br>0x05: User defined 3<br>0x06: No schedule<br> |
| 5F               | Cooling schedule       | u8        | 0x00: Predefined 1<br>0x01: Predefined 2<br>0x02: Predefined 3<br>0x03: User defined 1<br>0x04: No schedule<br/> |
| 64               | DHW schedule           | u8        | 0x00: Predefined 1<br>0x01: Predefined 2<br>0x02: Predefined 3<br>0x03: User defined 1<br>0x04: No schedule |

