#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <ESP8266WiFi.h>

class WiFiManager {
public:
    bool connectFromINI(const String& iniPath);
    bool connectFallback();
    bool startAP();
    bool isConnected();
    String getIP();
    String getSSID();
private:
    bool parseINI(const String& path, String& ssid, String& password);
};

#endif
