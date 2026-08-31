#include "sd_manager.h"
#include "config.h"
#include <SPI.h>
#include <time.h>

bool SDManager::begin() {
    pinMode(csSensePin, INPUT_PULLUP);
    if (!sd.begin(SD_CS, SPI_FULL_SPEED)) {
        ready = false;
        return false;
    }
    ready = true;
    return true;
}

bool SDManager::isReady() { return ready; }

bool SDManager::isBusy() {
    // Если CS_SENSE HIGH, предполагаем, что шина свободна (Marlin её не использует)
    // Если LOW - занята. Можно изменить логику по необходимости.
    return digitalRead(csSensePin) == LOW;
}

bool SDManager::isDirectory(const String& path) {
    if (!ready || isBusy()) return false;
    File f = sd.open(path.c_str());
    if (!f) return false;
    bool isDir = f.isDirectory();
    f.close();
    return isDir;
}

bool SDManager::exists(const String& path) {
    if (!ready || isBusy()) return false;
    return sd.exists(path.c_str());
}

bool SDManager::createDirectory(const String& path) {
    if (!ready || isBusy()) return false;
    return sd.mkdir(path.c_str());
}

bool SDManager::removePath(const String& path) {
    if (!ready || isBusy()) return false;
    if (isDirectory(path)) {
        return sd.rmdir(path.c_str());
    } else {
        return sd.remove(path.c_str());
    }
}

bool SDManager::renamePath(const String& oldPath, const String& newPath) {
    if (!ready || isBusy()) return false;
    return sd.rename(oldPath.c_str(), newPath.c_str());
}

bool SDManager::movePath(const String& src, const String& dst) {
    // Просто переименование (если разные пути, то это перемещение)
    return renamePath(src, dst);
}

File SDManager::openFile(const String& path, const char* mode) {
    if (!ready || isBusy()) return File();
    return sd.open(path.c_str(), mode);
}

bool SDManager::listDirectory(const String& path, String& output) {
    if (!ready || isBusy()) return false;
    File dir = sd.open(path.c_str());
    if (!dir || !dir.isDirectory()) return false;
    output = "[";
    bool first = true;
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;
        if (!first) output += ",";
        first = false;
        output += "{\"name\":\"" + String(entry.name()) + "\",";
        output += "\"type\":\"" + String(entry.isDirectory() ? "dir" : "file") + "\",";
        output += "\"size\":" + String(entry.size()) + ",";
        // Получить время модификации (если поддерживается)
        output += "\"modified\":\"" + getModTime(entry.name()) + "\"}";
        entry.close();
    }
    output += "]";
    dir.close();
    return true;
}

String SDManager::getModTime(const String& path) {
    // SdFat не предоставляет простого получения времени, вернём пустую строку
    return "";
}

size_t SDManager::getFileSize(const String& path) {
    if (!ready || isBusy()) return 0;
    File f = sd.open(path.c_str());
    if (!f) return 0;
    size_t s = f.size();
    f.close();
    return s;
}

String SDManager::getThumbnailPath(const String& gcodePath) {
    // Ищем в той же папке файл с тем же именем, но .jpg или .png
    int lastDot = gcodePath.lastIndexOf('.');
    String base = (lastDot > 0) ? gcodePath.substring(0, lastDot) : gcodePath;
    String candidates[2] = {base + ".jpg", base + ".png"};
    for (int i = 0; i < 2; i++) {
        if (exists(candidates[i])) return candidates[i];
    }
    // Ищем логотип в корне
    if (exists("/logo.jpg")) return "/logo.jpg";
    if (exists("/logo.png")) return "/logo.png";
    return "";  // заглушка будет отдана встроенная
}

void SDManager::setCS_SENSE(int pin) {
    csSensePin = pin;
}
