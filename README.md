# SimpleFM FYSETC – SD WiFi File Manager

[![Build Status](https://github.com/yourusername/simplefmfystec/actions/workflows/build.yml/badge.svg)](https://github.com/yourusername/simplefmfystec/actions/workflows/build.yml)

Прошивка для **FYSETC SD WiFi** (ESP8285) – превращает устройство в полноценный файловый менеджер для SD-карты с веб-интерфейсом.

## Возможности

- Просмотр, загрузка, скачивание, удаление, переименование, перемещение файлов и папок.
- Drag & Drop загрузка с индикатором прогресса.
- Умные миниатюры для G-кода (300x300).
- Два варианта интерфейса: десктопный и мобильный.
- Настройка WiFi через SETUP.INI на SD, fallback и собственная точка доступа.
- Кастомизация интерфейса через свои HTML-файлы на SD.

## Установка

1. Скачайте прошивку (firmware.bin) из [Releases](https://github.com/yourusername/simplefmfystec/releases).
2. Прошейте устройство через USB-UART (например, при помощи esptool или PlatformIO).
3. Вставьте SD-карту с файлом SETUP.INI (опционально).
4. Подайте питание.

## Использование

После включения устройство попытается подключиться к WiFi. Если не удастся – создаст точку доступа `sd-card-3dp` (пароль `12345678`).  
Откройте IP-адрес (выводится в Serial) в браузере и управляйте файлами.

## API

Документация API доступна в разделе [API](API.md).

## Сборка из исходников

```bash
git clone https://github.com/yourusername/simplefmfystec.git
cd simplefmfystec
pio run
