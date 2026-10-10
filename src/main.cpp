#include <wups.h>
#include <wups/storage.h>
#include <wups/config/WUPSConfigCategory.h>
#include <wups/config/WUPSConfigItemStub.h>
#include <wups/config/WUPSConfigItemMultipleValues.h>
#include <wups/config_api.h>
#include <wups/button_combo/api.h>
#include <coreinit/title.h>
#include <nn/acp.h>
#include <coreinit/cache.h>
#include <sysapp/launch.h>
#include <vpad/input.h>
#include <notifications/notifications.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <atomic>
#include <sys/stat.h>
#include <dirent.h>

WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("recent_apps_history");

WUPS_PLUGIN_NAME("WiiUKioskRecentApps")
WUPS_PLUGIN_DESCRIPTION("Tracks recent titles and displays them in an on-screen overlay.");
WUPS_PLUGIN_VERSION("v0.6-alpha");
WUPS_PLUGIN_AUTHOR("Developer");
WUPS_PLUGIN_LICENSE("GPL-3.0");

namespace {
constexpr uint64_t kHomeMenuUsa = 0x0005001010040100ULL;
constexpr uint64_t kHomeMenuEur = 0x0005001010040200ULL;
constexpr uint64_t kHomeMenuJpn = 0x0005001010040000ULL;
constexpr uint64_t kHomeMenuKor = 0x0005001010040300ULL;
constexpr uint64_t kHomeMenuTwn = 0x0005001010040400ULL;
constexpr uint64_t kHomeMenuChn = 0x0005001010040500ULL;
constexpr uint64_t kHealthSafetyJpn = 0x000500101004A000ULL;
constexpr uint64_t kHealthSafetyUsa = 0x000500101004A100ULL;
constexpr uint64_t kHealthSafetyEur = 0x000500101004A200ULL;
constexpr uint64_t kHealthSafetyKor = 0x000500101004A300ULL;
constexpr uint64_t kHealthSafetyTwn = 0x000500101004A400ULL;
constexpr uint64_t kHealthSafetyChn = 0x000500101004A500ULL;
constexpr size_t kHistoryCount = 10;
constexpr char kHistoryStorageId[] = "recent_apps_history";
constexpr char kBackgroundColorStorageId[] = "background_color";
constexpr uint32_t kDefaultBackgroundColor = 0;
constexpr uint32_t kBackgroundColorCount = 51;
constexpr uint32_t kUseGlobalBackgroundColor = kBackgroundColorCount;
constexpr char kSlotBackgroundColorStoragePrefix[] = "slot_color_";

uint64_t recentApps[kHistoryCount] = {};
char recentAppNames[kHistoryCount][64] = {};
alignas(0x40) ACPMetaXml titleMetaBuffer = {};
bool acpReady = false;
WUPSButtonCombo_ComboHandle overlayComboHandle = {};
WUPSButtonCombo_ComboHandle previousComboHandle = {};
WUPSButtonCombo_ComboHandle nextComboHandle = {};
WUPSButtonCombo_ComboHandle launchComboHandle = {};
size_t selectedRecentIndex = 0;
uint32_t backgroundColorSelection = kDefaultBackgroundColor;
uint32_t slotBackgroundColorSelections[kHistoryCount] = {};
bool overlayVisible = false;
bool notificationLibraryReady = false;
constexpr size_t kHomeNotificationIndex = kHistoryCount;
constexpr size_t kHeaderNotificationIndex = kHistoryCount + 1;
NotificationModuleHandle notificationHandles[kHistoryCount + 2] = {};
char notificationStatusLine[96] = "NotificationModule: not initialized";
char historyStatusLine[96] = "History: not checked";
char titleIconStatusLine[128] = "Title icons: not checked";
alignas(0x40) uint8_t titleIconRgba[128 * 128 * 4] = {};
alignas(0x40) uint8_t titleIconSourceBgra[128 * 128 * 4] = {};

void BuildHomeMenuLine(char* buffer, size_t bufferSize) {
    if (bufferSize == 0) return;
    (void)std::snprintf(buffer, bufferSize, "%c %02u  HOME",
                        selectedRecentIndex == 0 ? '>' : ' ', 1u);
}

void BuildRecentAppTitleLine(size_t index, char* buffer, size_t bufferSize) {
    if (bufferSize == 0 || index >= kHistoryCount) return;
    (void)std::snprintf(buffer, bufferSize, "%c %02u  %.45s",
                        index + 1 == selectedRecentIndex ? '>' : ' ',
                        static_cast<unsigned>(index + 2),
                        recentAppNames[index][0] != '\0' ? recentAppNames[index] : "Unnamed title");
}

NMColor BackgroundColorForSelection(uint32_t selection) {
    switch (selection) {
        case 0: return NMColor{100, 100, 100, 255}; // Standard Notifications (native styling)
        case 1: return NMColor{24, 24, 28, 255};    // Black
        case 2: return NMColor{48, 52, 58, 255};    // Charcoal
        case 3: return NMColor{82, 86, 92, 255};    // Gray
        case 4: return NMColor{105, 125, 145, 255}; // Slate
        case 5: return NMColor{160, 168, 176, 255}; // Silver
        case 6: return NMColor{205, 212, 220, 255}; // Ghost Gray
        case 7: return NMColor{220, 35, 45, 255};   // Red
        case 8: return NMColor{235, 75, 65, 255};   // Red-Orange
        case 9: return NMColor{225, 35, 90, 255};   // Crimson
        case 10: return NMColor{245, 105, 95, 255}; // Coral
        case 11: return NMColor{250, 135, 35, 255}; // Orange
        case 12: return NMColor{245, 165, 45, 255}; // Orange-Yellow
        case 13: return NMColor{220, 105, 35, 255}; // Pumpkin
        case 14: return NMColor{245, 185, 55, 255}; // Amber
        case 15: return NMColor{245, 205, 65, 255}; // Gold
        case 16: return NMColor{245, 230, 75, 255}; // Yellow
        case 17: return NMColor{190, 225, 55, 255}; // Yellow-Green
        case 18: return NMColor{125, 205, 45, 255}; // Lime
        case 19: return NMColor{45, 175, 70, 255};  // Green
        case 20: return NMColor{35, 155, 105, 255}; // Green-Cyan
        case 21: return NMColor{95, 210, 155, 255}; // Mint
        case 22: return NMColor{35, 175, 155, 255}; // Teal
        case 23: return NMColor{45, 205, 205, 255}; // Cyan
        case 24: return NMColor{105, 220, 235, 255}; // Cyan-Blue
        case 25: return NMColor{85, 175, 235, 255}; // Sky Blue
        case 26: return NMColor{35, 125, 230, 255}; // Blue
        case 27: return NMColor{65, 90, 220, 255};  // Blue-Violet
        case 28: return NMColor{70, 105, 195, 255}; // Royal Blue
        case 29: return NMColor{95, 135, 220, 255}; // Periwinkle
        case 30: return NMColor{115, 85, 215, 255}; // Indigo
        case 31: return NMColor{150, 75, 215, 255}; // Purple
        case 32: return NMColor{175, 105, 225, 255}; // Purple-Magenta
        case 33: return NMColor{190, 95, 235, 255}; // Violet
        case 34: return NMColor{195, 165, 235, 255}; // Lavender
        case 35: return NMColor{220, 65, 190, 255}; // Magenta
        case 36: return NMColor{245, 75, 165, 255}; // Hot Pink
        case 37: return NMColor{245, 125, 185, 255}; // Pink
        case 38: return NMColor{220, 90, 125, 255}; // Rose
        case 39: return NMColor{160, 95, 55, 255};  // Brown
        case 40: return NMColor{190, 115, 65, 255}; // Chocolate
        case 41: return NMColor{205, 135, 85, 255}; // Copper
        case 42: return NMColor{220, 185, 115, 255}; // Sand
        case 43: return NMColor{240, 225, 185, 255}; // Cream
        case 44: return NMColor{75, 155, 85, 255};  // Forest Green
        case 45: return NMColor{55, 155, 205, 255}; // Ocean Blue
        case 46: return NMColor{70, 210, 255, 255}; // Neon Blue
        case 47: return NMColor{145, 235, 65, 255}; // Neon Green
        case 48: return NMColor{255, 90, 205, 255}; // Neon Pink
        case 49: return NMColor{250, 125, 90, 255}; // Sunset
        case 50: return NMColor{95, 205, 185, 255}; // Aurora
        default: return NMColor{100, 100, 100, 255};
    }
}

void ApplyNotificationBackgroundColor() {
    if (!notificationLibraryReady) return;

    // Leave NotificationModule's native styling untouched for Standard Notifications.
    // Only set a background explicitly when the global or slot setting is a custom color.
    for (size_t i = 0; i < (sizeof(notificationHandles) / sizeof(notificationHandles[0])); ++i) {
        const NotificationModuleHandle handle = notificationHandles[i];
        if (handle == 0) continue;

        const uint32_t slotSelection = i < kHistoryCount
            ? slotBackgroundColorSelections[i] : kUseGlobalBackgroundColor;
        const uint32_t selection = slotSelection == kUseGlobalBackgroundColor
            ? backgroundColorSelection : slotSelection;
        if (selection == kDefaultBackgroundColor) continue;

        (void)NotificationModule_UpdateDynamicNotificationBackgroundColor(
            handle, BackgroundColorForSelection(selection));
    }
}

void HideRecentAppsNotification();

void ShowRecentAppsNotification() {
    if (!notificationLibraryReady) {
        (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                            "NotificationModule: unavailable");
        return;
    }

    bool hasTitles = false;
    for (size_t i = 0; i < kHistoryCount; ++i) {
        if (recentApps[i] != 0) {
            hasTitles = true;
            break;
        }
    }

    // Notifications stack newest at the top. Add older titles first, then
    // newer titles, and finally the header so the visible order is natural.
    for (size_t i = kHistoryCount; i-- > 0;) {
        if (recentApps[i] == 0) continue;

        char text[96];
        BuildRecentAppTitleLine(i, text, sizeof(text));
        const NotificationModuleStatus status =
            NotificationModule_AddDynamicNotification(text, &notificationHandles[i]);
        if (status != NOTIFICATION_MODULE_RESULT_SUCCESS) {
            HideRecentAppsNotification();
            (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                                "Notification failed: %s", NotificationModule_GetStatusStr(status));
            return;
        }
    }

    char homeText[32];
    BuildHomeMenuLine(homeText, sizeof(homeText));
    const NotificationModuleStatus homeStatus = NotificationModule_AddDynamicNotification(
        homeText, &notificationHandles[kHomeNotificationIndex]);
    if (homeStatus != NOTIFICATION_MODULE_RESULT_SUCCESS) {
        HideRecentAppsNotification();
        (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                            "Notification failed: %s", NotificationModule_GetStatusStr(homeStatus));
        return;
    }

    const NotificationModuleStatus status = NotificationModule_AddDynamicNotification(
        "Recent Apps", &notificationHandles[kHeaderNotificationIndex]);
    if (status != NOTIFICATION_MODULE_RESULT_SUCCESS) {
        HideRecentAppsNotification();
        (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                            "Notification failed: %s", NotificationModule_GetStatusStr(status));
        return;
    }

    ApplyNotificationBackgroundColor();

    (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                        "NotificationModule: display requested");
}

void HideRecentAppsNotification() {
    if (!notificationLibraryReady) return;

    constexpr float kNotificationExitStaggerSeconds = 0.04f;
    bool hadNotifications = false;
    float delay = 0.0f;

    // Start at the oldest title (the bottom row) and stagger upward so the
    // stack doesn't appear to collapse all at once as the notifications fade.
    for (size_t i = kHistoryCount; i-- > 0;) {
        const NotificationModuleHandle handle = notificationHandles[i];
        if (handle == 0) continue;

        hadNotifications = true;
        const NotificationModuleStatus status =
            NotificationModule_FinishDynamicNotification(handle, delay);
        notificationHandles[i] = 0;
        if (status != NOTIFICATION_MODULE_RESULT_SUCCESS) {
            (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                                "Notification close failed: %s", NotificationModule_GetStatusStr(status));
        }
        delay += kNotificationExitStaggerSeconds;
    }

    const NotificationModuleHandle homeHandle = notificationHandles[kHomeNotificationIndex];
    if (homeHandle != 0) {
        hadNotifications = true;
        const NotificationModuleStatus status =
            NotificationModule_FinishDynamicNotification(homeHandle, delay);
        notificationHandles[kHomeNotificationIndex] = 0;
        if (status != NOTIFICATION_MODULE_RESULT_SUCCESS) {
            (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                                "Notification close failed: %s", NotificationModule_GetStatusStr(status));
        }
        delay += kNotificationExitStaggerSeconds;
    }

    // Keep the header until all rows have started to leave.
    const NotificationModuleHandle headerHandle = notificationHandles[kHeaderNotificationIndex];
    if (headerHandle != 0) {
        hadNotifications = true;
        const NotificationModuleStatus status =
            NotificationModule_FinishDynamicNotification(headerHandle, delay);
        notificationHandles[kHeaderNotificationIndex] = 0;
        if (status != NOTIFICATION_MODULE_RESULT_SUCCESS) {
            (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                                "Notification close failed: %s", NotificationModule_GetStatusStr(status));
        }
    }

    if (hadNotifications) {
        (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                            "NotificationModule: hidden");
    }
}

bool isHomeMenuTitle(uint64_t titleId) {
    return titleId == kHomeMenuUsa ||
           titleId == kHomeMenuEur ||
           titleId == kHomeMenuJpn ||
           titleId == kHomeMenuKor ||
           titleId == kHomeMenuTwn ||
           titleId == kHomeMenuChn;
}

bool isHealthAndSafetyTitle(uint64_t titleId) {
    if (titleId == kHealthSafetyUsa ||
        titleId == kHealthSafetyEur ||
        titleId == kHealthSafetyJpn ||
        titleId == kHealthSafetyKor ||
        titleId == kHealthSafetyTwn ||
        titleId == kHealthSafetyChn) {
        return true;
    }

    // Some firmware/region combinations may report a different title ID.
    // Fall back to installed title metadata so the startup hook cannot
    // accidentally promote Health and Safety into the recent-app list.
    if (acpReady) {
        alignas(0x40) ACPMetaXml metadata = {};
        if (ACPGetTitleMetaXml(titleId, &metadata) == ACP_RESULT_SUCCESS) {
            const char* names[] = {
                metadata.shortname_en,
                metadata.longname_en
            };
            for (const char* name : names) {
                if (std::strstr(name, "Health and Safety") != nullptr ||
                    std::strstr(name, "Health & Safety") != nullptr ||
                    std::strstr(name, "Health and Safety Information") != nullptr) {
                    return true;
                }
            }
        }
    }
    return false;
}


bool sdHomebrewMetaMatches(const char* metaPath, const char* appName) {
    if (appName == nullptr || appName[0] == '\0') return false;
    FILE* metaFile = std::fopen(metaPath, "r");
    if (metaFile == nullptr) return false;

    char line[256];
    bool matched = false;
    while (std::fgets(line, sizeof(line), metaFile) != nullptr) {
        // Homebrew meta.xml commonly stores the visible app name in <name>.
        char* begin = std::strstr(line, "<name>");
        if (begin == nullptr) continue;
        begin += 6;
        char* finish = std::strstr(begin, "</name>");
        if (finish == nullptr) continue;
        *finish = '\0';
        if (std::strcmp(begin, appName) == 0) {
            matched = true;
            break;
        }
    }
    std::fclose(metaFile);
    return matched;
}

bool findSdHomebrewIcon(const char* appName, char* foundPath, size_t foundPathSize) {
    DIR* appsDirectory = opendir("sd:/wiiu/apps");
    if (appsDirectory == nullptr) return false;

    struct dirent* entry;
    while ((entry = readdir(appsDirectory)) != nullptr) {
        if (entry->d_name[0] == '.') continue;

        char appDirectory[384];
        char metaPath[448];
        char iconPath[448];
        std::snprintf(appDirectory, sizeof(appDirectory), "sd:/wiiu/apps/%s", entry->d_name);
        std::snprintf(metaPath, sizeof(metaPath), "%s/meta.xml", appDirectory);

        struct stat info = {};
        if (stat(metaPath, &info) != 0 || !sdHomebrewMetaMatches(metaPath, appName)) continue;

        // Most Wii U homebrew apps use icon.png. TGA variants are accepted
        // as candidates too, but only installed-title TGA is decoded in this step.
        const char* iconNames[] = {"icon.png", "icon.tga", "iconTex.tga"};
        for (const char* iconName : iconNames) {
            std::snprintf(iconPath, sizeof(iconPath), "%s/%s", appDirectory, iconName);
            if (stat(iconPath, &info) == 0 && info.st_size > 0) {
                std::snprintf(foundPath, foundPathSize, "%s", iconPath);
                closedir(appsDirectory);
                return true;
            }
        }
    }

    closedir(appsDirectory);
    return false;
}

void inspectRecentTitleIcon(size_t index) {
    if (index >= kHistoryCount || recentApps[index] == 0) {
        std::snprintf(titleIconStatusLine, sizeof(titleIconStatusLine),
                      "Title icon: no title selected");
        return;
    }

    const uint64_t titleId = recentApps[index];
    const uint32_t titleHigh = static_cast<uint32_t>(titleId >> 32);
    const uint32_t titleLow = static_cast<uint32_t>(titleId & 0xFFFFFFFFULL);

    char candidatePaths[3][320] = {};
    std::snprintf(candidatePaths[0], sizeof(candidatePaths[0]),
                  "/vol/storage_mlc01/usr/title/%08X/%08X/meta/iconTex.tga",
                  titleHigh, titleLow);
    std::snprintf(candidatePaths[1], sizeof(candidatePaths[1]),
                  "/vol/storage_usb01/usr/title/%08X/%08X/meta/iconTex.tga",
                  titleHigh, titleLow);
    candidatePaths[2][0] = '\0';

    if (acpReady) {
        char metaDirectory[256] = {};
        const ACPResult result = ACPGetTitleMetaDir(titleId, metaDirectory,
                                                     sizeof(metaDirectory));
        if (result == ACP_RESULT_SUCCESS && metaDirectory[0] != '\0') {
            std::snprintf(candidatePaths[2], sizeof(candidatePaths[2]),
                          "%s/iconTex.tga", metaDirectory);
        }
    }

    for (const char* path : candidatePaths) {
        if (path[0] == '\0') continue;

        FILE* iconFile = std::fopen(path, "rb");
        if (iconFile == nullptr) continue;

        uint8_t header[18] = {};
        if (std::fread(header, 1, sizeof(header), iconFile) != sizeof(header)) {
            std::fclose(iconFile);
            continue;
        }

        const uint16_t width = static_cast<uint16_t>(header[12]) |
                               (static_cast<uint16_t>(header[13]) << 8);
        const uint16_t height = static_cast<uint16_t>(header[14]) |
                                (static_cast<uint16_t>(header[15]) << 8);
        const uint8_t imageType = header[2];
        const uint8_t pixelDepth = header[16];
        const uint8_t descriptor = header[17];
        const char* location = std::strstr(path, "storage_usb01") != nullptr
            ? "USB" : (std::strstr(path, "storage_mlc01") != nullptr ? "MLC" : "ACP");

        if (width != 128 || height != 128 || imageType != 2 || pixelDepth != 32 ||
            header[1] != 0) {
            std::fclose(iconFile);
            std::snprintf(titleIconStatusLine, sizeof(titleIconStatusLine),
                          "Icon %s: unsupported TGA format", location);
            return;
        }

        const uint8_t idLength = header[0];
        if (idLength != 0 && std::fseek(iconFile, idLength, SEEK_CUR) != 0) {
            std::fclose(iconFile);
            continue;
        }

        if (std::fread(titleIconSourceBgra, 1, sizeof(titleIconSourceBgra), iconFile) != sizeof(titleIconSourceBgra)) {
            std::fclose(iconFile);
            std::snprintf(titleIconStatusLine, sizeof(titleIconStatusLine),
                          "Icon %s: pixel data read failed", location);
            return;
        }
        std::fclose(iconFile);

        const bool topOrigin = (descriptor & 0x20) != 0;
        const bool rightOrigin = (descriptor & 0x10) != 0;
        for (uint32_t y = 0; y < 128; ++y) {
            for (uint32_t x = 0; x < 128; ++x) {
                const uint32_t sourceX = rightOrigin ? 127 - x : x;
                const uint32_t sourceY = topOrigin ? y : 127 - y;
                const size_t src = (static_cast<size_t>(sourceY) * 128 + sourceX) * 4;
                const size_t dst = (static_cast<size_t>(y) * 128 + x) * 4;
                titleIconRgba[dst + 0] = titleIconSourceBgra[src + 2];
                titleIconRgba[dst + 1] = titleIconSourceBgra[src + 1];
                titleIconRgba[dst + 2] = titleIconSourceBgra[src + 0];
                titleIconRgba[dst + 3] = titleIconSourceBgra[src + 3];
            }
        }

        std::snprintf(titleIconStatusLine, sizeof(titleIconStatusLine),
                      "Icon %s decoded: 128x128 RGBA", location);
        return;
    }

    char sdIconPath[448] = {};
    const char* appName = recentAppNames[index];
    if (findSdHomebrewIcon(appName, sdIconPath, sizeof(sdIconPath))) {
        if (std::strstr(sdIconPath, ".png") != nullptr) {
            std::snprintf(titleIconStatusLine, sizeof(titleIconStatusLine),
                          "SD homebrew PNG found; decoder needed");
        } else {
            std::snprintf(titleIconStatusLine, sizeof(titleIconStatusLine),
                          "SD homebrew TGA found; format check needed");
        }
        return;
    }

    std::snprintf(titleIconStatusLine, sizeof(titleIconStatusLine),
                  "No icon found for this title on MLC, USB, ACP, or SD");
}

void resolveRecentAppName(size_t index) {
    if (index >= kHistoryCount || recentApps[index] == 0) {
        if (index < kHistoryCount) recentAppNames[index][0] = '\0';
        return;
    }

    recentAppNames[index][0] = '\0';
    if (acpReady) {
        std::memset(&titleMetaBuffer, 0, sizeof(titleMetaBuffer));
        if (ACPGetTitleMetaXml(recentApps[index], &titleMetaBuffer) == ACP_RESULT_SUCCESS) {
            const char* name = titleMetaBuffer.shortname_en[0] != '\0'
                ? titleMetaBuffer.shortname_en
                : titleMetaBuffer.longname_en;
            if (name[0] != '\0') {
                std::snprintf(recentAppNames[index], sizeof(recentAppNames[index]), "%s", name);
            }
        }
    }

    // Only show the ID when title metadata has no usable name.
    if (recentAppNames[index][0] == '\0') {
        std::snprintf(recentAppNames[index], sizeof(recentAppNames[index]),
                      "%016llX", static_cast<unsigned long long>(recentApps[index]));
    }
}

void resolveAllRecentAppNames() {
    for (size_t i = 0; i < kHistoryCount; ++i) resolveRecentAppName(i);
}

void loadBackgroundColorSetting() {
    const WUPSStorageError error = WUPSStorageAPI::GetOrStoreDefault<uint32_t>(
        kBackgroundColorStorageId, backgroundColorSelection, kDefaultBackgroundColor);
    if (error != WUPS_STORAGE_ERROR_SUCCESS || backgroundColorSelection >= kBackgroundColorCount) {
        backgroundColorSelection = kDefaultBackgroundColor;
    }
}

void loadRecentApps() {
    for (size_t i = 0; i < kHistoryCount; ++i) {
        char key[16];
        std::snprintf(key, sizeof(key), "title%u", static_cast<unsigned>(i));
        const WUPSStorageError error =
            WUPSStorageAPI::GetOrStoreDefault(key, recentApps[i], uint64_t{0});
        if (error != WUPS_STORAGE_ERROR_SUCCESS) {
            (void)std::snprintf(historyStatusLine, sizeof(historyStatusLine),
                                "History load failed: %.55s",
                                WUPSStorageAPI::GetStatusStr(error));
            return;
        }
    }

    resolveAllRecentAppNames();
    inspectRecentTitleIcon(0);
    const WUPSStorageError saveError = WUPSStorageAPI::SaveStorage();
    if (saveError != WUPS_STORAGE_ERROR_SUCCESS) {
        (void)std::snprintf(historyStatusLine, sizeof(historyStatusLine),
                            "History init save failed: %.45s",
                            WUPSStorageAPI::GetStatusStr(saveError));
        return;
    }
    (void)std::snprintf(historyStatusLine, sizeof(historyStatusLine),
                        "History: loaded from plugin storage");
}

void saveRecentApps() {
    for (size_t i = 0; i < kHistoryCount; ++i) {
        char key[16];
        std::snprintf(key, sizeof(key), "title%u", static_cast<unsigned>(i));
        const WUPSStorageError error = WUPSStorageAPI::Store(key, recentApps[i]);
        if (error != WUPS_STORAGE_ERROR_SUCCESS) {
            (void)std::snprintf(historyStatusLine, sizeof(historyStatusLine),
                                "History store failed: %.52s",
                                WUPSStorageAPI::GetStatusStr(error));
            return;
        }
    }

    const WUPSStorageError saveError = WUPSStorageAPI::SaveStorage();
    if (saveError == WUPS_STORAGE_ERROR_SUCCESS) {
        (void)std::snprintf(historyStatusLine, sizeof(historyStatusLine),
                            "History: saved to plugin storage");
    } else {
        (void)std::snprintf(historyStatusLine, sizeof(historyStatusLine),
                            "History save failed: %.50s",
                            WUPSStorageAPI::GetStatusStr(saveError));
    }
}

void rememberTitle(uint64_t titleId, bool explicitlyLaunched = false) {
    if (titleId == 0 || isHomeMenuTitle(titleId)) return;
    // Some system startup flows report Health & Safety as the current title.
    // Do not let that boot-time state overwrite the persisted recent list.
    // It can still be added when the user explicitly launches it from this plugin.
    if (isHealthAndSafetyTitle(titleId) && !explicitlyLaunched) return;

    uint64_t updated[kHistoryCount] = {};
    updated[0] = titleId;
    size_t next = 1;

    for (size_t i = 0; i < kHistoryCount && next < kHistoryCount; ++i) {
        if (recentApps[i] == 0 || recentApps[i] == titleId) continue;
        updated[next++] = recentApps[i];
    }

    if (std::memcmp(recentApps, updated, sizeof(recentApps)) != 0) {
        std::memcpy(recentApps, updated, sizeof(recentApps));
        selectedRecentIndex = 0;
        resolveAllRecentAppNames();
        inspectRecentTitleIcon(0);
        saveRecentApps();
    }
}

void RecentAppsComboCallback(WUPSButtonCombo_ControllerTypes, WUPSButtonCombo_ComboHandle, void*) {
    overlayVisible = !overlayVisible;
    if (overlayVisible) {
        ShowRecentAppsNotification();
    } else {
        HideRecentAppsNotification();
    }
}

void RefreshRecentAppsNotification() {
    if (!overlayVisible || !notificationLibraryReady) return;

    for (size_t i = 0; i < kHistoryCount; ++i) {
        if (recentApps[i] == 0 || notificationHandles[i] == 0) continue;

        char text[96];
        BuildRecentAppTitleLine(i, text, sizeof(text));
        const NotificationModuleStatus status =
            NotificationModule_UpdateDynamicNotificationText(notificationHandles[i], text);
        if (status != NOTIFICATION_MODULE_RESULT_SUCCESS) {
            HideRecentAppsNotification();
            ShowRecentAppsNotification();
            (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                                "Notification update fallback: %s",
                                NotificationModule_GetStatusStr(status));
            return;
        }
    }

    if (notificationHandles[kHomeNotificationIndex] != 0) {
        char homeText[32];
        BuildHomeMenuLine(homeText, sizeof(homeText));
        const NotificationModuleStatus status =
            NotificationModule_UpdateDynamicNotificationText(notificationHandles[kHomeNotificationIndex], homeText);
        if (status != NOTIFICATION_MODULE_RESULT_SUCCESS) {
            HideRecentAppsNotification();
            ShowRecentAppsNotification();
            (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                                "Notification update fallback: %s",
                                NotificationModule_GetStatusStr(status));
        }
    }
}

void MoveRecentSelection(int direction) {
    if (!overlayVisible) return;

    const size_t menuEntryCount = kHistoryCount + 1;
    for (size_t attempt = 0; attempt < menuEntryCount; ++attempt) {
        if (direction > 0) {
            selectedRecentIndex = (selectedRecentIndex + 1) % menuEntryCount;
        } else {
            selectedRecentIndex = (selectedRecentIndex + menuEntryCount - 1) % menuEntryCount;
        }

        if (selectedRecentIndex == 0) {
            RefreshRecentAppsNotification();
            return;
        }

        const size_t titleIndex = selectedRecentIndex - 1;
        if (recentApps[titleIndex] != 0) {
            inspectRecentTitleIcon(titleIndex);
            RefreshRecentAppsNotification();
            return;
        }
    }
}

void RecentAppsPreviousCallback(WUPSButtonCombo_ControllerTypes, WUPSButtonCombo_ComboHandle, void*) {
    MoveRecentSelection(-1);
}

void RecentAppsNextCallback(WUPSButtonCombo_ControllerTypes, WUPSButtonCombo_ComboHandle, void*) {
    MoveRecentSelection(1);
}

void RecentAppsLaunchCallback(WUPSButtonCombo_ControllerTypes, WUPSButtonCombo_ComboHandle, void*) {
    if (!overlayVisible) return;

    if (selectedRecentIndex == 0) {
        HideRecentAppsNotification();
        overlayVisible = false;
        (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                            "Returning to HOME Menu");
        SYSLaunchMenu();
        return;
    }

    const uint64_t titleId = recentApps[selectedRecentIndex - 1];
    if (titleId == 0) {
        (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                            "Launch skipped: no title selected");
        return;
    }
    if (titleId == OSGetTitleID()) {
        (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                            "Selected title is already running");
        return;
    }

    rememberTitle(titleId, true);
    HideRecentAppsNotification();
    overlayVisible = false;
    (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                        "Launch requested: %016llX",
                        static_cast<unsigned long long>(titleId));
    SYSLaunchTitle(titleId);
}

void BackgroundColorChangedCallback(ConfigItemMultipleValues*, uint32_t newValue) {
    backgroundColorSelection = newValue < kBackgroundColorCount ? newValue : kDefaultBackgroundColor;
    ApplyNotificationBackgroundColor();

    const WUPSStorageError error =
        WUPSStorageAPI::Store(kBackgroundColorStorageId, backgroundColorSelection);
    if (error != WUPS_STORAGE_ERROR_SUCCESS) {
        (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                            "Color setting save failed: %.45s",
                            WUPSStorageAPI::GetStatusStr(error));
    }
}

void LoadSlotBackgroundColors() {
    for (size_t slot = 0; slot < kHistoryCount; ++slot) {
        char key[32] = {};
        (void)std::snprintf(key, sizeof(key), "%s%02u", kSlotBackgroundColorStoragePrefix,
                            static_cast<unsigned>(slot + 1));
        slotBackgroundColorSelections[slot] = kUseGlobalBackgroundColor;
        const WUPSStorageError error = WUPSStorageAPI::GetOrStoreDefault<uint32_t>(
            key, slotBackgroundColorSelections[slot], kUseGlobalBackgroundColor);
        if (error != WUPS_STORAGE_ERROR_SUCCESS ||
            slotBackgroundColorSelections[slot] > kUseGlobalBackgroundColor) {
            slotBackgroundColorSelections[slot] = kUseGlobalBackgroundColor;
        }
    }
}

void SlotBackgroundColorChanged(size_t slot, uint32_t newValue) {
    if (slot >= kHistoryCount) return;
    slotBackgroundColorSelections[slot] = newValue <= kUseGlobalBackgroundColor
        ? newValue : kUseGlobalBackgroundColor;

    char key[32] = {};
    (void)std::snprintf(key, sizeof(key), "%s%02u", kSlotBackgroundColorStoragePrefix,
                        static_cast<unsigned>(slot + 1));
    const WUPSStorageError error = WUPSStorageAPI::Store(key, slotBackgroundColorSelections[slot]);
    if (error != WUPS_STORAGE_ERROR_SUCCESS) {
        (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                            "Slot color save failed: %.44s",
                            WUPSStorageAPI::GetStatusStr(error));
    }
    ApplyNotificationBackgroundColor();
}

void SlotBackgroundColorChanged0(ConfigItemMultipleValues*, uint32_t newValue) {
    SlotBackgroundColorChanged(0, newValue);
}

void SlotBackgroundColorChanged1(ConfigItemMultipleValues*, uint32_t newValue) {
    SlotBackgroundColorChanged(1, newValue);
}

void SlotBackgroundColorChanged2(ConfigItemMultipleValues*, uint32_t newValue) {
    SlotBackgroundColorChanged(2, newValue);
}

void SlotBackgroundColorChanged3(ConfigItemMultipleValues*, uint32_t newValue) {
    SlotBackgroundColorChanged(3, newValue);
}

void SlotBackgroundColorChanged4(ConfigItemMultipleValues*, uint32_t newValue) {
    SlotBackgroundColorChanged(4, newValue);
}

void SlotBackgroundColorChanged5(ConfigItemMultipleValues*, uint32_t newValue) {
    SlotBackgroundColorChanged(5, newValue);
}

void SlotBackgroundColorChanged6(ConfigItemMultipleValues*, uint32_t newValue) {
    SlotBackgroundColorChanged(6, newValue);
}

void SlotBackgroundColorChanged7(ConfigItemMultipleValues*, uint32_t newValue) {
    SlotBackgroundColorChanged(7, newValue);
}

void SlotBackgroundColorChanged8(ConfigItemMultipleValues*, uint32_t newValue) {
    SlotBackgroundColorChanged(8, newValue);
}

void SlotBackgroundColorChanged9(ConfigItemMultipleValues*, uint32_t newValue) {
    SlotBackgroundColorChanged(9, newValue);
}

WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle rootHandle) {
    try {
        LoadSlotBackgroundColors();
        WUPSConfigCategory root(rootHandle);
        root.add(WUPSConfigItemStub::Create("TV + L: toggle Recent Apps notification"));
        root.add(WUPSConfigItemStub::Create("TV + Up/Down: select HOME or a recent title"));
        root.add(WUPSConfigItemStub::Create("TV + A: open HOME or launch the selected title"));

        constexpr WUPSConfigItemMultipleValues::ValuePair backgroundColorOptions[] = {
            {0, "Standard Notifications"},
            {1, "Black"},
            {2, "Charcoal"},
            {3, "Gray"},
            {4, "Slate Blue"},
            {5, "Silver"},
            {6, "Light Gray"},
            {7, "Red"},
            {8, "Red-Orange"},
            {9, "Crimson"},
            {10, "Coral"},
            {11, "Orange"},
            {12, "Orange-Yellow"},
            {13, "Pumpkin"},
            {14, "Amber"},
            {15, "Gold"},
            {16, "Yellow"},
            {17, "Yellow-Green"},
            {18, "Lime"},
            {19, "Green"},
            {20, "Green-Cyan"},
            {21, "Mint"},
            {22, "Teal"},
            {23, "Cyan"},
            {24, "Cyan-Blue"},
            {25, "Sky Blue"},
            {26, "Blue"},
            {27, "Blue-Violet"},
            {28, "Royal Blue"},
            {29, "Periwinkle"},
            {30, "Indigo"},
            {31, "Purple"},
            {32, "Purple-Magenta"},
            {33, "Violet"},
            {34, "Lavender"},
            {35, "Magenta"},
            {36, "Hot Pink"},
            {37, "Pink"},
            {38, "Rose"},
            {39, "Brown"},
            {40, "Chocolate"},
            {41, "Copper"},
            {42, "Sand"},
            {43, "Cream"},
            {44, "Forest Green"},
            {45, "Ocean Blue"},
            {46, "Neon Blue"},
            {47, "Neon Green"},
            {48, "Neon Pink"},
            {49, "Sunset"},
            {50, "Aurora"},
        };
        root.add(WUPSConfigItemMultipleValues::CreateFromValue(
            kBackgroundColorStorageId, "Background Color", kDefaultBackgroundColor,
            backgroundColorSelection, backgroundColorOptions, &BackgroundColorChangedCallback));

        constexpr WUPSConfigItemMultipleValues::ValuePair slotBackgroundColorOptions[] = {
            {0, "Standard Notifications"},
            {1, "Black"},
            {2, "Charcoal"},
            {3, "Gray"},
            {4, "Slate Blue"},
            {5, "Silver"},
            {6, "Light Gray"},
            {7, "Red"},
            {8, "Red-Orange"},
            {9, "Crimson"},
            {10, "Coral"},
            {11, "Orange"},
            {12, "Orange-Yellow"},
            {13, "Pumpkin"},
            {14, "Amber"},
            {15, "Gold"},
            {16, "Yellow"},
            {17, "Yellow-Green"},
            {18, "Lime"},
            {19, "Green"},
            {20, "Green-Cyan"},
            {21, "Mint"},
            {22, "Teal"},
            {23, "Cyan"},
            {24, "Cyan-Blue"},
            {25, "Sky Blue"},
            {26, "Blue"},
            {27, "Blue-Violet"},
            {28, "Royal Blue"},
            {29, "Periwinkle"},
            {30, "Indigo"},
            {31, "Purple"},
            {32, "Purple-Magenta"},
            {33, "Violet"},
            {34, "Lavender"},
            {35, "Magenta"},
            {36, "Hot Pink"},
            {37, "Pink"},
            {38, "Rose"},
            {39, "Brown"},
            {40, "Chocolate"},
            {41, "Copper"},
            {42, "Sand"},
            {43, "Cream"},
            {44, "Forest Green"},
            {45, "Ocean Blue"},
            {46, "Neon Blue"},
            {47, "Neon Green"},
            {48, "Neon Pink"},
            {49, "Sunset"},
            {50, "Aurora"},
            {kUseGlobalBackgroundColor, "Use Global Color"},
        };
        root.add(WUPSConfigItemStub::Create("Per-Slot Background Colors"));
        using SlotColorCallback = void (*)(ConfigItemMultipleValues*, uint32_t);
        constexpr SlotColorCallback slotColorCallbacks[] = {&SlotBackgroundColorChanged0, &SlotBackgroundColorChanged1, &SlotBackgroundColorChanged2, &SlotBackgroundColorChanged3, &SlotBackgroundColorChanged4, &SlotBackgroundColorChanged5, &SlotBackgroundColorChanged6, &SlotBackgroundColorChanged7, &SlotBackgroundColorChanged8, &SlotBackgroundColorChanged9};
        for (size_t slot = 0; slot < kHistoryCount; ++slot) {
            char key[32] = {};
            char label[112] = {};
            (void)std::snprintf(key, sizeof(key), "%s%02u", kSlotBackgroundColorStoragePrefix,
                                static_cast<unsigned>(slot + 1));
            const char* titleName = recentApps[slot] == 0
                ? "Empty" : (recentAppNames[slot][0] != '\\0' ? recentAppNames[slot] : "Unnamed title");
            (void)std::snprintf(label, sizeof(label), "Slot %02u: %.58s",
                                static_cast<unsigned>(slot + 1), titleName);
            root.add(WUPSConfigItemMultipleValues::CreateFromValue(
                key, label, kUseGlobalBackgroundColor, slotBackgroundColorSelections[slot],
                slotBackgroundColorOptions, slotColorCallbacks[slot]));
        }
    } catch (...) {
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }
    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}
void ConfigMenuClosedCallback() {
    const WUPSStorageError error = WUPSStorageAPI::SaveStorage();
    if (error != WUPS_STORAGE_ERROR_SUCCESS) {
        (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                            "Settings save failed: %.45s",
                            WUPSStorageAPI::GetStatusStr(error));
    }
}
} // namespace


// Keep TV-modified shortcuts from also triggering native HOME Menu navigation.
// The TV + L shortcut opens Recent Apps; suppress L while TV is held so it
// cannot also turn the HOME Menu app pages. Once the overlay is visible,
// suppress TV + Up/Down from moving the HOME Menu cursor as well.
DECL_FUNCTION(int32_t, VPADRead, VPADChan chan, VPADStatus* buffers, uint32_t count, VPADReadError* error) {
    const int32_t result = real_VPADRead(chan, buffers, count, error);
    if (result > 0 && buffers != nullptr) {
        for (int32_t i = 0; i < result; ++i) {
            if ((buffers[i].hold & VPAD_BUTTON_TV) != 0) {
                constexpr uint32_t kNavigationButtons = VPAD_BUTTON_UP | VPAD_BUTTON_DOWN;
                constexpr uint32_t kPageNavigationButton = VPAD_BUTTON_L;
                buffers[i].hold &= ~kPageNavigationButton;
                buffers[i].trigger &= ~kPageNavigationButton;
                buffers[i].release &= ~kPageNavigationButton;
                if (overlayVisible) {
                    buffers[i].hold &= ~kNavigationButtons;
                    buffers[i].trigger &= ~kNavigationButtons;
                    buffers[i].release &= ~kNavigationButtons;
                }
            }
        }
    }
    return result;
}
WUPS_MUST_REPLACE(VPADRead, WUPS_LOADER_LIBRARY_VPAD, VPADRead);

INITIALIZE_PLUGIN() {
    ACPInitialize();
    acpReady = true;
    loadBackgroundColorSetting();
    loadRecentApps();
    LoadSlotBackgroundColors();
    resolveAllRecentAppNames();
    inspectRecentTitleIcon(0);

    const NotificationModuleStatus notificationStatus = NotificationModule_InitLibrary();
    notificationLibraryReady = notificationStatus == NOTIFICATION_MODULE_RESULT_SUCCESS;
    if (notificationLibraryReady) {
        ApplyNotificationBackgroundColor();
        (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                            "NotificationModule: ready");
    } else {
        (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                            "NotificationModule unavailable: %s",
                            NotificationModule_GetStatusStr(notificationStatus));
    }

    WUPSButtonCombo_ComboStatus comboStatus = WUPS_BUTTON_COMBO_COMBO_STATUS_INVALID_STATUS;
    const WUPSButtonCombo_Error comboError = WUPSButtonComboAPI_AddButtonComboPressDown(
        "Recent Apps overlay (TV + L)",
        static_cast<WUPSButtonCombo_Buttons>(WUPS_BUTTON_COMBO_BUTTON_TV | WUPS_BUTTON_COMBO_BUTTON_L),
        RecentAppsComboCallback,
        nullptr,
        &overlayComboHandle,
        &comboStatus);
    (void)comboError;
    (void)comboStatus;

    WUPSButtonCombo_ComboStatus previousStatus = WUPS_BUTTON_COMBO_COMBO_STATUS_INVALID_STATUS;
    const WUPSButtonCombo_Error previousError = WUPSButtonComboAPI_AddButtonComboPressDown(
        "Recent Apps: previous title (TV + Up)",
        static_cast<WUPSButtonCombo_Buttons>(WUPS_BUTTON_COMBO_BUTTON_TV | WUPS_BUTTON_COMBO_BUTTON_UP),
        RecentAppsPreviousCallback,
        nullptr,
        &previousComboHandle,
        &previousStatus);

    WUPSButtonCombo_ComboStatus nextStatus = WUPS_BUTTON_COMBO_COMBO_STATUS_INVALID_STATUS;
    const WUPSButtonCombo_Error nextError = WUPSButtonComboAPI_AddButtonComboPressDown(
        "Recent Apps: next title (TV + Down)",
        static_cast<WUPSButtonCombo_Buttons>(WUPS_BUTTON_COMBO_BUTTON_TV | WUPS_BUTTON_COMBO_BUTTON_DOWN),
        RecentAppsNextCallback,
        nullptr,
        &nextComboHandle,
        &nextStatus);

    (void)previousError;
    (void)previousStatus;
    (void)nextError;
    (void)nextStatus;

    WUPSButtonCombo_ComboStatus launchStatus = WUPS_BUTTON_COMBO_COMBO_STATUS_INVALID_STATUS;
    const WUPSButtonCombo_Error launchError = WUPSButtonComboAPI_AddButtonComboPressDown(
        "Recent Apps: launch selected title (TV + A)",
        static_cast<WUPSButtonCombo_Buttons>(WUPS_BUTTON_COMBO_BUTTON_TV | WUPS_BUTTON_COMBO_BUTTON_A),
        RecentAppsLaunchCallback,
        nullptr,
        &launchComboHandle,
        &launchStatus);

    (void)launchError;
    (void)launchStatus;

    WUPSConfigAPIOptionsV1 configOptions = {.name = "Recent Apps"};
    (void)WUPSConfigAPI_Init(configOptions, ConfigMenuOpenedCallback, ConfigMenuClosedCallback);
}

DEINITIALIZE_PLUGIN() {
    HideRecentAppsNotification();
    if (acpReady) {
        ACPFinalize();
        acpReady = false;
    }
    if (notificationLibraryReady) {
        (void)NotificationModule_DeInitLibrary();
        notificationLibraryReady = false;
    }
    if (overlayComboHandle.handle != nullptr) {
        (void)WUPSButtonComboAPI_RemoveButtonCombo(overlayComboHandle);
        overlayComboHandle.handle = nullptr;
    }
    if (previousComboHandle.handle != nullptr) {
        (void)WUPSButtonComboAPI_RemoveButtonCombo(previousComboHandle);
        previousComboHandle.handle = nullptr;
    }
    if (nextComboHandle.handle != nullptr) {
        (void)WUPSButtonComboAPI_RemoveButtonCombo(nextComboHandle);
        nextComboHandle.handle = nullptr;
    }
    if (launchComboHandle.handle != nullptr) {
        (void)WUPSButtonComboAPI_RemoveButtonCombo(launchComboHandle);
        launchComboHandle.handle = nullptr;
    }
}

ON_APPLICATION_START() {
    rememberTitle(OSGetTitleID());
}

