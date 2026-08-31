#include "wifi_manager.h"
#include "sd_manager.h"
#include "config.h"
#include <ESP8266WiFi.h>

bool WiFiManager::parseINI(const String& path, String& ssid, String& password) {
    SDManager sd;
    if (!sd.begin()) return false;
    FsFile f = sd.openFile(path);
    if (!f) return false;
    bool inWifi = false;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.startsWith("[") && line.endsWith("]")) {
            inWifi = (line == "[WIFI]");
            continue;
        }
        if (inWifi) {
            int eq = line.indexOf('=');
            if (eq > 0) {
                String key = line.substring(0, eq);
                key.trim();
                String value = line.substring(eq + 1);
                value.trim();
                if (key == "SSID") ssid = value;
                else if (key == "PASSWORD") password = value;
            }
        }
    }
    f.close();
    return (ssid.length() > 0 && password.length() > 0);
}

bool WiFiManager::connectFromINI(const String& iniPath) {
    String ssid, password;
    if (!parseINI(iniPath, ssid, password)) return false;
    WiFi.begin(ssid.c_str(), password.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT) {
        delay(500);
    }
    return WiFi.status() == WL_CONNECTED;
}

bool WiFiManager::connectFallback() {
    WiFi.begin(FALLBACK_SSID, FALLBACK_PASSWORD);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT) {
        delay(500);
    }
    return WiFi.status() == WL_CONNECTED;
}

bool WiFiManager::startAP() {
    WiFi.mode(WIFI_AP);
    return WiFi.softAP(AP_SSID, AP_PASSWORD);
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManager::getIP() {
    if (WiFi.getMode() == WIFI_AP) {
        return WiFi.softAPIP().toString();
    } else {
        return WiFi.localIP().toString();
    }
}

String WiFiManager::getSSID() {
    return WiFi.SSID();
}
