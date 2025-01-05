const byte URI_SIZE = 24;   // a smaller buffer for uri
const byte POST_SIZE = 24;  // a smaller buffer for single post parameter + key

// Actions that need to be taken after saving configuration.
enum action_type : byte {
  ACT_NONE,
  ACT_DEFAULT,       // Load default factory settings (but keep MAC address)
  ACT_MAC,           // Generate new random MAC
  ACT_REBOOT,        // Reboot the microcontroller
  ACT_RESET_ETH,     // Ethernet reset
  ACT_RESET_EEPROM,  // Reset Daikin EEPROM Writes counter
  ACT_RESET_STATS,   // Reset P1P2 Read Statistics
  ACT_CONNECT,       // Connect Controller
  ACT_DISCONNECT,    // Disconnect Controller
  ACT_CLEAR_QUOTA,   // Clear Daikin EEPROM Writes Daily Quota
  ACT_WEB            // Restart webserver
};
enum action_type action;

// Pages served by the webserver. Order of elements defines the order in the left menu of the web UI.
// URL of the page (*.htm) contains number corresponding to its position in this array.
// The following enum array can have a maximum of 10 elements (incl. PAGE_NONE and PAGE_WAIT)
enum page : byte {
  PAGE_ERROR,  // 404 Error
  PAGE_INFO,
  PAGE_STATUS,
  PAGE_IP,
  PAGE_TCP,
  PAGE_P1P2,
  PAGE_FILTER,
  PAGE_TOOLS,
  PAGE_WAIT,  // page with "Reloading. Please wait..." message.
  PAGE_DATA,  // d.json
};

// Keys for POST parameters, used in web forms and processed by processPost() function.
// Using enum ensures unique identification of each POST parameter key and consistence across functions.
// In HTML code, each element will apear as number corresponding to its position in this array.
enum post_key : byte {
  POST_NONE,  // reserved for NULL
  POST_DHCP,  // enable DHCP
  POST_MAC,
  POST_MAC_1,
  POST_MAC_2,
  POST_MAC_3,
  POST_MAC_4,
  POST_MAC_5,
  POST_IP,
  POST_IP_1,
  POST_IP_2,
  POST_IP_3,  // IP address         || Each part of an IP address has its own POST parameter.     ||
  POST_SUBNET,
  POST_SUBNET_1,
  POST_SUBNET_2,
  POST_SUBNET_3,  // subnet             || Because HTML code for IP, subnet, gateway and DNS          ||
  POST_GATEWAY,
  POST_GATEWAY_1,
  POST_GATEWAY_2,
  POST_GATEWAY_3,  // gateway            || is generated through one (nested) for-loop,                ||
  POST_DNS,
  POST_DNS_1,
  POST_DNS_2,
  POST_DNS_3,  // DNS                || all these 16 enum elements must be listed in succession!!  ||
  POST_UDP_BROADCAST,
  POST_REM_IP,
  POST_REM_IP_1,
  POST_REM_IP_2,
  POST_REM_IP_3,        // remote IP
  POST_UDP,             // local UDP port
  POST_WEB,             // web UI port
  POST_CONTROL_MODE,    // controller mode
  POST_SEND_ALL,        // send all packets
  POST_COUNTER_PERIOD,  // period for counter requests
  POST_DATA_PACKETS,    // save data packets (send only if payload changed)
  POST_TIMEOUT,         // connection timeout
  POST_QUOTA,           // write throttle
  POST_HYSTERESIS,      // temp setpoint hysteresis
  POST_CMD_TYPE,        // write command packet type
  POST_CMD_PARAM_1,     // write command parameter number
  POST_CMD_PARAM_2,     // write command parameter number
  POST_CMD_VAL_1,       // write command parameter value
  POST_CMD_VAL_2,
  POST_CMD_VAL_3,
  POST_CMD_VAL_4,
  POST_ACTION,  // actions on Tools page
};



// Keys for JSON elements, used in: 1) JSON documents, 2) ID of span tags, 3) Javascript.
enum JSON_type : byte {
  JSON_RUNTIME,             // Runtime
  JSON_DAIKIN_INDOOR,       // Daikin Indoor Unit
  JSON_DAIKIN_OUTDOOR,      // Daikin Outdoor Unit
  JSON_DATE,                // date and time
  JSON_DAIKIN_EEPROM_DATE,  // EEPROM Stats since
  JSON_DAIKIN_EEPROM,       // EEPROM Health
  JSON_WRITE_P1P2,          // write P1P2 button
  JSON_P1P2_STATS_DATE,     // P1P2 Stats since
  JSON_P1P2_STATS,          // Multiple P1P2 Read Statistics
  JSON_UDP_STATS,           // Multiple P1P2 Write Statistics
  JSON_CONTROLLER,          // Controller Mode
  JSON_LAST,                // Must be the very last element in this array
};

/**************************************************************************/
/*!
  @brief Receives GET requests for web pages, receives POST data from web forms,
  calls @ref processPost() function, sends web pages. For simplicity, all web pages
  should are numbered (1.htm, 2.htm, ...), the page number is passed to 
  the @ref sendPage() function. Also executes actions (such as ethernet restart,
  reboot) during "please wait" web page.
  @param client Ethernet TCP client.
*/
/**************************************************************************/
void recvWeb(EthernetClient &client) {
  char uri[URI_SIZE];  // the requested page
  memset(uri, 0, sizeof(uri));
  while (client.available()) {        // start reading the first line which should look like: GET /uri HTTP/1.1
    if (client.read() == ' ') break;  // find space before /uri
  }
  byte len = 0;
  while (client.available() && len < sizeof(uri) - 1) {
    char c = client.read();  // parse uri
    if (c == ' ') break;     // find space after /uri
    uri[len] = c;
    len++;
  }
  while (client.available()) {
    if (client.read() == '\r')
      if (client.read() == '\n')
        if (client.read() == '\r')
          if (client.read() == '\n')
            break;  // find 2 end of lines between header and body
  }
  if (client.available()) {
    processPost(client);  // parse post parameters
  }

  // Get the requested page from URI
  byte reqPage = PAGE_ERROR;  // requested page, 404 error is a default
  if (uri[0] == '/') {
    if (uri[1] == '\0')  // the homepage System Info
      reqPage = PAGE_INFO;
    else if (!strcmp(uri + 2, ".htm")) {
      reqPage = byte(uri[1] - 48);  // Convert single ASCII char to byte
      if (reqPage >= PAGE_WAIT) reqPage = PAGE_ERROR;
    } else if (!strcmp(uri, "/d.json")) {
      reqPage = PAGE_DATA;
    }
  }
  // Actions that require "please wait" page
  if (action == ACT_WEB || action == ACT_MAC || action == ACT_RESET_ETH || action == ACT_REBOOT || action == ACT_DEFAULT) {
    reqPage = PAGE_WAIT;
  }
  // Send page
  sendPage(client, reqPage);

  // Do all actions before the "please wait" redirects (5s delay at the moment)
  if (reqPage == PAGE_WAIT) {
    delay(500);  // wait for the wait page to load
    switch (action) {
      case ACT_WEB:
      case ACT_MAC:
      case ACT_RESET_ETH:
        for (byte s = 0; s < maxSockNum; s++) {
          // close all TCP and UDP sockets
          disconSocket(s);
        }
        startEthernet();
        break;
      case ACT_REBOOT:
      case ACT_DEFAULT:
        resetFunc();
        break;
      default:
        break;
    }
  }
  action = ACT_NONE;
}

/**************************************************************************/
/*!
  @brief Processes POST data from forms and buttons, updates data.config (in RAM)
  and saves config into EEPROM. Executes actions which do not require webserver restart
  @param client Ethernet TCP client.
*/
/**************************************************************************/
void processPost(EthernetClient &client) {
  byte command[1 + 2 + MAX_PARAM_SIZE];  // 1 byte packet type + 2 bytes param number + MAX_PARAM_SIZE bytes param value
  byte cmdLen = 0;                       // Length of the P1P2 command from WebUI
  while (client.available()) {
    char post[POST_SIZE];
    byte len = 0;
    while (client.available() && len < sizeof(post) - 1) {
      char c = client.read();
      if (c == '&') break;
      post[len] = c;
      len++;
    }
    post[len] = '\0';
    char *paramKey = post;
    char *paramValue = post;
    while (*paramValue) {
      if (*paramValue == '=') {
        paramValue++;
        break;
      }
      paramValue++;
    }
    if (*paramValue == '\0')
      continue;  // do not process POST parameter if there is no parameter value
    byte paramKeyByte = strToByte(paramKey);
    uint16_t paramValueUint = atol(paramValue);
    if (paramKey[0] == 'p') {  // POST parameter starts with 'p': this is a setting for sent packets
      setPacketStatus(strToByte(paramKey + 1), PACKET_SENT, byte(paramValueUint));
      continue;
    }

    switch (paramKeyByte) {
      case POST_NONE:  // reserved, because atoi / atol returns NULL in case of error
        break;
#ifdef ENABLE_DHCP
      case POST_DHCP:
        {
          data.config.enableDhcp = byte(paramValueUint);
        }
        break;
      case POST_DNS ... POST_DNS_3:
        {
          data.config.dns[paramKeyByte - POST_DNS] = byte(paramValueUint);
        }
        break;
#endif /* ENABLE_DHCP */
      case POST_CMD_TYPE:
        {
          command[0] = strToByte(paramValue);
        }
        break;
      case POST_CMD_PARAM_1:
        {
          command[1] = strToByte(paramValue);
        }
        break;
      case POST_CMD_PARAM_2:
        {
          command[2] = strToByte(paramValue);
        }
        break;
      case POST_CMD_VAL_1 ... POST_CMD_VAL_4:
        {
          cmdLen = 3 + paramKeyByte - POST_CMD_VAL_1 + 1;
          command[cmdLen - 1] = strToByte(paramValue);
        }
        break;
      case POST_MAC ... POST_MAC_5:
        {
          action = ACT_RESET_ETH;  // this RESET_ETH is triggered when the user changes anything on the "IP Settings" page.
                                   // No need to trigger RESET_ETH for other cases (POST_SUBNET, POST_GATEWAY etc.)
                                   // if "Randomize" button is pressed, action is set to ACT_MAC
          data.mac[paramKeyByte - POST_MAC] = strToByte(paramValue);
        }
        break;
      case POST_IP ... POST_IP_3:
        {
          data.config.ip[paramKeyByte - POST_IP] = byte(paramValueUint);
        }
        break;
      case POST_SUBNET ... POST_SUBNET_3:
        {
          data.config.subnet[paramKeyByte - POST_SUBNET] = byte(paramValueUint);
        }
        break;
      case POST_GATEWAY ... POST_GATEWAY_3:
        {
          data.config.gateway[paramKeyByte - POST_GATEWAY] = byte(paramValueUint);
        }
        break;
      case POST_REM_IP ... POST_REM_IP_3:
        {
          data.config.remoteIp[paramKeyByte - POST_REM_IP] = byte(paramValueUint);
        }
        break;
      case POST_UDP_BROADCAST:
        data.config.udpBroadcast = byte(paramValueUint);
        break;
      case POST_UDP:
        {
          if (data.config.udpPort != paramValueUint) {
            data.config.udpPort = paramValueUint;
            Udp.stop();
            Udp.begin(data.config.udpPort);
          }
        }
        break;
      case POST_WEB:
        {
          if (paramValueUint != data.config.webPort) {  // continue only if the value changed
            data.config.webPort = paramValueUint;
            action = ACT_WEB;
          }
        }
        break;
      case POST_CONTROL_MODE:
        data.config.controllerMode = byte(paramValueUint);
        break;
      case POST_TIMEOUT:
        data.config.connectTimeout = byte(paramValueUint);
        break;
      case POST_QUOTA:
        data.config.writeQuota = byte(paramValueUint);
        break;
      case POST_HYSTERESIS:
        data.config.hysteresis = byte(paramValueUint);
        break;
      case POST_SEND_ALL:
        data.config.sendAllPackets = byte(paramValueUint);
        memset(savedPackets, 0xFF, sizeof(savedPackets));  // reset saved packets whenever some setting on "Packet Filter" page changes
        break;
      case POST_COUNTER_PERIOD:
        data.config.counterPeriod = byte(paramValueUint);
        break;
      case POST_DATA_PACKETS:
        data.config.sendDataPackets = byte(paramValueUint);
        break;
      case POST_ACTION:
        action = action_type(paramValueUint);
        break;
      default:
        break;
    }
  }  // while (point != NULL)
  switch (action) {
    case ACT_DEFAULT:
      {
        data.config = DEFAULT_CONFIG;
        setPacketStatus(PACKET_TYPE_COUNTER, PACKET_SENT, true);
        for (byte i = PACKET_TYPE_DATA[FIRST]; i <= PACKET_TYPE_DATA[LAST]; i++) {
          setPacketStatus(i, PACKET_SENT, true);
        }
        break;
      }
    case ACT_MAC:
      generateMac();
      break;
    case ACT_RESET_STATS:
      resetStats();
      break;
    case ACT_RESET_EEPROM:
      resetEepromStats();
      break;
    case ACT_CONNECT:
      controllerAddr = CONNECTING;
      break;
    case ACT_DISCONNECT:
      controllerAddr = DISCONNECTED;
      break;
    case ACT_CLEAR_QUOTA:
      data.eepromDaikin.today = 0;
      data.eepromDaikin.dropped = 0;
      data.eepromDaikin.invalid = 0;
      break;
    default:
      break;
  }
  // if new P1P2 command received, put into queue
  if (cmdLen > 1) {
    for (byte i = 0; i < cmdLen; i++) {
      //Serial.println(hex(command[i]));
    }
    checkCommand(command, cmdLen);
    return;  // do not update EEPROM
  }
  if (action == ACT_CONNECT || action == ACT_DISCONNECT) return;  // do not update EEPROM
  // new parameter values received, save them to EEPROM (but do not save after Connect or Disconnect button or after manual P1P2 write)
  updateEeprom();  // it is safe to call, only changed values (and changed error and data counters) are updated
}

/**************************************************************************/
/*!
  @brief Parses string and returns single byte.
  @param myStr String (2 chars, 1 char + null or 1 null) to be parsed.
  @return Parsed byte.
*/
/**************************************************************************/
byte strToByte(const char myStr[]) {
  if (!myStr) return 0;
  byte x = 0;
  for (byte i = 0; i < 2; i++) {
    char c = myStr[i];
    if (c >= '0' && c <= '9') {
      x *= 16;
      x += c - '0';
    } else if (c >= 'A' && c <= 'F') {
      x *= 16;
      x += (c - 'A') + 10;
    } else if (c >= 'a' && c <= 'f') {
      x *= 16;
      x += (c - 'a') + 10;
    }
  }
  return x;
}

char __printbuffer[3];
/**************************************************************************/
/*!
  @brief Converts byte to char string, from https://github.com/RobTillaart/printHelpers
  @param val Byte to be conferted.
  @return Char string.
*/
/**************************************************************************/
char *hex(byte val) {
  char *buffer = __printbuffer;
  byte digits = 2;
  buffer[digits] = '\0';
  while (digits > 0) {
    byte v = val & 0x0F;
    val >>= 4;
    digits--;
    buffer[digits] = (v < 10) ? '0' + v : ('A' - 10) + v;
  }
  return buffer;
}

/**************************************************************************/
/*!
  @brief Converts date to number of days since 1.1.2000.
  @param date Date
  @return Number of days since 1.1.2000
*/
/**************************************************************************/
uint16_t days(byte *date) {
  return (date[3] * 365) + ((date[4] - 1) * 30) + date[5];
}