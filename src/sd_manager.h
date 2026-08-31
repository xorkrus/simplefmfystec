#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <SdFat.h>
#include <Arduino.h>

class SDManager {
public:
    bool begin();
    bool isReady();
    bool exists(const String& path);
    bool isDirectory(const String& path);
    bool createDirectory(const String& path);
    bool removePath(const String& path);
    bool renamePath(const String& oldPath, const String& newPath);
    bool movePath(const String& src, const String& dst);
    FsFile openFile(const String& path, uint8_t oflag = O_RDONLY);
    bool listDirectory(const String& path, String& output);
    String getModTime(const String& path);
    size_t getFileSize(const String& path);
    String getThumbnailPath(const String& gcodePath);
    void setCS_SENSE(int pin);
private:
    SdFat sd;
    bool ready = false;
    int csSensePin = CS_SENSE;
    bool isBusy();
};

#endif
