# Daikin-P1P2---UDP-Gateway

Daikin P1P2 <---> UDP Gateway on Arduino

## Functionality

This program implements P1P2Serial library (https://github.com/Arnold-n/P1P2Serial) in order to connect to Daikin P1/P2 interface, used in Daikin/Rotex heat pumps. Custom HW adapter is necessary. The program reads from the P1P2 bus and forwards data via UDP or Serial. You can also use this program to control your Daikin heat pump: Commands received from UDP or Serial are written to the P1/P2 bus. Big thanks go to Arnold Niessen for his development of the P1P2 adapter, P1P2Serial library and reverse engineering of the protocol. 

## Intro: Daikin P1P2 bus

Daikin uses the two wire P1P2 bus for connection between the heat pump itself and the main controller (user interface). The same P1P2 bus is used to connect Daikin external controllers (such as LAN adapter) or third-party external controllers (KNX adapter). Unfortunately, the possibilities of connecting and controlling Daikin Altherma heat pumps are very limited. There is no external Modbus adapter for newer Altherma models, the official LAN adapter is very limited in functionality and as of today Daikin has no usable public API or cloud-based solution. In terms of connectivity, Daikin Altherma is lagging behind its competitors. Therefore, we have decided to help ourselves.

## Hardware

* **Daikin heat pump** with P1P2 interface. The program should work with these models:
  * **Daikin Altherma LT** (sketch tested with EHVX model)
  * **Daikin Altherma Hybrid** (library tested with EHYHBX model)
  * possibly other models of the Altherma series
  * contributions and feedback welcomed (especially if you have new Altherma 3)
* **DIY adapter** based on the MM1192 chip. You can find the schematics of the adapter in the repo of the library: https://github.com/Arnold-n/P1P2Serial
* **Arduino**. Tested with Uno and Mega, but other boards can be used.
* **Ethernet Shield** with W5500 (or W5200, W5100) chipset.

## Read data from P1P2 bus

Arduino monitors P1P2 bus. The heat pump and the main controller continuously exchange all sensor data (leaving and returning temperature, DHW temperature, water flow, outdoor and indoor temperature etc.) and operational data (compressor and circulation pump running, valves open or closed, heating and cooling modes, defrost, etc.). Therefore, it is easy to monitor the heat pump operation in real time, simply by passively listening to the communication. Listening to the bus should be quite safe. But still, be aware that the bus is powered (15V DC), so avoid short circuits.

P1P2 bus uses a master/slave protocol. Communication is asynchronous: master sends a "request" data packet and waits for "response" data packet from a slave (or timeout) before sending new packet. There can only be one master. The master is the main controller (Daikin user interface attached to the unit), all other devices on the bus act as slaves: the heat pump itself and external controllers. Each data packet consists of:

* **Header**
  * 1 byte indicating **direction of the communication**: request from master (0x00), response from slave (0x40) or other (0x80)
  * 1 byte **slave address**: 0x00 heat pump
  * 1 byte **packet type**: the packet type indicates what kind of data is transmitted in the payload. For example, packet type 0x11 (response from the heat pump) contains temperature values (leaving and returning water, outdoor and indoor temperature etc.)
* **Payload**
  * Up to cca 30 bytes of **payload data**. Payload length and content are determined by the header. For example, packet with header 0x400011 (= response from heat pump, packet type 0x11) always has 18 byte payload. First and second bytes of the payload in this particular packet always hold the leaving water temperature. Content (and length) of thepayload may vary depending on the heat pump model.
* **Checksum**
  * 1 byte **CRC checksum** 

There is a large number of packet types observed on the P1P2 bus, each of them with specific payload. For more details about the P1P2 protocol (what we learned so far through reverse engineering) and for a full overview of the packet types identified (empirically observed on the tested heat pumps), please visit https://github.com/Arnold-n/P1P2Serial.

The P1P2 bus is very busy, therefore this Arduino program **filters data packets** exchanged between the heat pump, main controller and external controllers and only forwards packets which contain useful and interesting data. By default, only packet types 0x10 - 0x16 and 0xB8 (which contain all useful data from Altherma LT and Altherma Hybrid) are forwarded to UDP and Serial, but you can configure your own data type range in config file. Also, **the payload of these packets is stored** in memory and only packets with new (modified) data are forwarded.

Data packet is forwarded as-it-is (Header + Payload) in Hex String format (or raw Hex - see config). The main purpose of this program is the **integration of Daikin Altherma heat pumps into larger home automation and monitoring systems** (such as Loxone, OpenHAB, Home Assistant, EmonCMS and other). You will need one of these systems (or at least some custom script) to parse and convert the UDP or Serial data into human-readable variables.

Payload data formats (i.e. instructions how to parse data you are interested in) for 0x10 - 0x16 and 0xB8 packet types are described here: 

**[Readable packet types](https://github.com/budulinek/Daikin-P1P2---UDP-Gateway/blob/main/Payload-data-read.md)**

## Write data to P1P2 bus

Arduino (with the custom P1P2 adapter attached) can control the heat pump by actively communicating with the main controller (user interface). The main controller periodically polls the P1P2 bus for the presence of external controllers. Arduino acts as an external controller: it replies with a "handshake" response (packet type 0x30), and in the next round of request-response sends new data (commands) to the main controller. At the moment, only packet types 0x35, 0x36 and 0x3A are supported for writing data.

These packets have a specific structure: 

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

The program takes care of the handshake, selection of the slave address and CRC calculation. All you need to do is send the command (via UDP or Serial) using this format:

**\<packet type>\<parameter number>\<parameter value>**

Remember that both parameter number and value use little endian bytes order. Overview of all writeable parameters and data types used parameter values is in this document: 

[**Writeable packet types**](https://github.com/budulinek/Daikin-P1P2---UDP-Gateway/blob/main/Payload-data-write.md)

Here are few examples of commands you can send via UDP or Serial:

`35400001`	= turn DHW on

`360300D601`	= set DHW setpoint to 47°C

`360800F6FF`	= set LWT setpoint deviation to -1°C

There is also one "service" command:

`00`	= clear stored payloads, resend all data 

The P1P2 bus is much slower than UDP or Serial, therefore incoming commands are temporarily stored in a queue (circular buffer).

Older Daikin units (Altherma LT and Altherma Hybrid) only support one external controller. Since Daikin LAN adapter, or a 3rd party KNX adapter (Zennio) also act as external controllers, you have to disconnect them before using Arduino as an external controller. Newer Daikin units (Altherma 3) seem to support more external controllers.

Arduino uses hysteresis when writing temperature setpoints to the heat pump, in order to minimize wear and tear of the heat pump's EEPROM. If the difference between new and old value is smaller than hysteresis, new value  will NOT be written to P1P2 bus. But you should still be cautious - do not let your home automation system change the values too often. One of Daikin's manuals states a maximum of 7000 setting changes per year.

Arduino adapter communicates with the main controller (user interface), rather than the heat pump itself. Also, the program goes to great lengths to avoid bus collision with packets sent by other devices on the bus (by the heat pump, main controller, other external controllers). Yet be aware: hic sunt leones and you are on your own. **No guarantees, use this program at your own risk.** Remember that you can damage or destroy your (expensive) heat pump. Be careful and watch for errors in the output. I also recommend reading documentation provided by the author of the library: https://github.com/Arnold-n/P1P2Serial

## Settings

Specify network parameters in P1P2-UDP_NetConfig.h file. Random MAC address is generated and stored in EEPROM.

You can also configure the behaviour of this program in P1P2-UDP_Config.h file, especially:

* format of UDP input and output. Choose between HEX String or raw HEX.
* range of packet types which will be forwarded via UDP or Serial
* memory size for storing packet data (program forwards a packet only if it contains new or changed data)
* various timeouts

## Error codes

Various error codes are sent via UDP and Serial if things go wrong, in the following format:

**EEEEEE\<error code>**

For a list of error codes see:

**[Error codes](https://github.com/budulinek/Daikin-P1P2---UDP-Gateway/blob/main/Error-codes.md)**
