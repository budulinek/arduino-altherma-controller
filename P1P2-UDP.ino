/* Daikin P1P2 <---> UDP Gateway
   This Arduino program reads Daikin/Rotex P1/P2 bus using P1P2Serial library and output data over UDP and Serial.
   You can also use this program to control your Daikin/Rotex heat pump: Arduino receives commands from UDP and Serial and writes them to the P1/P2 bus.

   Copyright (c) 2019-2020 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - and Budulinek licensed under GPL v2.0 (see LICENSE)
       Arnold Niessen contributed with most of the code for Serial and P1P2 bus communication (see https://github.com/Arnold-n/P1P2Serial/tree/master/examples/P1P2Monitor),
       budulinek profided UDP communication, error reporting and configuration.
       Thanks to Luis Lamich Arocas for sharing logfiles and testing the new controlling functions for the EHVX08S23D6V,
       and to Paul Stoffregen for publishing the AltSoftSerial library.

   This program is based on the public domain Echo program from the
       Alternative Software Serial Library, v1.2
       Copyright (c) 2014 PJRC.COM, LLC, Paul Stoffregen, paul@pjrc.com
       (AltSoftSerial itself is available under the MIT license, see
        http://www.pjrc.com/teensy/td_libs_AltSoftSerial.html,
        but please note that P1P2Monitor and P1P2Serial are licensed under the GPL v2.0)

   Custom HW adapter is need for connecting to P1P2 bus (https://github.com/Arnold-n/P1P2Serial/tree/master/circuits)
   P1P2erial library is written and tested for the Arduino Uno and Mega.
   It may or may not work on other hardware using a 16MHz clock.
   As it is based on AltSoftSerial, the following pins should work
   for connecting the custom adapter to other boards:

   Board          Transmit  Receive   PWM Unusable
   -----          --------  -------   ------------
   Arduino Uno        9         8         10
   Arduino Mega      46        48       44, 45
   Teensy 3.0 & 3.1  21        20         22
   Teensy 2.0         9        10       (none)
   Teensy++ 2.0      25         4       26, 27
   Arduino Leonardo   5        13       (none)
   Wiring-S           5         6          4
   Sanguino          13        14         12
*/

#include "P1P2-UDP_Config.h"
#include <P1P2Serial.h>         // https://github.com/Arnold-n/P1P2Serial
#include <CircularBuffer.h>     // CircularBuffer https://github.com/rlogiacco/CircularBuffer
#include <TrueRandom.h>         // https://github.com/sirleech/TrueRandom
#include <EEPROM.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include "P1P2-UDP_NetConfig.h"

#ifdef DEBUG
#define dbg(x...) Serial.print(x);
#define dbgln(x...) Serial.println(x);
#else /* DEBUG */
#define dbg(x...) ;
#define dbgln(x...) ;
#endif /* DEBUG */

P1P2Serial P1P2Serial;

int8_t FxAbsentCnt[2] = { -1, -1}; // FxAbsentCnt[x] counts number of unanswered 00Fx30 messages;
// if -1 than no Fx request or response seen (relevant to detect whether F1 controller is supported or not)

#define RS_SIZE 99      // buffer to store ASCII data read from serial port and UDP, max line length on serial input is 99 (3 characters per byte, plus 'W" and '\r\n')
static char RS[RS_SIZE];
#define WB_SIZE 32       // buffer to store raw data for writing to P1P2bus, max packet size is 32 (have not seen anytyhing over 24 (23+CRC))
static byte WB[WB_SIZE];
#define RB_SIZE 33       // buffers to store raw data and error codes read from P1P2bus; 1 extra for reading back CRC byte
static byte RB[RB_SIZE];
static byte EB[RB_SIZE];

CircularBuffer<uint8_t, COMMANDS_QUEUE> cmdLen;    // length of each command
CircularBuffer<byte, COMMANDS_QUEUE_BYTES> CMD;           // queue of commands

// App will generate unique MAC address, bytes 4, 5 and 6 will hold random value
byte mac[6] = { 0x90, 0xA2, 0xDA, 0x00, 0x00, 0x00 };

#define UDP_TX_PACKET_MAX_SIZE RS_SIZE

EthernetUDP udpRecv;
EthernetUDP udpSend;

static byte crc_gen = 0xD9;    // Default generator/Feed for CRC check; these values work at least for the Daikin hybrid
static byte crc_feed = 0x00;   // Seting crc_gen to 0x00 means no CRC is checked when reading or added when writing

void error(byte errCode)
{
  Serial.print(F("EEEEEE"));
  if (errCode < 0x10) Serial.print(F("0"));
  Serial.println(errCode, HEX);
  if (Ethernet.hardwareStatus() != EthernetNoHardware) {
    udpSend.beginPacket(sendIpAddress, remPort);
#ifdef RAW_UDP
    udpSend.write(0xEE);
    udpSend.write(0xEE);
    udpSend.write(0xEE);
    udpSend.write(errCode);
#else /* RAW_UDP */
    udpSend.print(F("EEEEEE"));
    if (errCode < 0x10) udpSend.print(F("0"));
    udpSend.print(errCode, HEX);
#endif /* RAW_UDP */
    udpSend.endPacket();
  }
}

#include "P1P2-UDP_History.h"

// timers
class Timer {
  private:
    unsigned long timestampLastHitMs;
    unsigned long sleepTimeMs;
  public:
    boolean isOver();
    void start(unsigned long sleepTimeMs);
};

boolean Timer::isOver() {
  if ((unsigned long)(millis() - timestampLastHitMs) > sleepTimeMs) {
    return true;
  }
  return false;
}

void Timer::start(unsigned long sleepTimeMs) {
  this->sleepTimeMs = sleepTimeMs;
  timestampLastHitMs = millis();
}

Timer dataTimeout;
Timer dataResend;
Timer controllerTimeout;
bool controllerRetryPeriod = true;   // True if controller retry period is running
bool monitoringOnly = false;         // True if device is monitoring only, it is not acting as external controller and all commands are dropped

byte controllerId = 0x00;
byte counterrequest = 0;
bool newRS = false;                 // True if new Serial input has been read
byte rsLen = 0;    // serial read length
byte rsp = 0;      // serial read byte position
unsigned long startMillis;
#define SERIAL_TIMEOUT 5
#define SERIAL_TERMINATOR '\n'


void setup() {
  Serial.begin(115200);
  while (!Serial);      // wait for Arduino Serial Monitor to open
  // Serial.setTimeout(5); // hardcoded in timedRead above
  dbgln(F("*"));
  dbg(F("* Daikin P1P2 <---> UDP Gateway"));
  dbgln();
  dbgln("*");
  // Initial parameters
  dbg(F("* PACKET_30_DELAY="));
  dbgln(PACKET_30_DELAY);
  dbg(F("* PACKET_30_THRESHOLD="));
  dbgln(PACKET_30_THRESHOLD);
  dbg(F("* PACKET_3X_DELAY="));
  dbgln(PACKET_3X_DELAY);
  dbg(F("* PACKET_B8_DELAY="));
  dbgln(PACKET_B8_DELAY);
  dbgln(F("*"));
  P1P2Serial.begin(9600);

#ifdef ETH_RESET_PIN
  pinMode(ETH_RESET_PIN, OUTPUT);
  digitalWrite(ETH_RESET_PIN, LOW);
  delay(25);
  digitalWrite(ETH_RESET_PIN, HIGH);
  delay(500);
  pinMode(ETH_RESET_PIN, INPUT);
  delay(1000);
#endif /* ETH_RESET_PIN */

  // MAC stored in EEPROM
  if (EEPROM.read(1) == '#') {
    dbgln(F("* Reading MAC address from EEPROM"));
    for (int i = 3; i < 6; i++)
      mac[i] = EEPROM.read(i);
  }

  // MAC not set, generate random value
  else {
    dbgln(F("* Generating random MAC address"));
    for (int i = 3; i < 6; i++) {
      mac[i] = TrueRandom.randomByte();
      EEPROM.write(i, mac[i]);
    }
    EEPROM.write(1, '#');
  }

  // Log MAC address
  dbg(F("* Ethernet MAC address: "));
  for (byte i = 0; i <= 5; i++) {
    if (mac[i] < 0x10) dbg(F("0"));
    dbg(mac[i], HEX);
    if (i != 5) dbg(F(":"));
  }
  dbgln();
  dbgln(F("*"));

  Ethernet.begin(mac, ip);
  if (Ethernet.hardwareStatus() != EthernetNoHardware) {
    udpRecv.begin(listenPort);
    udpSend.begin(sendPort);
  }

  // Restart: Device restarted, possible power failure.
  error(0xFF);
  dataTimeout.start(DATA_TIMEOUT);
}

void loop() {
  recvSerial();
  processSerial();
  if (Ethernet.hardwareStatus() != EthernetNoHardware) {
    processUdp();
  }
  recvBus();
  checkTimeout();
}

// non-blocking serial read, terminates either by SERIAL_TIMEOUT or by SERIAL_TERMINATOR \n, buffer includes SERIAL_TERMINATOR
void recvSerial()
{
  while (Serial.available() > 0 && newRS == false) {
    if (rsp == 0) {
      memset(RS, 0, RS_SIZE);
    }
    if (rsp < RS_SIZE - 1) {
      RS[rsp] = Serial.read();
      rsp++;
    } else {
      Serial.read();
    }
    startMillis = millis();
  }
  if (rsp != 0 && (millis() - startMillis >= SERIAL_TIMEOUT || RS[rsp - 1] == SERIAL_TERMINATOR)) {
    rsLen = rsp;
    rsp = 0;
    newRS = true;
  }
}

void processSerial() {
  byte cmdp = 0; byte n; byte cmdtemp;
  static byte ignoreremainder = 1; // 1 is more robust to avoid misreading a partial message upon reboot, but be aware the first line received will be ignored.
  if (newRS == true) {
    newRS = false;
    if (rsLen > 0) {
      if (ignoreremainder) {
        // we should ignore this serial input block except for this check:
        // if this message ends with a '\n', we should act on next serial input
        if (RS[rsLen - 1] == '\n') ignoreremainder = 0;
      } else if (RS[rsLen - 1] != '\n') {
        ignoreremainder = 1; // message without \n, ignore next input(s)
        // Command error: Serial input line too long, interrupted or not EOL-terminated.
        error(0x30);
      } else {
        if ((--rsLen > 0) && (RS[rsLen - 1] == '\r')) rsLen--;
        RS[rsLen] = '\0';
        if (RS[0] == '\0') {
          // Command error: Empty line received.
          error(0x31);
          // check command (first byte and length)
        } else if (sscanf(&RS[0], "%2x", &cmdtemp) && (checkCommand(cmdtemp, rsLen / 2))) {
          if (monitoringOnly) {
            // Command error: Monitoring only, command dropped.
            error(0x32);
          } else if (cmdLen.available() < 1) {     // check if queue is not full
            // Memory error: Command queue full. Increase number of commands stored COMMANDS_QUEUE.
            error(0x41);
          } else if (CMD.available() < (rsLen / 2)) {
            // Memory error: Command queue full. Increase number of bytes stored COMMANDS_QUEUE_BYTES.
            error(0x42);
          } else {
            // store input in HEX format
            cmdLen.push(0);
            while ((sscanf(&RS[cmdp], "%2x%n", &cmdtemp, &n) == 1)) {
              CMD.push(cmdtemp);
              cmdLen.push(cmdLen.pop() + 1);
              cmdp += n;
            }
          }
        }
      }
    }
  }
}

void processUdp()
{
  int udpLen = udpRecv.parsePacket();
  if (udpLen) {
    byte firstByte;
#ifdef RAW_UDP
    firstByte = udpRecv.read();
#else /* RAW_UDP */
    char temp[2];
    temp[0] = udpRecv.read();
    temp[1] = udpRecv.read();
    udpLen /= 2;
    firstByte = (byte)strtol(temp, NULL, 16);
#endif /* RAW_UDP */
    if (checkCommand(firstByte, udpLen)) {   // check first byte and length
      if (monitoringOnly) {
        while (udpRecv.available()) {
          udpRecv.read();
        }
        // Command error: Monitoring only, command dropped.
        error(0x32);
      } else if (cmdLen.available() < 1) {   // check if queue is not full
        while (udpRecv.available()) {
          udpRecv.read();
        }
        // Memory error: Command queue full. Increase number of commands stored COMMANDS_QUEUE.
        error(0x41);
      } else if (CMD.available() < udpLen) {
        while (udpRecv.available()) {
          udpRecv.read();
        }
        // Memory error: Command queue full. Increase number of bytes stored COMMANDS_QUEUE_BYTES.
        error(0x42);
      } else {
        // store input in HEX format, we need to store the first byte we read earlier
        cmdLen.push(1);
        CMD.push(firstByte);
#ifdef RAW_UDP
        while (udpRecv.available()) {
          CMD.push(udpRecv.read());
          cmdLen.push(cmdLen.pop() + 1);
        }
#else /* RAW_UDP */
        while (udpRecv.available()) {
          temp[0] = udpRecv.read();
          temp[1] = udpRecv.read();
          CMD.push((byte)strtol(temp, NULL, 16));
          cmdLen.push(cmdLen.pop() + 1);
        }
#endif /* RAW_UDP */
      }
    } else {
      // checkCommand failed, clear UDP buffer
      while (udpRecv.available()) {
        udpRecv.read();
      }
    }
  }
}



bool checkCommand(byte cmd, byte n)
{
  if (cmd == 0x35) {
    // check command length for packet 0x35, max 19 bytes: 1 byte packet type + 6 * (2 bytes param number + 1 byte param value)
    switch (n) {
      case 4 :
      case 7 :
      case 10 :
      case 13 :
      case 16 :
      case 19 :
        // Command OK, will be processed in recvBus()
        return true;
        break;
      default :
        // Command error: Wrong command length.
        error(0x34);
        return false;
        break;
    }
  } else if (cmd == 0x36) {
    // check command length for packet 0x36, max 21 bytes: 1 byte packet type + 5 * (2 bytes param number + 2 bytes param value)
    switch (n) {
      case 5 :
      case 9 :
      case 13 :
      case 17 :
      case 21 :
        // Command OK, will be processed in recvBus()
        return true;
        break;
      default :
        // Command error: Wrong command length.
        error(0x34);
        return false;
        break;
    }
  } else if (cmd == 0x3A) {
    // check command length for packet 0x3A, max 19 bytes: 1 byte packet type + 6 * (2 bytes param number + 1 byte param value)
    switch (n) {
      case 4 :
      case 7 :
      case 10 :
      case 13 :
      case 16 :
      case 19 :
        // Command OK, will be processed in recvBus()
        return true;
        break;
      default :
        // Command error: Wrong command length.
        error(0x34);
        return false;
        break;
    }
  } else if (cmd == 0x10) {
    if (n == 1) {
      deletehistory();
      counterrequest = 1;
      // Command OK, but no need to store in command queue
      return false;
    } else {
      // Command error: Wrong command length.
      error(0x34);
      return false;
    }
  } else {
    // Command error: Command for unknown packet type.
    error(0x33);
    return false;
  }
}

void recvBus()
{
  while (P1P2Serial.packetavailable()) {
    // the following controller timeouts are checked only after first data are received
    if (controllerTimeout.isOver()) {
      if (controllerRetryPeriod) {
        FxAbsentCnt[0] = -1;
        FxAbsentCnt[1] = -1;
        controllerRetryPeriod = false;
        controllerTimeout.start(CONTROLLER_TIMEOUT);
      } else if (!((FxAbsentCnt[0] == PACKET_30_THRESHOLD) || (FxAbsentCnt[1] == PACKET_30_THRESHOLD))) {
        if (FxAbsentCnt[0] == -1) {
          // Connection error: External controllers not supported by the unit. Monitoring only.
          error(0x01);
        } else if (FxAbsentCnt[1] == -1) {
          // Connection error: External controller already present, second one not supported by the unit. Monitoring only.
          error(0x02);
        } else {
          // Connection error: Two external controllers already present, third one not supported by the unit. Monitoring only.
          error(0x03);
        }
        controllerRetryPeriod = true;
        controllerId = 0x00;
        controllerTimeout.start(CONTROLLER_RETRY_PERIOD);
        monitoringOnly = true;
        while (!CMD.isEmpty()) {
          // Command error: Monitoring only, all commands in queue dropped.
          error(0x32);
          deleteCommand();
        }
      }
    }
    dataTimeout.start(DATA_TIMEOUT);
    uint16_t delta;
    int nread = P1P2Serial.readpacket(RB, delta, EB, RB_SIZE, crc_gen, crc_feed);

    if (RB[1] == 0xF1) Serial.println("prd");

    if (!(EB[nread - 1] & ERROR_CRC)) {
      // message received, no error detected
      byte w;
      if ((nread > 4) && (RB[0] == 0x40) && (RB[1] == 0x00) && (RB[2] == 0x12)) {
        if (counterrequest) {
          // request one counter per cycle
          WB[0] = 0x00;
          WB[1] = 0x00;
          WB[2] = 0xB8;
          WB[3] = (counterrequest - 1);
          if (P1P2Serial.writeready()) {
            P1P2Serial.writepacket(WB, 4, PACKET_B8_DELAY, crc_gen, crc_feed);
          } else {
            // Bus write error: Refusing to write counter-request packet 0xB8 while previous packet wasn't finished
            error(0x22);
          }
          if (++counterrequest == 7) counterrequest = 0;
        }
      }
      if ((nread > 4) && (RB[0] == 0x40) && ((RB[1] & 0xFE) == 0xF0) && ((RB[2] & 0x30) == 0x30)) {
        // 40Fx30 external controller reply received, check if it really came from other external controller
        // note this could be our own (slow, delta=PACKET_30_DELAY or PACKET_3X_DELAY) reply so only reset FxAbsentCnt count if delta < min(PACKET_3X_DELAY, PACKET_30_DELAY) (- margin)
        if ((delta < PACKET_3X_DELAY - 2) && (delta < PACKET_30_DELAY - 2)) {
          // other external controller detected, reset counter
          FxAbsentCnt[RB[1] & 0x01] = 0;
          // check if we already use the same controller ID, if so then throw an error
          if (RB[1] == controllerId) {
            // Connection error: Conflict with other external controller.
            error(0x04);
            controllerRetryPeriod = true;
            controllerId = 0x00;
            controllerTimeout.start(CONTROLLER_RETRY_PERIOD);
            monitoringOnly = true;
            while (!CMD.isEmpty()) {
              // Command error: Monitoring only, command dropped.
              error(0x32);
              deleteCommand();
            }
          }
        }
      } else if ((nread > 4) && (RB[0] == 0x00) && ((RB[1] & 0xFE) == 0xF0) && ((RB[2] & 0x30) == 0x30)) {
        // 00Fx30 request message received
        if ((RB[2] == 0x30) && (FxAbsentCnt[RB[1] & 0x01] < PACKET_30_THRESHOLD)) {
          FxAbsentCnt[RB[1] & 0x01]++;
          if ((FxAbsentCnt[RB[1] & 0x01] == PACKET_30_THRESHOLD) && !controllerRetryPeriod) {
            // No external controller answering to address RB[1] detected, so we can send our own answer.
            controllerId = RB[1];
          }
        }
        // act as external controller
        if (controllerId && (FxAbsentCnt[controllerId & 0x01] == PACKET_30_THRESHOLD) && (RB[1] == controllerId)) {
          monitoringOnly = false;
          WB[0] = 0x40;
          WB[1] = RB[1];
          WB[2] = RB[2];
          int n = nread;
          if (crc_gen) n--; // omit CRC from received-byte-counter
          int d = PACKET_3X_DELAY;
          bool newWB = false;
          if (n > WB_SIZE) {
            n = WB_SIZE;
            //            dbg(F("* Surprise: received 00Fx3x packet of size "));
            //            dbgln(nread);
          }
          switch (RB[2]) {
            case 0x30 : // in: 17 byte; out: 17 byte; out pattern WB[7] should contain a 01 if we want to communicate a new setting
              for (w = 3; w < n; w++) WB[w] = 0x00;
              // set byte WB[7] to 0x01 for triggering F035 and byte WB[7] to 0x01 for triggering F036
              switch (CMD[0]) {
                case 0x35 :
                  WB[7] = 0x01;
                  break;
                case 0x36 :
                  WB[8] = 0x01;
                  break;
                case 0x3A :
                  WB[12] = 0x01;
                  break;
                default   :
                  break;
              }
              d = PACKET_30_DELAY;
              newWB = true;
              break;
            case 0x31 : // in: 15 byte; out: 15 byte; out pattern is copy of in pattern except for 2 bytes RB[7] RB[8]; function partly date/time, partly unknown
              for (w = 3; w < n; w++) WB[w] = RB[w];
              // Do not pretend to be a LAN adapter, because after unit restart it complaints about "data not in sync"
              //              WB[7] = 0x08; // product type for LAN adapter?
              //              WB[8] = 0xB4; // ??
              //              WB[9] = 0x10; // ??
              newWB = true;
              break;
            case 0x32 : // in: 19 byte: out 19 byte, out is copy in
              for (w = 3; w < n; w++) WB[w] = RB[w];
              newWB = true;
              break;
            case 0x33 : // not seen, no response
              break;
            case 0x34 : // not seen, no response
              break;
            case 0x35 : // in: 21 byte; out 21 byte; 3-byte parameters reply with FF
              for (byte i = 5; i < n; i += 3) {
                if (RB[i - 1] != 0xFF) {
                  dbg(F("35: "));
                  if (RB[i - 2] < 0x10) dbg(F("0"));
                  dbg(RB[i - 2], HEX);
                  if (RB[i - 1] < 0x10) dbg(F("0"));
                  dbg(RB[i - 1], HEX);
                  dbg(F(": "));
                  if (RB[i] < 0x10) dbg(F("0"));
                  dbgln(RB[i], HEX);
                }
              }
              for (w = 3; w < n; w++) WB[w] = 0xFF;
              if (CMD[0] == 0x35) {
                // write command
                for (byte i = 1; i < cmdLen.first(); i++) {
                  WB[i + 2] = CMD[i];
                }
                deleteCommand();
              }
              newWB = true;
              break;
            case 0x36 : // in: 23 byte; out 23 byte; 4-byte parameters; reply with FF
              for (byte i = 6; i < n; i += 4) {
                if (RB[i - 2] != 0xFF) {
                  dbg(F("36: "));
                  if (RB[i - 3] < 0x10) dbg(F("0"));
                  dbg(RB[i - 3], HEX);
                  if (RB[i - 2] < 0x10) dbg(F("0"));
                  dbg(RB[i - 2], HEX);
                  dbg(F(": "));
                  if (RB[i - 1] < 0x10) dbg(F("0"));
                  dbg(RB[i - 1], HEX);
                  if (RB[i] < 0x10) dbg(F("0"));
                  dbgln(RB[i], HEX);
                }
              }
              for (w = 3; w < n; w++) WB[w] = 0xFF;
              if (CMD[0] == 0x36) {
                // write command
                for (byte i = 1; i < cmdLen.first(); i++) {
                  WB[i + 2] = CMD[i];
                }
                deleteCommand();
              }
              newWB = true;
              break;
            // fallthrough
            case 0x37 : // in: 23 byte; out 23 byte; 5-byte parameters; reply with FF
            // not seen in EHVX08S23D6V
            // seen in EHVX08S26CB9W (value: 00001001010100001001)
            // seen in EHYHBX08AAV3 (could it be date?: 000013081F = 31 aug 2019)
            // fallthrough
            case 0x38 : // in: 21 byte; out 21 byte; 6-byte parameters; reply with FF
            // parameter range 0000-001E; kwH/hour counters?
            // not seen in EHVX08S23D6V
            // seen in EHVX08S26CB9W
            // seen in EHYHBX08AAV3
            // A parameter consists of 6 bytes: ?? bytes for param nr, and ?? bytes for value/??
            // fallthrough
            case 0x39 : // in: 21 byte; out 21 byte; 6-byte parameters; reply with FF
            // fallthrough
            case 0x3A : // in: 21 byte; out 21 byte; 3-byte parameters; reply with FF
              for (byte i = 5; i < n; i += 3) {
                if (RB[i - 1] != 0xFF) {
                  dbg(F("3A: "));
                  if (RB[i - 2] < 0x10) dbg(F("0"));
                  dbg(RB[i - 2], HEX);
                  if (RB[i - 1] < 0x10) dbg(F("0"));
                  dbg(RB[i - 1], HEX);
                  dbg(F(": "));
                  if (RB[i] < 0x10) dbg(F("0"));
                  dbgln(RB[i], HEX);
                }
              }
              for (w = 3; w < n; w++) WB[w] = 0xFF;
              if (CMD[0] == 0x3A) {
                // write command
                for (byte i = 1; i < cmdLen.first(); i++) {
                  WB[i + 2] = CMD[i];
                }
                deleteCommand();
              }
              newWB = true;
              break;
            case 0x3B : // in: 23 byte; out 23 byte; 4-byte parameters; reply with FF
            // not seen in EHVX08S23D6V
            // seen in EHVX08S26CB9W
            // seen in EHYHBX08AAV3
            // fallthrough
            case 0x3C : // in: 23 byte; out 23 byte; 5 or 10-byte parameters; reply with FF
            // fallthrough
            case 0x3D : // in: 21 byte; out: 21 byte; 6-byte parameters; reply with FF
              // parameter range 0000-001F; kwH/hour counters?
              // not seen in EHVX08S23D6V
              // seen in EHVX08S26CB9W
              // seen in EHYHBX08AAV3
              for (w = 3; w < n; w++) WB[w] = 0xFF;
              newWB = true;
              break;
            case 0x3E : // schedule related packet
              if (RB[3]) {
                // 0x3E01, 0x3E02, ... in: 23 byte; out: 23 byte; out 40F13E01(even for higher) + 19xFF
                // schedule-related
                WB[3] = 0x01;
              } else {
                // 0x3E00 in: 8 byte out: 8 byte; 40F13E00 (4x FF)
                WB[3] = 0x00;
              }
              for (w = 4; w < n; w++) WB[w] = 0xFF;
              newWB = true;
              break;
            default   : // not seen, no response
              break;
          }
          if (newWB) {
            if (P1P2Serial.writeready()) {
              P1P2Serial.writepacket(WB, n, d, crc_gen, crc_feed);
            } else {
              // Bus write error: Refusing to write packet while previous packet wasn't finished
              error(0x20);
            }
          }
        }
      }
    }

    bool readerror = 0;
    for (int i = 0; i < nread; i++) readerror |= EB[i];
    if (readerror) {
      for (int i = 0; i < nread; i++) {
        if ((EB[i] & ERROR_READBACK)) {
          // Bus write error: collision suspicion due to verification error in reading back written data
          error(0x22);
        }
        if ((EB[i] & ERROR_PE)) {
          // Bus read error: parity error detected
          error(0x10);
        }
        if ((EB[i] & ERROR_OVERRUN)) {
          // Bus read error: buffer overrun detected (overrun is after, not before, the read byte)
          error(0x11);
        }
        if ((EB[i] & ERROR_CRC)) {
          // Bus read error: CRC error detected in readpacket
          error(0x12);
        }
      }
    } else {
      // no error, print packet
      int n = nread;
      if (crc_gen) n--; // omit CRC
      if (changedPacket(RB, n)) {
        // Print only FORWARDED_PACKETS_RANGE
        if (((RB[2] >= forwardedPacketsRange[0]) && (RB[2] <= forwardedPacketsRange[1])) || (RB[2] == 0xB8)) {
          for (int i = 0; i < n; i++) {
            if (RB[i] < 0x10) Serial.print(F("0"));
            Serial.print(RB[i], HEX);
          }
          Serial.println();
          if (Ethernet.hardwareStatus() != EthernetNoHardware) {
            udpSend.beginPacket(sendIpAddress, remPort);
#ifdef RAW_UDP
            udpSend.write(RB, n);
#else /* RAW_UDP */
            for (int i = 0; i < n; i++) {
              if (RB[i] < 0x10) udpSend.print(F("0"));
              udpSend.print(RB[i], HEX);
            }
#endif /* RAW_UDP */
            udpSend.endPacket();
          }
        }
      }
      savehistory(RB, n);
    }
  }
}

void checkTimeout()
{
  if (dataTimeout.isOver()) {
    // Connection error: No data received after DATA_TIMEOUT.
    error(0x00);
    dataTimeout.start(DATA_TIMEOUT);
    monitoringOnly = true;
    while (!CMD.isEmpty()) {
      // Command error: Monitoring only, command dropped.
      error(0x32);
      deleteCommand();
    }
  }
  if (dataResend.isOver()) {
    deletehistory();
    counterrequest = 1;
    dataResend.start(DATA_RESEND_PERIOD);
  }
}

void deleteCommand()        // delete command from queue
{
  for (byte i = 0; i < cmdLen.first(); i++) {
    CMD.shift();
  }
  cmdLen.shift();
}
