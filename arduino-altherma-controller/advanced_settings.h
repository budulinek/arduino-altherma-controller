/*  Advanced settings, extra functions and default config forDaikin P1P2 ⇔ UDP Gateway
*/

/****** ADVANCED SETTINGS ******/

const byte MAX_QUEUE_DATA = 64;                      // total length of UDP commands stored in a queue (in bytes)
const byte PACKET_TYPE_DATA[2] = { 0x10, 0x16 };     // First and last data packet type, regularly sent between heat pump and main controller
const byte PACKET_TYPE_CONTROL[2] = { 0x30, 0x3E };  // First and last control packet type, between main and auxiliary controller
const byte PACKET_TYPE_INDOOR_NAME = 0xB1;           // Heat pump indoorname packet type
const byte PACKET_TYPE_OUTDOOR_NAME = 0xA1;          // Heat pump outdoor name packet type
const byte PACKET_TYPE_COUNTER = 0xB8;               // Counters packet type
const byte PACKET_TYPE_RESTART = 0x12;
const byte RESTART_PACKET_PAYLOAD_BYTE = 12;
const byte RESTART_PACKET_BYTE = 0x20;
const byte F030DELAY = 100;  // Time delay for in ms auxiliary controller simulation, should be larger than any response of other auxiliary controllers (which is typically 25-80 ms)
const byte F03XDELAY = 50;   // Time delay for in ms auxiliary controller simulation, should preferably be a bit larger than any regular response from auxiliary controllers (which is typically 25 ms)
const byte F0THRESHOLD = 5;  // Number of 00Fx30 messages to remain unanswered before we feel safe to act as auxiliary controller
                             // Each message takes ~770ms so we can use F0THRESHOLD to set minimum and default connectTimeout

const byte DATA_PACKETS_CNT = PACKET_TYPE_DATA[LAST] - PACKET_TYPE_DATA[FIRST] + 1;
const byte CTRL_PACKETS_CNT = PACKET_TYPE_CONTROL[LAST] - PACKET_TYPE_CONTROL[FIRST] + 1;
//byte packetsrc                                    = { { 00                      }, { 40                       } };
//byte packettype                                   = { { 10,11, 12,13, 14,15, 16 }, { 10, 11, 12, 13, 14,15,16 } };
const byte PACKET_PAYLOAD_SIZE[2][DATA_PACKETS_CNT] = { { 20, 8, 15, 3, 15, 6, 16 }, { 20, 20, 20, 16, 19, 9, 9 } };  // sum is SAVED_PACKETS_SIZE = 196
const byte SAVED_PACKETS_SIZE = 196;                                                                                  // if SAVED_PACKETS_SIZE > 256, change datatype for these variables: SAVED_PACKETS_SIZE, bytestart, pi2
//byte packettype                                  = {30,31,32,33,34,35,36,37,38,39,3A,3B,3C,3D,3E }
const byte PACKET_PARAM_VAL_SIZE[CTRL_PACKETS_CNT] = { 0, 0, 0, 0, 0, 1, 2, 3, 4, 4, 1, 2, 3, 4, 0 };  // 0 = write command not supported (yet)
const byte MAX_PARAM_SIZE = 6;
const byte MAX_36_PARAMS = 0x0D;  // max number of packet type 0x36 parameters stored

// CRC settings
const byte CRC_GEN = 0xD9;   // Default generator/Feed for CRC check; these values work at least for the Daikin hybrid
const byte CRC_FEED = 0x00;  // Define CRC_GEN to 0x00 means no CRC is checked when reading or added when writing

const byte WB_SIZE = 32;          // P1/P2 write buffer size for writing to P1P2bus, max packet size is 32 (have not seen anytyhing over 24 (23+CRC))
const byte RB_SIZE = 33;          // P1/P2 read buffer size to store raw data and error codes read from P1P2bus; 1 extra for reading back CRC byte; 24 might be enough
const uint16_t INIT_SDTO = 2500;  // P1/P2 write time-out delay (ms)

// set CTRL adapter ID; if not used, installer mode becomes unavailable on main controller
const byte CTRL_ID[] = { 0xB4, 0x10 };  // LAN adapter ID in 0x31 payload bytes 7 and 8

const byte MAC_START[3] = { 0x90, 0xA2, 0xDA };  // MAC range for Gheo SA
const byte ETH_RESET_PIN = 7;                    // Ethernet shield reset pin (deals with power on reset issue on low quality ethernet shields)
const uint16_t ETH_RESET_DELAY = 500;            // Delay (ms) during Ethernet start, wait for Ethernet shield to start (reset issue on low quality ethernet shields)
const uint16_t WEB_IDLE_TIMEOUT = 400;           // Time (ms) from last client data after which webserver TCP socket could be disconnected, non-blocking.
const uint16_t TCP_DISCON_TIMEOUT = 500;         // Timeout (ms) for client DISCON socket command, non-blocking alternative to https://www.arduino.cc/reference/en/libraries/ethernet/client.setconnectiontimeout/
const uint16_t TCP_RETRANSMISSION_TIMEOUT = 50;  // Ethernet controller’s timeout (ms), blocking (see https://www.arduino.cc/reference/en/libraries/ethernet/ethernet.setretransmissiontimeout/)
const byte TCP_RETRANSMISSION_COUNT = 3;         // Number of transmission attempts the Ethernet controller will make before giving up (see https://www.arduino.cc/reference/en/libraries/ethernet/ethernet.setretransmissioncount/)
const uint16_t FETCH_INTERVAL = 2000;            // Fetch API interval (ms) for the Modbus Status webpage to renew data from JSON served by Arduino

const byte DATA_START = 96;      // Start address where config and counters are saved in EEPROM
const byte EEPROM_INTERVAL = 6;  // Interval (hours) for saving Modbus statistics to EEPROM (in order to minimize writes to EEPROM)

/****** EXTRA FUNCTIONS ******/

// these do not fit into the limited flash memory of Arduino Uno/Nano, uncomment if you have a board with more memory
// #define ENABLE_DHCP       // Enable DHCP (Auto IP settings)
// #define ENABLE_EXTRA_DIAG // Enable outdoor unit name, runtime counter and UDP statistics.

/****** DEFAULT FACTORY SETTINGS ******/

/*
  Please note that after boot, Arduino loads user settings stored in EEPROM, even if you flash new program to it!
  Arduino loads factory defaults if:
  1) User clicks "Load default settings" in WebUI (factory reset configuration, keeps MAC)
  2) VERSION_MAJOR changes (factory reset configuration AND generates new MAC)
*/
const config_t DEFAULT_CONFIG = {
  { 192, 168, 1, 254 },     // ip
  { 255, 255, 255, 0 },     // subnet
  { 192, 168, 1, 1 },       // gateway
  { 192, 168, 1, 1 },       // Dns (only used if ENABLE_DHCP)
  false,                    // enableDhcp (only used if ENABLE_DHCP)
  { 192, 168, 1, 22 },      // remoteIp
  true,                     // udpBroadcast
  503,                      // udpPort
  80,                       // webPort
  CONTROL_MANUAL,           // controllerMode
  (F0THRESHOLD * 2),        // connectTimeout
  false,                    // notSupported
  1,                        // hysteresis
  24,                       // writeQuota
  false,                    // sendAllPackets
  10,                       // counterPeriod
  DATA_CHANGE_AND_REQUEST,  // sendDataPackets
  {}                        // packetStatus
};