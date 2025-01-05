/* Altherma UDP Controller: Monitors and controls Daikin E-Series (Altherma) heat pumps through P1/P2 bus.

  Version history
  v0.1 2020-11-30 Initial commit, save history of selected packets, settings
  v0.2 2020-12-05 Hysteresis, vertify commands sent to P1P2
  v0.3 2020-12-09 More effective and reliable writing to P1P2 bus
  v0.4 2020-12-10 Minor tweaks
  v1.0 2023-04-18 Major upgrade: web interface, store settings in EEPROM, P1P2 error counters
  v2.0 2023-08-25 Manual MAC, Daikin EEPROM write daily quota, Simplify Arduino EEPROM read / write, Tools page
  v2.1 2023-09-17 Improve advanced settings, disable DHCP renewal fallback
  v3.0 2024-02-02 Function comments. Remove "Disabled" Controller mode (only Manual; Auto),
                  improved automatic connection to the P1P2 bus, connect to any peripheral address
                  between 0xF0 to 0xFF (depends on Altherma model), show other controllers and available addresses.
  v4.0 2025-XX-XX CSS improvement, code optimization, simplify P1P2 Status page
*/

const byte VERSION[] = { 4, 0 };

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <utility/w5100.h>
#include <CircularBuffer.hpp>  // CircularBuffer https://github.com/rlogiacco/CircularBuffer
#include <EEPROM.h>
#include <StreamLib.h>  // StreamLib https://github.com/jandrassy/StreamLib
// #include <P1P2Serial.h>  // P1P2Serial https://github.com/Arnold-n/P1P2Serial
#include "src/P1P2Serial_mod/P1P2Serial_mod.h"  // modified P1P2Serial library

// these are used by CreateTrulyRandomSeed() function
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/atomic.h>


enum first_last_t : byte {
  FIRST,
  LAST
};

enum packetStatus_t : byte {
  PACKET_SEEN,  // Packet Type was detected on P1P2 bus
  PACKET_SENT,  // Packet is sent via UDP
  PACKET_LAST   // Number of status flags in this enum. Must be the last element within this enum!!
};

enum mode_t : byte {
  CONTROL_MANUAL,  // Manual Connect
  CONTROL_AUTO     // Auto Connect
};

// Data Packets
enum data_packets_t : byte {
  DATA_ALWAYS,              // Always Send (~770ms cycle)
  DATA_CHANGE_AND_REQUEST,  // If Payload Changed & At Counter Requests
  DATA_ONLY_CHANGE          // Only If Payload Changed
};

typedef struct {
  byte ip[4];
  byte subnet[4];
  byte gateway[4];
  byte dns[4];      // only used if ENABLE_DHCP
  bool enableDhcp;  // only used if ENABLE_DHCP
  byte remoteIp[4];
  bool udpBroadcast;
  uint16_t udpPort;
  uint16_t webPort;
  byte controllerMode;
  byte connectTimeout;
  byte hysteresis;
  byte writeQuota;
  bool sendAllPackets;
  byte counterPeriod;
  byte sendDataPackets;
  byte packetStatus[PACKET_LAST][256 / 8];
} config_t;

#include "advanced_settings.h"

// default values for the web UI
const config_t DEFAULT_CONFIG = {
  DEFAULT_STATIC_IP,
  DEFAULT_SUBMASK,
  DEFAULT_GATEWAY,
  DEFAULT_DNS,
  DEFAULT_AUTO_IP,
  DEFAULT_REMOTE_IP,
  DEFAULT_BROADCAST,
  DEFAULT_UDP_PORT,
  DEFAULT_WEB_PORT,
  DEFAULT_COTROLLER_MODE,
  (F0THRESHOLD * 2),  // connectTimeout
  DEFAUT_TEMPERATURE_HYSTERESIS,
  DEFAULT_EEPROM_QUOTA,
  DEFAULT_SEND_ALL,
  DEFAULT_COUNTER_PERIOD,
  DEFAULT_DATA_PACKETS_MODE,  // sendDataPackets
  {}                          // packetStatus
};

enum p1p2_Error : byte {
  P1P2_READ_OK,      // Read OK
  P1P2_READ_ERROR,   // Read Error
  P1P2_WRITE_OK,     // Write OK
  P1P2_WRITE_ERROR,  // Write Error
  P1P2_LAST          // Number of status flags in this enum. Must be the last element within this enum!!
};

enum udp_Error : byte {
  UDP_SENT,      // Sent to UDP
  UDP_RECEIVED,  // Received from UDP
  UDP_LAST       // Number of status flags in this enum. Must be the last element within this enum!!
};

typedef struct {
  uint32_t total;      // Number of commands written to Daikin EEPROM
  byte date[6];        // Time and date when Daikin EEPROM write cycles counter started
  uint32_t dropped;    // Number of commands dropped
  uint32_t invalid;    // Number of commands invalid
  uint16_t today;      // Number of commands written today
  uint16_t yesterday;  // Number of commands written yesterday
} eeprom_t;

typedef struct {
  uint32_t eepromWrites;  // Number of Arduino EEPROM write cycles
  eeprom_t eepromDaikin;
  byte major;                   // major version
  byte mac[6];                  // MAC Address (initial value is random generated)
  config_t config;              // configuration values
  byte statsDate[6];            // Time and date when stats counter started
  uint32_t p1p2Cnt[P1P2_LAST];  // array for storing P1P2 counters
#ifdef ENABLE_EXTENDED_WEBUI
  uint32_t udpCnt[UDP_LAST];  // array for storing UDP counters
#endif                        /* ENABLE_EXTENDED_WEBUI */
} data_t;

data_t data;

CircularBuffer<byte, MAX_QUEUE_DATA> cmdQueue;  // queue of write commands


/****** ETHERNET AND P1P2 SERIAL ******/

byte maxSockNum = MAX_SOCK_NUM;

#ifdef ENABLE_DHCP
bool dhcpSuccess = false;
#endif /* ENABLE_DHCP */

EthernetUDP Udp;
EthernetServer webServer(DEFAULT_CONFIG.webPort);

#define SPI_CLK_PIN_VALUE (PINB & 0x20)

P1P2Serial P1P2Serial;
static byte hwID = 0;

/****** TIMERS AND STATE MACHINE ******/

class Timer {
private:
  uint32_t timestampLastHitMs;
  uint32_t sleepTimeMs;
public:
  boolean isOver();
  void sleep(uint32_t sleepTimeMs);
};
boolean Timer::isOver() {
  if (uint32_t(millis() - timestampLastHitMs) > sleepTimeMs) {
    return true;
  }
  return false;
}
void Timer::sleep(uint32_t sleepTimeMs) {
  this->sleepTimeMs = sleepTimeMs;
  timestampLastHitMs = millis();
}

Timer eepromTimer;          // timer to delay writing statistics to EEPROM
Timer connectionTimer;      // timer to monitor connection status (connection to write to bus)
Timer p1p2Timer;            // timer to monitor P1P2 messages (reading from bus)
Timer counterRequestTimer;  // timer for 0xB8 counter requests

enum state : byte {
  DISCONNECTED,
  CONNECTING,
};

/*!
    @brief Status and peripheral address for the Arduino controller
    @return Status of the Arduino controller:
        - @c DISCONNECTED controller disconnected
        - @c CONNECTING controller connecting to first available address
        - @c Fx controller connected with address Fx
*/
byte controllerAddr = DISCONNECTED;

/*!
    @brief Counts number of unanswered 00Fx30 requests, supports up to 16 addresses from F0 to FF
    @return Status of each address:
        - FxRequests[x] == 0 no 00Fx30 request was made (Fx address not supported by the pump)
        - FxRequests[x] < 0 other device is connected with Fx address
        - FxRequests[x] > 0 number of unasnwered requests for Fx address
*/
static int8_t FxRequests[16];

/****** RUN TIME AND DATA COUNTERS ******/

byte savedPackets[SAVED_PACKETS_SIZE] = {};
byte saved36Params[MAX_36_PARAMS + 1][2];  // storage for 0x36 parameters, param value is s16 (2 bytes)
const byte PACKET_TYPE_HANDSHAKE = PACKET_TYPE_CONTROL[FIRST];

const byte NAME_SIZE = 16;  // buffer size for device name
char daikinIndoor[NAME_SIZE];
bool indoorInQueue;   // request for an indoor name in queue
bool outdoorInQueue;  // request for an outdoor name in queue

#ifdef ENABLE_EXTENDED_WEBUI
char daikinOutdoor[NAME_SIZE];
#endif /* ENABLE_EXTENDED_WEBUI */

volatile uint32_t seed1;  // seed1 is generated by CreateTrulyRandomSeed()
volatile int8_t nrot;
uint32_t seed2 = 17111989;  // seed2 is static

byte date[6];  // Date and time from Daikin Unit

#ifdef ENABLE_EXTENDED_WEBUI
// store uptime seconds (includes seconds counted before millis() overflow)
uint32_t seconds;
// store last millis() so that we can detect millis() overflow
uint32_t last_milliseconds = 0;
// store seconds passed until the moment of the overflow so that we can add them to "seconds" on the next call
int32_t remaining_seconds;
#endif /* ENABLE_EXTENDED_WEBUI */

/****** SETUP: RUNS ONCE ******/

void setup() {
  CreateTrulyRandomSeed();
  EEPROM.get(DATA_START, data);
  // is configuration already stored in EEPROM?
  if (data.major != VERSION[0]) {
    data.major = VERSION[0];
    // load default configuration from flash memory
    data.config = DEFAULT_CONFIG;
    // Send data packets (0x10-0x16) and counter packet (0xB8) by default
    setPacketStatus(PACKET_TYPE_COUNTER, PACKET_SENT, true);
    for (byte i = PACKET_TYPE_DATA[FIRST]; i <= PACKET_TYPE_DATA[LAST]; i++) {
      setPacketStatus(i, PACKET_SENT, true);
    }
    generateMac();  // generate new MAC (bytes 0, 1 and 2 are static, bytes 3, 4 and 5 are generated randomly)
    resetStats();   // resets all counters to 0
    updateEeprom();
  }
  startEthernet();

  memset(savedPackets, 0xFF, sizeof(savedPackets));  // initial value for all saved packets is 0xFF

  hwID = SPI_CLK_PIN_VALUE ? 0 : 1;
  P1P2Serial.begin(9600, hwID ? true : false, 6, 7);  // if hwID = 1, use ADC6 and ADC7
  P1P2Serial.setEcho(true);                           // defines whether written data is read back and verified against written data (advise to keep this 1)
  P1P2Serial.setDelayTimeout(INIT_SDTO);

  connectionTimer.sleep(data.config.connectTimeout * 1000UL);
  eepromTimer.sleep(EEPROM_INTERVAL * 60UL * 60UL * 1000UL);  // EEPROM_INTERVAL is in hours, sleep is in milliseconds!
}

void loop() {

  recvBus();
  recvUdp();
  manageSockets();

  manageController();

  if (rollover()) {
    resetStats();
    updateEeprom();
  }

  if (EEPROM_INTERVAL > 0 && eepromTimer.isOver() == true) {
    updateEeprom();
  }

#ifdef ENABLE_EXTENDED_WEBUI
  maintainUptime();  // maintain uptime in case of millis() overflow
#endif               /* ENABLE_EXTENDED_WEBUI */
#ifdef ENABLE_DHCP
  Ethernet.maintain();
#endif /* ENABLE_DHCP */
}
