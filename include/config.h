#ifndef CONFIG_H
#define CONFIG_H

// Пины
#define SD_CS       4
#define SD_MISO     12
#define SD_MOSI     13
#define SD_SCLK     14
#define CS_SENSE    5
#define LED_PIN     2

// Встроенные SSID/пароль как fallback
#define FALLBACK_SSID     "xopkland"
#define FALLBACK_PASSWORD "1234567890987654321"

// Настройки AP
#define AP_SSID     "sd-card-3dp"
#define AP_PASSWORD "12345678"

// Таймауты (мс)
#define WIFI_TIMEOUT 15000

// Размер буфера для чтения файлов
#define BUFFER_SIZE 4096

#endif
