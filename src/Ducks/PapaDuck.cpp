#include "../PapaDuck.h"

int PapaDuck::setupWithDefaults(std::vector<byte> deviceId, String ssid,
  String password) {
  loginfo("setupWithDefaults...");

  int err = Duck::setupWithDefaults(deviceId, ssid, password);

  if (err != DUCK_ERR_NONE) {
    logerr("ERROR setupWithDefaults rc = " + String(err));
    return err;
  }

  err = setupRadio();
  if (err != DUCK_ERR_NONE) {
    logerr("ERROR setupWithDefaults  rc = " + String(err));
    return err;
  }

  err = setupWifi("PapaDuck Setup");
  if (err != DUCK_ERR_NONE) {
    logerr("ERROR setupWithDefaults  rc = " + String(err));
    return err;
  }

  err = setupDns();
  if (err != DUCK_ERR_NONE) {
    logerr("ERROR setupWithDefaults  rc = " + String(err));
    return err;
  }



  err = setupWebServer(false);
  if (err != DUCK_ERR_NONE) {
    logerr("ERROR setupWithDefaults  rc = " + String(err));
    return err;
  }

  err = setupOTA();
  if (err != DUCK_ERR_NONE) {
    logerr("ERROR setupWithDefaults  rc = " + String(err));
    return err;
  }

  if (ssid.length() != 0 && password.length() != 0) {
    err = setupInternet(ssid, password);

    if (err != DUCK_ERR_NONE) {
      logerr("ERROR setupWithDefaults  rc = " + String(err));
      return err;
    }
  } 

  if (ssid.length() == 0 && password.length() == 0) {
  // If WiFi credentials inside the INO are empty use the saved credentials
  // TODO: if the portal credentials were saved and the INO contains credentials it won't
  // take the Portal credentials on setup
    err = duckNet.loadWiFiCredentials();

    if (err != DUCK_ERR_NONE) {
      logerr("ERROR setupWithDefaults  rc = " + String(err));
     
      return err;
    }
    
    
  }



  loginfo("setupWithDefaults done");
  return DUCK_ERR_NONE;
}

void PapaDuck::run() {

  handleOtaUpdate();
  if (getReceiveFlag()) {
    duckutils::setInterrupt(false);
    setReceiveFlag(false);

    handleReceivedPacket();
    rxPacket->reset();
    
    duckutils::setInterrupt(true);
    startReceive();
  }
}

void PapaDuck::handleReceivedPacket() {

  loginfo("handleReceivedPacket() START");
  std::vector<byte> data;
  int err = duckRadio.readReceivedData(&data);

  if (err != DUCK_ERR_NONE) {
    logerr("ERROR handleReceivedPacket. Failed to get data. rc = " +
     String(err));
    return;
  }
  // ignore pings
  if (data[TOPIC_POS] == reservedTopic::ping) {
    rxPacket->reset();
    return;
  }
  // build our RX DuckPacket which holds the updated path in case the packet is relayed
  bool relay = rxPacket->prepareForRelaying(duid, data);
  if (relay) {
    logdbg("relaying:  " +
      duckutils::convertToHex(rxPacket->getBuffer().data(),
        rxPacket->getBuffer().size()));
    loginfo("invoking callback in the duck application...");
    recvDataCallback(rxPacket->getBuffer());

    const CdpPacket packet = CdpPacket(rxPacket->getBuffer());

    logdbg("Sending ack to DUID " + duckutils::toString(packet.sduid)
      + " for MUID " + duckutils::toString(packet.muid));

    // The data payload of a ack is the MUID of the received packet
    err = txPacket->prepareForSending(packet.sduid, DuckType::PAPA,
      reservedTopic::ack, packet.muid);
    if (err != DUCK_ERR_NONE) {
      logerr("ERROR handleReceivedPacket. Failed to prepare ack. Error: " +
        String(err));
      return;
    }

    err = duckRadio.sendData(txPacket->getBuffer());
    if (err != DUCK_ERR_NONE) {
      logerr("ERROR handleReceivedPacket. Failed to send ack. Error: " +
        String(err));
      return;
    }

    loginfo("handleReceivedPacket() DONE");
  }
}

int PapaDuck::reconnectWifi(String ssid, String password) {
#ifdef CDPCFG_WIFI_NONE
  logwarn("WARNING reconnectWifi skipped, device has no WiFi.");
  return DUCK_ERR_NONE;
#else
  if (!duckNet.ssidAvailable(ssid)) {
    return DUCKWIFI_ERR_NOT_AVAILABLE;
  }
  duckNet.setupInternet(ssid, password);
  duckNet.setupDns();
  if (WiFi.status() != WL_CONNECTED) {
    logerr("ERROR WiFi reconnection failed!");
    return DUCKWIFI_ERR_DISCONNECTED;
  }
  return DUCK_ERR_NONE;
#endif
}
