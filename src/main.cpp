#include <Arduino.h>
#include "config.h"
#include "sd_manager.h"
#include "wifi_manager.h"
#include "web_server.h"

SDManager sdManager;
WiFiManager wifiManager;
WebServer webServer;

void setup() {
    Serial.begin(115200);
    Serial.println("\n\nFYSETC SD WiFi File Manager v1.0");

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Инициализация SD
    if (!sdManager.begin()) {
        Serial.println("SD init failed!");
        // Мигаем LED для индикации ошибки
        while (true) {
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
            delay(500);
        }
    }
    Serial.println("SD initialized.");

    // Подключение к WiFi
    bool connected = false;
    // 1. Пробуем SETUP.INI
    if (sdManager.exists("/SETUP.INI")) {
        connected = wifiManager.connectFromINI("/SETUP.INI");
        if (connected) Serial.println("Connected via SETUP.INI");
    }
    // 2. Fallback
    if (!connected) {
        connected = wifiManager.connectFallback();
        if (connected) Serial.println("Connected via fallback");
    }
    // 3. AP
    if (!connected) {
        wifiManager.startAP();
        Serial.println("AP mode started. SSID: " + String(AP_SSID) + " PWD: " + String(AP_PASSWORD));
    }

    Serial.print("IP: ");
    Serial.println(wifiManager.getIP());

    // Запуск веб-сервера
    webServer.begin(&sdManager);
    Serial.println("Web server started.");

    digitalWrite(LED_PIN, HIGH);
}

void loop() {
    // Ничего не делаем, всё в асинхронных обработчиках
    // Можно добавить мигание LED для индикации активности
    delay(1000);
}
