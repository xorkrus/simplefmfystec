#include "sd_manager.h"
#include "config.h"
#include <SPI.h>

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
    return digitalRead(csSensePin) == LOW;
}

bool SDManager::isDirectory(const String& path) {
    if (!ready || isBusy()) return false;
    FsFile f = sd.open(path.c_str());
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
    return renamePath(src, dst);
}

FsFile SDManager::openFile(const String& path, uint8_t oflag) {
    if (!ready || isBusy()) return FsFile();
    return sd.open(path.c_str(), oflag);
}

bool SDManager::listDirectory(const String& path, String& output) {
    if (!ready || isBusy()) return false;
    FsFile dir = sd.open(path.c_str());
    if (!dir || !dir.isDirectory()) return false;
    output = "[";
    bool first = true;
    while (true) {
        FsFile entry = dir.openNextFile();
        if (!entry) break;
        if (!first) output += ",";
        first = false;
        output += "{\"name\":\"" + String(entry.name()) + "\",";
        output += "\"type\":\"" + String(entry.isDirectory() ? "dir" : "file") + "\",";
        output += "\"size\":" + String(entry.size()) + ",";
        output += "\"modified\":\"" + getModTime(entry.name()) + "\"}";
        entry.close();
    }
    output += "]";
    dir.close();
    return true;
}

String SDManager::getModTime(const String& path) {
    return "";
}

size_t SDManager::getFileSize(const String& path) {
    if (!ready || isBusy()) return 0;
    FsFile f = sd.open(path.c_str());
    if (!f) return 0;
    size_t s = f.size();
    f.close();
    return s;
}

String SDManager::getThumbnailPath(const String& gcodePath) {
    int lastDot = gcodePath.lastIndexOf('.');
    String base = (lastDot > 0) ? gcodePath.substring(0, lastDot) : gcodePath;
    String candidates[2] = {base + ".jpg", base + ".png"};
    for (int i = 0; i < 2; i++) {
        if (exists(candidates[i])) return candidates[i];
    }
    if (exists("/logo.jpg")) return "/logo.jpg";
    if (exists("/logo.png")) return "/logo.png";
    return "";
}

void SDManager::setCS_SENSE(int pin) {
    csSensePin = pin;
}
