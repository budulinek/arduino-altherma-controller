/* *******************************************************************
   P1P2 bus read and write functions

   recvBus()
   - receives data from the P1P2 bus
   - calls other functions to parse data, write to the P1P2 bus and process errors

   processParseRead()
   - forwards P1P2 packets to UDP
   - reads some important variables (date, unit name, etc.)
   - checks for other auxiliary controllers and gets controller ID

   processErrors()
   - stores errors in counters

   processWrite()
   - writes to the P1P2 bus

   changedPacket()
   - checks whether the packet payload (received via P1P2) has changed and stores new value

   changed36Param()
   - checks whether the parameter 36 value in the command (received via UDP or web interface) changed (more than hysteresis)
   ***************************************************************** */

static byte WB[WB_SIZE];
static byte RB[RB_SIZE];
static errorbuf_t EB[RB_SIZE];

void recvBus() {
  while (P1P2Serial.packetavailable()) {
    uint16_t delta;
    errorbuf_t readError = 0;
    uint16_t nread = P1P2Serial.readpacket(RB, delta, EB, RB_SIZE, CRC_GEN, CRC_FEED);
    if (nread > RB_SIZE) {
      //  Received packet longer than RB_SIZE
      p1p2Count[P1P2_LARGE]++;
      nread = RB_SIZE;
      readError = 0xFF;
    }
    for (uint16_t i = 0; i < nread; i++) readError |= EB[i];

    if (!readError) {
      // message received, no error detected, forward to UDP and parse some info about the heat pump (name, date etc.)
      processParseRead(nread, delta);

      // act as auxiliary controller:
      if (P1P2Serial.writeready() && controllerId && (RB[0] == 0x00) && (RB[1] == controllerId)) {  // controllerId > 0 only if controllerState == CONNECTED or CONNECTING

        //if 1) the main controller sends request to our auxiliary controller 2) we are write ready => always respond
        processWrite(nread);
      }
    } else {
      processErrors(nread);
    }
  }
}


void processParseRead(uint16_t n, uint16_t delta) {
  if (CRC_GEN) n--;  // omit CRC

  // update counters and packet type status
  p1p2Count[P1P2_READ_OK]++;
  if (setPacketStatus(RB[2], PACKET_SEEN, true) == true) {
    updateEeprom();
  }

  // Send to UDP
  if (localConfig.sendAllPackets || getPacketStatus(RB[2], PACKET_SENT) == true) {
    if (changedPacket(RB, n) == true) {
      // Send packets according to settings
      IPAddress remIp = localConfig.remoteIp;
      if (localConfig.udpBroadcast) remIp = { 255, 255, 255, 255 };
      Udp.beginPacket(remIp, localConfig.udpPort);
      Udp.write(RB, n);
      Udp.endPacket();
#ifdef ENABLE_EXTRA_DIAG
      udpCount[UDP_SENT]++;
#endif /* ENABLE_EXTRA_DIAG */
    }
  }
  // Parse time and date
  if ((RB[0] == 0x00) && (RB[1] == 0x00) && (RB[2] == 0x12)) {
    if (date[1] == 23 && RB[5] == 0) {  // midnight
      eepromCount.yesterdayWrites = eepromCount.todayWrites;
      eepromCount.todayWrites = 0;
    }
    for (byte i = 0; i < 6; i++) {
      date[i] = RB[i + 4];
    }
    if (eepromCount.eepromDate[5] == 0) {
      memcpy(eepromCount.eepromDate, date, sizeof(eepromCount.eepromDate));
    }
  }
  // Parse name
  if ((RB[0] == 0x40) && (RB[1] == 0x00) && (RB[2] == PACKET_TYPE_INDOOR_NAME)) {
    for (byte i = 0; i < NAME_SIZE - 1; i++) {
      if (RB[i + 4] == 0) break;
      daikinIndoor[i] = RB[i + 4];
    }
    if (daikinIndoor[0] == '\0') daikinIndoor[0] = '-';  // if response from heat pup is empty, write '-' in order to prevent repeated requests from us
  }

#ifdef ENABLE_EXTRA_DIAG
  if ((RB[0] == 0x40) && (RB[1] == 0x00) && (RB[2] == PACKET_TYPE_OUTDOOR_NAME)) {
    for (byte i = 0; i < NAME_SIZE - 1; i++) {
      if (RB[i + 4] == 0) break;
      daikinOutdoor[i] = RB[i + 4];
    }
    if (daikinOutdoor[0] == '\0') daikinOutdoor[0] = '-';  // if response from heat pup is empty, write '-' in order to prevent repeated requests from us
  }
#endif /* ENABLE_EXTRA_DIAG */

  // check for other auxiliary controllers and get controller ID
  if (((RB[1] & 0xFE) == 0xF0) && ((RB[2] & PACKET_TYPE_HANDSHAKE) == PACKET_TYPE_HANDSHAKE)) {
    if (RB[0] == 0x40) {
      // 40Fx3x auxiliary controller reply received - note this could be our own (slow, delta=F030DELAY or F03XDELAY) reply so only reset count if delta < min(F03XDELAY, F030DELAY) (- margin)
      // Note for developers using >1 P1P2Monitor-interfaces (=to self): this detection mechanism fails if there are 2 P1P2Monitor programs (and adapters) with same delay settings on the same bus.
      // check if there is any other auxiliary controller on 0x3x
      if ((delta < F03XDELAY - 2) && (delta < F030DELAY - 2)) {
        FxAbsentCnt[RB[1] & 0x01] = 0;
        if (RB[1] == controllerId) {
          // this should only happen if another auxiliary controller is connected if/after controllerId is set, either because
          //    -controllerId is set and conflicts with auxiliary controller, or
          //    -because another auxiliary controller has been connected after controllerId has been manually set
          controllerState = NOT_SUPPORTED;
        }
      }
    } else if (RB[0] == 0x00) {
      // 00Fx3x request message received
      // check if there is no other auxiliary controller
      if ((RB[2] == PACKET_TYPE_HANDSHAKE) && (FxAbsentCnt[RB[1] & 0x01] < F0THRESHOLD) && controllerState == CONNECTING) {
        FxAbsentCnt[RB[1] & 0x01]++;
        if (FxAbsentCnt[RB[1] & 0x01] == F0THRESHOLD) {
          // No auxiliary controller answering to address 0x (RB[1], HEX)
          controllerId = RB[1];
        }
      }
    }
  }
}

void processErrors(uint16_t nread) {
  bool packetError[P1P2_LAST];
  memset(packetError, 0, sizeof(packetError));
  for (uint16_t i = 0; i < nread; i++) {
    if ((EB[i] & ERROR_SB)) {
      // collision suspicion due to data verification error in reading back written data
      packetError[P1P2_ERROR_SB] = true;
    }
    if ((EB[i] & ERROR_BE) || (EB[i] & ERROR_BC)) {  // or BE3 (duplicate code)
      // collision suspicion due to data verification error in reading back written data
      // collision suspicion due to 0 during 2nd half bit signal read back
      packetError[P1P2_ERROR_BE_BC] = true;
    }
    if ((EB[i] & ERROR_PE)) {
      // parity error detected
      packetError[P1P2_ERROR_PE] = true;
    }
    if ((EB[i] & ERROR_OR)) {
      // buffer overrun detected (overrun is after, not before, the read byte)
      packetError[P1P2_ERROR_OR] = true;
    }
    if ((EB[i] & ERROR_CRC)) {
      // CRC error detected in readpacket
      packetError[P1P2_ERROR_CRC] = true;
    }
  }
  for (byte i = 0; i < P1P2_LAST; i++) {
    if (packetError[i] == true) p1p2Count[i]++;
  }
}

void processWrite(uint16_t n) {
  //if the main controller sends request to our auxiliary controller, always respond
  WB[0] = 0x40;
  WB[1] = RB[1];
  WB[2] = RB[2];
  byte d = F03XDELAY;
  byte cmdType = 0;
  byte cmdLen = 0;
  if (cmdQueue.isEmpty() == false) {
    cmdLen = cmdQueue[0];
    cmdType = cmdQueue[1];
  }
  if (CRC_GEN) n--;  // omit CRC from received-byte-counter
  if (n > WB_SIZE) {
    n = WB_SIZE;
    // Surprise: received 00Fx3x packet of size (nread)
  }
  for (byte i = 3; i < n; i++) WB[i] = 0xFF;  // default response

  // Write command from queue
  if (cmdLen && RB[2] == cmdType) {  // second byte in queue is packet type, compare to received packet type
    if (2 + cmdLen <= n) {           // check if param size in queue is not larger than space available in packet
      if (eepromCount.todayWrites <= localConfig.writeQuota) {
        for (byte i = 0; i < cmdLen; i++) {
          WB[i + 2] = cmdQueue[i + 1];  // skip the first byte in the queue (cmdLen)
        }
        eepromCount.daikinWrites++;
        eepromCount.todayWrites++;
      } else {
        p1p2Count[P1P2_WRITE_QUOTA]++;
      }
    } else {
      // TODO error
    }
    updateEeprom();
    deleteCmd();  // delete cmd in Queue
  } else {
    switch (RB[2]) {
      case PACKET_TYPE_HANDSHAKE:  // 0x30
        {
          d = F030DELAY;
          for (byte i = 3; i < n; i++) WB[i] = 0x00;
          // 00F030 request message received, we will:
          // - reply with 000F030 response
          // - hijack every 2nd time slot to send request counters
          static bool hijack = true;  // allow hijack
          if (cmdType >= PACKET_TYPE_CONTROL[FIRST] && cmdType <= PACKET_TYPE_CONTROL[LAST]) {
            // in: 17 byte; out: 17 byte; answer WB[7] should contain a 01 if we want to communicate a new setting in packet type 3X
            // set byte WB[7] to 0x01 for triggering F035 and byte WB[8] to 0x01 for triggering F036, etc.
            byte pos = (PACKET_TYPE_HANDSHAKE + 12) - cmdType;
            if (pos >= 3 && pos < n) WB[pos] = 0x01;
            hijack = true;
          } else if (hijack == false) {
            hijack = true;
          } else if (cmdLen > 0) {  // some command is in queue
            hijack = false;
            WB[0] = 0x00;
            WB[1] = 0x00;
            n = cmdLen + 2;
            if (2 + cmdLen <= sizeof(WB)) {  // check if size in queue is not larger than space available in packet
              for (byte i = 0; i < (cmdLen - 1); i++) {
                WB[i + 2] = cmdQueue[i + 1];  // skip the first byte in the queue (cmdLen)
              }
            } else {
              n = sizeof(WB);
              // TODO error
            }
            switch (cmdType) {
              case PACKET_TYPE_COUNTER:
                if (cmdQueue[2] < 5) {
                  cmdQueue.push(cmdLen);
                  cmdQueue.push(cmdType);
                  cmdQueue.push(cmdQueue[2] + 1);
                }
                break;
              case PACKET_TYPE_INDOOR_NAME:
                indoorInQueue = false;
                break;
              case PACKET_TYPE_OUTDOOR_NAME:
                outdoorInQueue = false;
                break;
              default:
                break;
            }
            deleteCmd();  // delete cmd in Queue
          }
        }
        break;
      case 0x31:                      // in: 15 byte; out: 15 byte; out pattern is copy of in pattern except for 2 bytes RB[7] RB[8]; function partly date/time, partly unknown
        controllerState = CONNECTED;  // handshake (exchange of packet types 0x30) was successful and the main controller proceeds to identify the auxiliary controller
        connectionTimer.sleep(localConfig.connectTimeout * 1000UL);
        // RB[7] RB[8] seem to identify the auxiliary controller type;
        // Do pretend to be a LAN adapter (even though this may trigger "data not in sync" upon restart?)
        // If we don't set address, installer mode in main thermostat may become inaccessible
        for (byte i = 3; i < n; i++) WB[i] = RB[i];
        WB[7] = CTRL_ID[0];
        WB[8] = CTRL_ID[1];
        break;
      case 0x32:  // in: 19 byte: out 19 byte, out is copy in
        for (byte i = 3; i < n; i++) WB[i] = RB[i];
        break;
      case 0x33:  // not seen; reply with FF
      case 0x34:  // not seen; reply with FF
      case 0x35:  // in: 21 byte; out 21 byte; 3-byte parameters reply with FF
      case 0x36:  // in: 23 byte; out 23 byte; 2-byte parameters; reply with FF
      case 0x37:  // in: 23 byte; out 23 byte; 3-byte parameters; reply with FF
      case 0x38:  // in: 21 byte; out 21 byte; 4-byte parameters; reply with FF
      case 0x39:  // in: 21 byte; out 21 byte; 4-byte parameters; reply with FF
      case 0x3A:  // in: 21 byte; out 21 byte; 1-byte parameters reply with FF
      case 0x3B:  // in: 23 byte; out 23 byte; 2-byte parameters; reply with FF
      case 0x3C:  // in: 23 byte; out 23 byte; 3-byte parameters; reply with FF
      case 0x3D:  // in: 21 byte; out: 21 byte; 4-byte parameters; reply with FF
        break;
      case 0x3E:  // schedule related packet
        // 0x3E01, 0x3E02, ... in: 23 byte; out: 23 byte; out 40F13E01(even for higher) + 19xFF
        WB[3] = RB[3];
        break;
      default:  // not seen, reply with FF
        break;
    }
  }
  P1P2Serial.writepacket(WB, n, d, CRC_GEN, CRC_FEED);
  p1p2Count[P1P2_WRITE_OK]++;
}

bool changedPacket(byte packet[], const byte packetLen) {
  // returns true if a packet is observed for the first time
  // returns true if any byte in the payload has changed
  // the new value is saved
  //
  bool newPacket = false;
  byte pts = (packet[0] >> 6) & 0x01;
  byte pti = packet[2] - PACKET_TYPE_DATA[FIRST];
  byte bytestart = 0;
  for (byte i = 0; i <= pts; i++) {
    for (byte j = 0; j < (PACKET_TYPE_DATA[LAST] - PACKET_TYPE_DATA[FIRST] + 1); j++) {
      if (i == pts && j == pti) break;
      bytestart += PACKET_PAYLOAD_SIZE[i][j];
    }
  }
  if (packet[2] < PACKET_TYPE_DATA[FIRST] || packet[2] > PACKET_TYPE_DATA[LAST] || localConfig.saveDataPackets == false) {
    newPacket = true;
  } else if (packetLen - 3 > PACKET_PAYLOAD_SIZE[pts][pti]) {
    // Warning: packet longer than expected
    newPacket = true;
  } else {
    for (byte i = 0; i < packetLen - 3; i++) {
      byte pi2 = bytestart + i;
      if (pi2 >= SAVED_PACKETS_SIZE) {
        pi2 = 0;
        // Warning: pi2 > SAVED_PACKETS_SIZE
        return 0;
      }
      // this byte or at least some bits have been seen and saved before.
      if (savedPackets[pi2] != packet[i + 3]) {
        newPacket = true;
        savedPackets[pi2] = packet[i + 3];
      }
    }
  }
  return newPacket;
}

bool changed36Param(byte cmd[]) {
  // returns true if a parameter is received from UDP for the first time
  // returns true if change in param value is greater than hysteresis
  // the new value is saved
  //
  bool newVal = false;
  if (cmd[0] != 0x36) {
    newVal = true;
  } else if (cmd[2] != 0 || cmd[1] > MAX_36_PARAMS) {
    // TODO Warning: param number is higher than max allowed
    newVal = false;
  } else {
    byte paramNum = cmd[1];
    int16_t paramVal = (cmd[4] << 8) | cmd[3];
    int16_t storedVal = (saved36Params[paramNum][1] << 8) | saved36Params[paramNum][0];
    // this byte or at least some bits have been seen and saved before.
    if (abs(int16_t(storedVal - paramVal)) >= int16_t(localConfig.hysteresis * 10)) {
      newVal = true;
      saved36Params[paramNum][0] = cmd[3];
      saved36Params[paramNum][1] = cmd[4];
    }
  }
  return newVal;
}