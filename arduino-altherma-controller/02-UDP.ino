byte masks[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };

/**************************************************************************/
/*!
  @brief Receives P1P2 command via UDP, calls @ref checkCommand() function.
*/
/**************************************************************************/
void recvUdp() {
  uint16_t udpLen = Udp.parsePacket();
  if (udpLen) {
    byte command[1 + 2 + MAX_PARAM_SIZE];  // 1 byte packet type + 2 bytes param number + MAX_PARAM_SIZE bytes param value
    if (udpLen > sizeof(command) || (!data.config.udpBroadcast && Udp.remoteIP() != IPAddress(data.config.remoteIp))) {
      while (Udp.available()) Udp.read();
      // TODO error: UDP too long or wrong remote IP
      return;
    }
    Udp.read(command, sizeof(command));
    checkCommand(command, byte(udpLen));
#ifdef ENABLE_EXTENDED_WEBUI
    data.udpCnt[UDP_RECEIVED]++;
#endif /* ENABLE_EXTENDED_WEBUI */
  }
}

/**************************************************************************/
/*!
  @brief Checks P1P2 command, checks availability of queue, stores commands
  into queue or records an error.
  @param command Command received via UDP or web UI.
  @param cmdLen Command length.
*/
/**************************************************************************/
void checkCommand(byte command[], byte cmdLen) {
  // Validate packet type and parameter size
  byte packetIndex = command[0] - PACKET_TYPE_CONTROL[FIRST];
  if (command[0] < PACKET_TYPE_CONTROL[FIRST] || command[0] > PACKET_TYPE_CONTROL[LAST] || 
      PACKET_PARAM_VAL_SIZE[packetIndex] == 0 || 
      cmdLen - 3 != PACKET_PARAM_VAL_SIZE[packetIndex]) {
    data.eepromDaikin.invalid++;  // Write Command Invalid
    return;
  }
  // Check queue availability
  if (cmdQueue.available() <= cmdLen) {
    data.eepromDaikin.invalid++;  // Write Queue Full
    return;
  }
  // Check if parameter has changed
  if (!changed36Param(command)) return;
  // Push command to queue
  cmdQueue.push(cmdLen);  // First byte in queue is cmdLen
  for (byte i = 0; i < cmdLen; i++) {
    cmdQueue.push(command[i]);
  }
}

// void checkCommand(byte command[], byte cmdLen) {
//   if (cmdQueue.available() > cmdLen) {                                                               // check available space in queue
//     if (PACKET_PARAM_VAL_SIZE[command[0] - PACKET_TYPE_CONTROL[FIRST]] != 0                          // check if param size is not zero (0 = write command not supported (yet))
//         && cmdLen - 3 == (PACKET_PARAM_VAL_SIZE[command[0] - PACKET_TYPE_CONTROL[FIRST]])            // check parameter value size
//         && (command[0] >= PACKET_TYPE_CONTROL[FIRST] && command[0] <= PACKET_TYPE_CONTROL[LAST])) {  // check packet type
//       if (changed36Param(command) == true) {
//         // push to queue (incl. cmdLen)
//         cmdQueue.push(cmdLen);  // first byte in queue is cmdLen
//         for (byte i = 0; i < cmdLen; i++) {
//           cmdQueue.push(command[i]);
//         }
//       }
//     } else {
//       data.eepromDaikin.invalid++;  // Write Command Invalid
//     }
//   } else {
//     data.eepromDaikin.invalid++;  // Write Queue Full
//   }
// }

/**************************************************************************/
/*!
  @brief Deletes command from queue.
*/
/**************************************************************************/
void deleteCmd() {
  byte cmdLen = cmdQueue.first();
  for (byte i = 0; i <= cmdLen; i++) {
    cmdQueue.shift();
  }
}

/**************************************************************************/
/*!
  @brief Checks whether the packet type has specific status.
  @param packetType Packet type.
  @param status Status we are inquiring
      - @c PACKET_SEEN Packets of this type have already been detected on the bus
      - @c PACKET_SENT Packets of this type will be sent to UDP
  @return True if the packet type has specific status
*/
/**************************************************************************/
bool getPacketStatus(const byte packetType, const byte status) {
  return (data.config.packetStatus[status][packetType / 8] & masks[packetType & 7]) > 0;
}

/**************************************************************************/
/*!
  @brief Sets status for the packet type.
  @param packetType Packet type.
  @param status Status we are setting
      - @c PACKET_SEEN Packets of this type have already been detected on the bus
      - @c PACKET_SENT Packets of this type will be sent to UDP
  @param value True or false
  @return True if value changed
*/
/**************************************************************************/
bool setPacketStatus(const byte packetType, byte status, const bool value) {
  if (getPacketStatus(packetType, status) == value) return false;
  if (value == 0) {
    data.config.packetStatus[status][packetType / 8] &= ~masks[packetType & 7];
  } else {
    data.config.packetStatus[status][packetType / 8] |= masks[packetType & 7];
  }
  return true;
}
