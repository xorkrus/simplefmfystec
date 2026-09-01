// config.h
#ifndef CONFIG_H
#define CONFIG_H

// Настройки пинов
#define SD_CS       4   // Chip Select SD карты
#define SD_MISO     12  // MISO
#define SD_MOSI     13  // MOSI
#define SD_SCLK     14  // SCLK
#define CS_SENSE    5   // Определение занятости шины Marlin (HIGH = занято)
#define LED_PIN     2   // Индикаторный светодиод

// Полярность светодиода (активный уровень)
#define LED_ON      LOW   // для встроенного LED на ESP-12 (LOW = включен)
// Если используется внешний светодиод с HIGH активным, поменяйте на HIGH

// Настройки Wi-Fi fallback
#define FALLBACK_SSID     "xopkland"
#define FALLBACK_PASSWORD "1234567890987654321"

// Настройки точки доступа
#define AP_SSID           "sd-card-3dp"
#define AP_PASSWORD       "12345678"

// Параметры веб-сервера
#define HTTP_PORT         80

// Имя файла конфигурации Wi-Fi на SD
#define SETUP_INI_FILENAME "/SETUP.INI"

// Максимальный размер пути
#define MAX_PATH_LEN      256

#endif
