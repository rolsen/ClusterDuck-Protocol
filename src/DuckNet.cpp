#include "include/DuckNet.h"

#include <Update.h>

#include "DuckLogger.h"
#include "include/Duck.h"

#include "include/redirectToPortal.h"

DuckNet::DuckNet(Duck* duckIn):
  duck(duckIn)
{}

#ifndef CDPCFG_WIFI_NONE
IPAddress apIP(CDPCFG_AP_IP1, CDPCFG_AP_IP2, CDPCFG_AP_IP3, CDPCFG_AP_IP4);
AsyncWebServer webServer(CDPCFG_WEB_PORT);
DNSServer DuckNet::dnsServer;

const char* DuckNet::DNS = "duck";
const byte DuckNet::DNS_PORT = 53;

// Username and password for /update
const char* http_username = CDPCFG_UPDATE_USERNAME;
const char* http_password = CDPCFG_UPDATE_PASSWORD;

bool restartRequired = false;

// Apple's Captive Network Assistant tear sheet will display if a request to
// /hotspot-detect.html does not return the text "Success". Once it does return
// "Success" though, full path URLs will redirect to the browser (instead of the
// CNA tear sheet). The browser has permission to use `window.localStorage`,
// while the CNA tear sheet doesn't.
// https://captivebehavior.wballiance.com/
const char * APPLE_HOTSPOT_SUCCESS = \
  "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>";
const char * APPLE_HOTSPOT_FAILURE = \
  "<HTML><HEAD><TITLE>Failure</TITLE></HEAD><BODY>Failure</BODY></HTML>";
const unsigned long CONFIDENT_TIME_UNTIL_CNA_REFRESH = 1000 * 60 * 5;
const unsigned long TENTATIVE_TIME_UNTIL_CNA_REFRESH = 1000 * 60;

void DuckNet::setDeviceId(std::vector<byte> deviceId) {
  this->deviceId.insert(this->deviceId.end(), deviceId.begin(), deviceId.end());
}

String DuckNet::getMuidStatusMessage(muidStatus status) {
  switch (status) {
  case invalid:
    return "Invalid MUID.";
  case unrecognized:
    return "Unrecognized MUID. Please try again.";
  case not_acked:
    return "Message sent, waiting for server to acknowledge.";
  case acked:
    return "Message acknowledged.";
  default:
    const char * str = "__FILE__:__LINE__ error: Unrecognized muidStatus";
    logerr(str);
    return str;
  }
}

String DuckNet::getMuidStatusString(muidStatus status) {
  switch (status) {
  case invalid:
    return "invalid";
  case unrecognized:
    return "unrecognized";
  case not_acked:
    return "not_acked";
  case acked:
    return "acked";
  default:
    return "error";
  }
}

String DuckNet::createMuidResponseJson(muidStatus status) {
  String statusStr = getMuidStatusString(status);
  String message = getMuidStatusMessage(status);
  return "{\"status\":\"" + statusStr + "\", \"message\":\"" + message + "\"}";
}

bool DuckNet::shouldForceCaptivePortal(const ClientState & client) {
  unsigned long now = millis();
  if (now > client.expirationTime) {
    return true;
  }
  return false;
}

String plainProcessor(const String& var) {
  if (var == "THE_TEMPLATED_TITLE")
    return F("ClusterDuck");
  return String();
}

// String successProcessor(const String& var) {
//   if (var == "THE_TEMPLATED_TITLE")
//     return F("Success");
//   return String();
// }
// AwsTemplateProcessor typedSuccess = successProcessor;
AwsTemplateProcessor typedPlain = plainProcessor;

unsigned int getTentativeCnaDeadline() {
  return millis() + TENTATIVE_TIME_UNTIL_CNA_REFRESH;
}

unsigned int getConfidentCnaDeadline() {
  return millis() + CONFIDENT_TIME_UNTIL_CNA_REFRESH;
}

int DuckNet::setupWebServer(bool createCaptivePortal, String html) {
  loginfo("Setting up Web Server");

  if (html == "") {
    logdbg("Web Server using main page");
    portal = MAIN_page;
  } else {
    logdbg("Web Server using custom main page");
    portal = html;
  }
  webServer.onNotFound([&](AsyncWebServerRequest* request) {
    logwarn("DuckNet - onNotFound: " + request->host() + request->url() +
            ", getRemoteAddress: " + request->client()->getRemoteAddress() +
            ", getLocalAddress: " + request->client()->getLocalAddress());
    request->send_P(200, "text/html", REDIRECT_TO_PORTAL_TEMPLATE, typedPlain);
  });

  webServer.on("/", HTTP_GET, [&](AsyncWebServerRequest* request) {
    logdbg("Request of /");
    request->send_P(200, "text/html", REDIRECT_TO_PORTAL_TEMPLATE, typedPlain);
  });

  webServer.on("/portal", HTTP_GET, [&](AsyncWebServerRequest* request) {
    logdbg("Request of /portal");
    uint32_t clientIpAddress = request->client()->getRemoteAddress();
    clientStates[clientIpAddress].expirationTime = getConfidentCnaDeadline();
    request->send(200, "text/html", portal);
  });

  webServer.on("/hotspot-detect.html", HTTP_GET, [&](AsyncWebServerRequest* request) {

    bool connectionClose = false;
    logdbg("headers");
    int headers = request->headers();
    for(int i=0;i<headers;i++){
      AsyncWebHeader* h = request->getHeader(i);
      logdbg(h->name() + ": " + h->value());
      if (h->name().equalsIgnoreCase("Connection")
        && h->value().equalsIgnoreCase("close"))
      {
        connectionClose = true;
      }
    }
    logdbg("params");
    int paramsNumber = request->params();
    for (int i = 0; i < paramsNumber; i++) {
      AsyncWebParameter* p = request->getParam(i);
      logdbg("Param " + String(i) + " - " + p->name() + ": " + p->value());
    }



    uint32_t clientIpAddress = request->client()->getRemoteAddress();
    ClientMap::const_iterator client = clientStates.find(clientIpAddress);
    if (client == clientStates.end() || shouldForceCaptivePortal(client->second)) {
      logdbg("Returning failure for /hotspot-detect.html to force captive portal");
      clientStates[clientIpAddress].expirationTime = getTentativeCnaDeadline();
      request->send(200, "text/html", APPLE_HOTSPOT_FAILURE);
    } else {
      if (connectionClose) {
        logdbg("Faking /hotspot-detect.html success");
        request->send(200, "text/html", APPLE_HOTSPOT_SUCCESS);
      } else {
        logdbg("Providing redirect to the portal in response to /hotspot-detect.html");
        request->send_P(200, "text/html", REDIRECT_TO_PORTAL_TEMPLATE, typedPlain);
      }
    }

    clientStates[clientIpAddress].hits += 1;
  });

  // This will serve as an easy to access "control panel" to change settings of devices easily
  // TODO: Need to be able to turn off this feature from the application layer for security
  // TODO: Can we limit controls depending on the duck?
  webServer.on("/controlPanel", HTTP_GET, [&](AsyncWebServerRequest* request) {
    request->send(200, "text/html", controlPanel);
  });

  webServer.on("/flipDetector", HTTP_POST, [&](AsyncWebServerRequest* request) {
    //Run flip method
    duckutils::flipDetectState();
  });

  // Update Firmware OTA
  webServer.on("/update", HTTP_GET, [&](AsyncWebServerRequest* request) {
    if (!request->authenticate(http_username, http_password))
      return request->requestAuthentication();

    AsyncWebServerResponse* response =
    request->beginResponse(200, "text/html", update_page);

    request->send(response);
  });

  webServer.on(
    "/update", HTTP_POST,
    [&](AsyncWebServerRequest* request) {
      AsyncWebServerResponse* response = request->beginResponse(
        (Update.hasError()) ? 500 : 200, "text/plain",
        (Update.hasError()) ? "FAIL" : "OK");
      response->addHeader("Connection", "close");
      response->addHeader("Access-Control-Allow-Origin", "*");
      request->send(response);
      restartRequired = true;
    },
    [&](AsyncWebServerRequest* request, String filename, size_t index,
      uint8_t* data, size_t len, bool final)
    {
      duck->updateFirmware(filename, index, data, len, final);
      // TODO: error/exception handling
      // TODO: request->send
    });

  webServer.on("/muidStatus.json", HTTP_GET, [&](AsyncWebServerRequest* request) {
    logdbg(request->url());

    String muid;
    int paramsNumber = request->params();
    for (int i = 0; i < paramsNumber; i++) {
      AsyncWebParameter* p = request->getParam(i);
      logdbg(p->name() + ": " + p->value());
      if (p->name() == "muid") {
        muid = p->value();
      }
    }

    std::vector<byte> muidVect = {muid[0], muid[1], muid[2], muid[3]};
    muidStatus status = duck->getMuidStatus(muidVect);

    String jsonResponse = createMuidResponseJson(status);
    switch (status) {
    case invalid:
      request->send(400, "text/json", jsonResponse);
      break;
    case unrecognized:
    case not_acked:
    case acked:
      request->send(200, "text/json", jsonResponse);
      break;
    }
  });

  // Captive Portal form submission
  webServer.on("/formSubmit.json", HTTP_POST, [&](AsyncWebServerRequest* request) {
    loginfo("Submitting Form to /formSubmit.json");

    int err = DUCK_ERR_NONE;

    int paramsNumber = request->params();
    String val = "";
    String clientId = "";

    for (int i = 0; i < paramsNumber; i++) {
      AsyncWebParameter* p = request->getParam(i);
      logdbg(p->name() + ": " + p->value());

      if (p->name() == "clientId") {
        clientId = p->value();
      } else {
        val = val + p->value().c_str() + "*";
      }
    }

    clientId.toUpperCase();
    val = "[" + clientId + "]" + val;
    std::vector<byte> muid;
    err = duck->sendData(topics::cpm, val, ZERO_DUID, &muid);

    switch (err) {
      case DUCK_ERR_NONE:
      {
        String response = "{\"muid\":\"" + duckutils::toString(muid) + "\"}";
        request->send(200, "text/html", response);
        logdbg("Sent 200 response: " + response);
      }
      break;
      case DUCKLORA_ERR_MSG_TOO_LARGE:
      request->send(413, "text/html", "Message payload too big!");
      break;
      case DUCKLORA_ERR_HANDLE_PACKET:
      request->send(400, "text/html", "BadRequest");
      break;
      default:
      request->send(500, "text/html", "Oops! Unknown error.");
      break;
    }
  });

  webServer.on("/id", HTTP_GET, [&](AsyncWebServerRequest* request) {
    std::string id(deviceId.begin(), deviceId.end());
    request->send(200, "text/html", id.c_str());
  });

  webServer.on("/restart", HTTP_GET, [&](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", "Restarting...");
    delay(1000);
    duckesp::restartDuck();
  });

  webServer.on("/mac", HTTP_GET, [&](AsyncWebServerRequest* request) {
    String mac = duckesp::getDuckMacAddress(true);
    request->send(200, "text/html", mac);
  });

  webServer.on("/wifi", HTTP_GET, [&](AsyncWebServerRequest* request) {
   request->send(200, "text/html", wifi_page);
   
 });

  webServer.on("/changeSSID", HTTP_POST, [&](AsyncWebServerRequest* request) {
    int paramsNumber = request->params();
    String val = "";
    String ssid = "";
    String password = "";

    for (int i = 0; i < paramsNumber; i++) {
      AsyncWebParameter* p = request->getParam(i);

      String name = String(p->name());
      String value = String(p->value());

      if (name == "ssid") {
        ssid = String(p->value());
      } else if (name == "pass") {
        password = String(p->value());
      }
    }

    if (ssid != "" && password != "") {
      setupInternet(ssid, password);
      saveWifiCredentials(ssid, password);
      request->send(200, "text/plain", "Success");
    } else {
      request->send(500, "text/plain", "There was an error");
    }
  });

  webServer.begin();

  return DUCK_ERR_NONE;
}

int DuckNet::setupWifiAp(const char* accessPoint) {

  bool success;

  success = WiFi.mode(WIFI_AP);
  if (!success) {
    return DUCKWIFI_ERR_AP_CONFIG;
  }

  success = WiFi.softAP(accessPoint);
  if (!success) {
    return DUCKWIFI_ERR_AP_CONFIG;
  }
  //TODO: need to find out why there is a delay here
  delay(200);
  success = WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  if (!success) {
    return DUCKWIFI_ERR_AP_CONFIG;
  }

  loginfo("Created Wifi Access Point");
  return DUCK_ERR_NONE;
}

int DuckNet::setupDns() {
  bool success = dnsServer.start(DNS_PORT, "*", apIP);

  if (!success) {
    logerr("ERROR dns server start failed");
    return DUCKDNS_ERR_STARTING;
  }

  success = MDNS.begin(DNS);
  
  if (!success) {
    logerr("ERROR dns server begin failed");
    return DUCKDNS_ERR_STARTING;
  }

  loginfo("Created local DNS");
  MDNS.addService("http", "tcp", CDPCFG_WEB_PORT);

  return DUCK_ERR_NONE;
}

int DuckNet::saveWifiCredentials(String ssid, String password) {
  this->ssid = ssid;
  this->password = password;


  EEPROM.begin(512);

  if (ssid.length() > 0 && password.length() > 0) {
    loginfo("Clearing EEPROM");
    for (int i = 0; i < 96; i++) {
      EEPROM.write(i, 0);
    }

    loginfo("writing EEPROM SSID:");
    for (int i = 0; i < ssid.length(); i++)
    {
      EEPROM.write(i, ssid[i]);
      loginfo("Wrote: ");
      loginfo(ssid[i]);
    }
    loginfo("writing EEPROM Password:");
    for (int i = 0; i < password.length(); ++i)
    {
      EEPROM.write(32 + i, password[i]);
      loginfo("Wrote: ");
      loginfo(password[i]);
    }
    EEPROM.commit();
  }
  return DUCK_ERR_NONE;
}

  int DuckNet::loadWiFiCredentials(){

  // This method will look for any saved WiFi credntials on the device and set up a internet connection
  EEPROM.begin(512); //Initialasing EEPROM

  String esid;
  for (int i = 0; i < 32; ++i)
  {
    esid += char(EEPROM.read(i));
  }
  // lopp through saved SSID carachters
  loginfo("Reading EEPROM SSID: " + esid);
  setSsid(esid);

  String epass = "";
  for (int i = 32; i < 96; ++i)
  {
    epass += char(EEPROM.read(i));
  }
  // lopp through saved Password carachters
  loginfo("Reading EEPROM Password: " + epass);
  setPassword(epass);

  if (esid.length() == 0 || epass.length() == 0){
    loginfo("ERROR setupInternet: Stored SSID and PASSWORD empty");
    return DUCK_ERR_SETUP;
  } else{
    loginfo("Setup Internet with saved credentials");
    setupInternet(esid, epass);
  }
  return DUCK_ERR_NONE;
}


int DuckNet::setupInternet(String ssid, String password) {
  this->ssid = ssid;
  this->password = password;


  // Check if SSID is available
  if (!ssidAvailable(ssid)) {
    logerr("ERROR setupInternet: " + ssid + " is not available. Please check the provided ssid and/or passwords");
    return DUCK_INTERNET_ERR_SSID;
  }



  //  Connect to Access Point
  logdbg("setupInternet: connecting to WiFi access point SSID: " + ssid);
  WiFi.begin(ssid.c_str(), password.c_str());
  // We need to wait here for the connection to estanlish. Otherwise the WiFi.status() may return a false negative
  WiFi.waitForConnectResult();

  //TODO: Handle bad password better
  if(WiFi.status() != WL_CONNECTED) {
    logerr("ERROR setupInternet: failed to connect to " + ssid);
    return DUCK_INTERNET_ERR_CONNECT;
  }

  loginfo("Duck connected to internet!");

  return DUCK_ERR_NONE;

}

bool DuckNet::ssidAvailable(String val) {
  int n = WiFi.scanNetworks();
  
  if (n == 0 || ssid == "") {
    logdbg("Networks found: "+String(n));
  } else {
    logdbg("Networks found: "+String(n));
    if (val == "") {
      val = ssid;
    }
    for (int i = 0; i < n; ++i) {
      if (WiFi.SSID(i) == val.c_str()) {
        logdbg("Given ssid is available!");
        return true;
      }
      delay(AP_SCAN_INTERVAL_MS);
    }
  }
  loginfo("No ssid available");

  return false;
}

void DuckNet::setSsid(String val) { ssid = val; }

void DuckNet::setPassword(String val) { password = val; }

String DuckNet::getSsid() { return ssid; }

String DuckNet::getPassword() { return password; }

#endif
