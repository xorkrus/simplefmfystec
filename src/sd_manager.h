#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <SdFat.h>
#include <FS.h>  // для File

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
    File openFile(const String& path, const char* mode = "r");
    bool listDirectory(const String& path, String& output);
    String getModTime(const String& path);
    size_t getFileSize(const String& path);
    // Получение миниатюры: возвращает путь к файлу изображения, если найден
    String getThumbnailPath(const String& gcodePath);
    void setCS_SENSE(int pin); // для проверки занятости шины
private:
    SdFat sd;
    bool ready = false;
    int csSensePin = CS_SENSE;
    bool isBusy();
};

#endif
