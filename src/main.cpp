#include <wups.h>
#include <wups/storage.h>
#include <wups/config/WUPSConfigCategory.h>
#include <wups/config/WUPSConfigItemStub.h>
#include <wups/config_api.h>
#include <wups/button_combo/api.h>
#include <coreinit/title.h>
#include <nn/acp.h>
#include <coreinit/cache.h>
#include <sysapp/launch.h>
#include <vpad/input.h>
#include <notifications/notifications.h>
#include <gx2/swap.h>
#include <gx2/surface.h>
#include <gx2/context.h>

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

uint64_t recentApps[kHistoryCount] = {};
char recentAppNames[kHistoryCount][64] = {};
alignas(0x40) ACPMetaXml titleMetaBuffer = {};
bool acpReady = false;
WUPSButtonCombo_ComboHandle overlayComboHandle = {};
WUPSButtonCombo_ComboHandle previousComboHandle = {};
WUPSButtonCombo_ComboHandle nextComboHandle = {};
WUPSButtonCombo_ComboHandle launchComboHandle = {};
char comboRegistrationLine[128] = "Shortcut registration: not initialized";
char navigationRegistrationLine[128] = "Navigation shortcuts: not initialized";
char launchRegistrationLine[96] = "Launch shortcut: not initialized";
size_t selectedRecentIndex = 0;
uint32_t overlayComboCallbackCount = 0;
uint32_t gx2SwapHookCallCount = 0;
uint32_t gx2CopyHookCallCount = 0;
uint32_t gx2TvCopyHookCallCount = 0;
uint32_t gx2DrcCopyHookCallCount = 0;
uint32_t gx2LastColorBufferWidth = 0;
uint32_t gx2LastColorBufferHeight = 0;
bool gx2ContextObserved = false;
bool gx2ContextSetupObserved = false;
bool overlayVisible = false;
bool notificationLibraryReady = false;
std::atomic<NotificationModuleHandle> notificationHandle{0};
char notificationStatusLine[96] = "NotificationModule: not initialized";
char historyStatusLine[96] = "History: not checked";
char titleIconStatusLine[128] = "Title icons: not checked";
alignas(0x40) uint8_t titleIconRgba[128 * 128 * 4] = {};
alignas(0x40) uint8_t titleIconSourceBgra[128 * 128 * 4] = {};

void BuildRecentAppsNotification(char* buffer, size_t bufferSize) {
    size_t used = 0;
    int written = std::snprintf(buffer, bufferSize, "Recent Apps");
    if (written < 0 || static_cast<size_t>(written) >= bufferSize) {
        if (bufferSize > 0) buffer[bufferSize - 1] = '\0';
        return;
    }
    used = static_cast<size_t>(written);

    for (size_t i = 0; i < kHistoryCount && used + 1 < bufferSize; ++i) {
        if (recentApps[i] == 0) continue;
        written = std::snprintf(buffer + used, bufferSize - used,
                                "\\n%c%02u. %.45s",
                                i == selectedRecentIndex ? '>' : ' ',
                                static_cast<unsigned>(i + 1),
                                recentAppNames[i][0] != '\0' ? recentAppNames[i] : "Unnamed title");
        if (written < 0) break;
        if (static_cast<size_t>(written) >= bufferSize - used) {
            buffer[bufferSize - 1] = '\0';
            break;
        }
        used += static_cast<size_t>(written);
    }

    if (used == std::strlen("Recent Apps")) {
        (void)std::snprintf(buffer + used, bufferSize - used, "\nNo titles recorded yet");
    }
}

void ShowRecentAppsNotification() {
    if (!notificationLibraryReady) {
        (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                            "NotificationModule: unavailable");
        return;
    }

    char text[256];
    BuildRecentAppsNotification(text, sizeof(text));

    NotificationModuleHandle handle = 0;
    const NotificationModuleStatus status = NotificationModule_AddDynamicNotification(
        text, &handle);
    if (status != NOTIFICATION_MODULE_RESULT_SUCCESS) {
        (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                            "Notification failed: %s", NotificationModule_GetStatusStr(status));
        return;
    }

    notificationHandle.store(handle);
    (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                        "NotificationModule: display requested");
}

void HideRecentAppsNotification() {
    const NotificationModuleHandle handle = notificationHandle.load();
    if (!notificationLibraryReady || handle == 0) return;

    const NotificationModuleStatus status = NotificationModule_FinishDynamicNotification(handle, 0.0f);
    if (status != NOTIFICATION_MODULE_RESULT_SUCCESS) {
        (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                            "Notification close failed: %s", NotificationModule_GetStatusStr(status));
    } else {
        notificationHandle.store(0);
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
    ++overlayComboCallbackCount;
    overlayVisible = !overlayVisible;
    if (overlayVisible) {
        ShowRecentAppsNotification();
    } else {
        HideRecentAppsNotification();
    }
}

void RefreshRecentAppsNotification() {
    if (!overlayVisible || !notificationLibraryReady) return;

    const NotificationModuleHandle handle = notificationHandle.load();
    if (handle == 0) {
        ShowRecentAppsNotification();
        return;
    }

    char text[256];
    BuildRecentAppsNotification(text, sizeof(text));
    const NotificationModuleStatus status =
        NotificationModule_UpdateDynamicNotificationText(handle, text);

    if (status != NOTIFICATION_MODULE_RESULT_SUCCESS) {
        // Keep navigation usable on older or incompatible NotificationModule versions.
        // The fallback can briefly flicker, but selection still updates.
        HideRecentAppsNotification();
        ShowRecentAppsNotification();
        (void)std::snprintf(notificationStatusLine, sizeof(notificationStatusLine),
                            "Notification update fallback: %s",
                            NotificationModule_GetStatusStr(status));
    }
}

void MoveRecentSelection(int direction) {
    if (!overlayVisible) return;

    for (size_t attempt = 0; attempt < kHistoryCount; ++attempt) {
        if (direction > 0) {
            selectedRecentIndex = (selectedRecentIndex + 1) % kHistoryCount;
        } else {
            selectedRecentIndex = (selectedRecentIndex + kHistoryCount - 1) % kHistoryCount;
        }

        if (recentApps[selectedRecentIndex] != 0) {
            // Keep the decoded RGBA buffer in sync with the highlighted title.
            // The renderer will consume this buffer when icon drawing is enabled.
            inspectRecentTitleIcon(selectedRecentIndex);
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

    const uint64_t titleId = recentApps[selectedRecentIndex];
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

WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle rootHandle) {
    try {
        WUPSConfigCategory root(rootHandle);
        root.add(WUPSConfigItemStub::Create("TV + L: toggle Recent Apps notification"));
        root.add(WUPSConfigItemStub::Create("TV + Up/Down: select a recent title"));
        root.add(WUPSConfigItemStub::Create("TV + A: launch the selected title"));
        root.add(WUPSConfigItemStub::Create("Recent titles:"));
        bool anyTitles = false;

        for (size_t i = 0; i < kHistoryCount; ++i) {
            char line[64];
            if (recentApps[i] == 0) {
                std::snprintf(line, sizeof(line), "%02u. (not recorded yet)",
                              static_cast<unsigned>(i + 1));
            } else {
                std::snprintf(line, sizeof(line), "%02u. %.48s",
                              static_cast<unsigned>(i + 1),
                              recentAppNames[i][0] != '\0' ? recentAppNames[i] : "Unnamed title");
                anyTitles = true;
            }
            root.add(WUPSConfigItemStub::Create(line));
        }

        if (!anyTitles) {
            root.add(WUPSConfigItemStub::Create("Launch a game/app; each distinct title fills one slot."));
        }
    } catch (...) {
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }
    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

void ConfigMenuClosedCallback() {}
} // namespace


// Prevent TV-modified D-pad navigation from also moving the HOME Menu cursor.
// ButtonComboModule still handles the registered TV + Up/Down shortcuts.
DECL_FUNCTION(int32_t, VPADRead, VPADChan chan, VPADStatus* buffers, uint32_t count, VPADReadError* error) {
    const int32_t result = real_VPADRead(chan, buffers, count, error);
    if (result > 0 && buffers != nullptr && overlayVisible) {
        for (int32_t i = 0; i < result; ++i) {
            if ((buffers[i].hold & VPAD_BUTTON_TV) != 0) {
                constexpr uint32_t kNavigationButtons = VPAD_BUTTON_UP | VPAD_BUTTON_DOWN;
                buffers[i].hold &= ~kNavigationButtons;
                buffers[i].trigger &= ~kNavigationButtons;
                buffers[i].release &= ~kNavigationButtons;
            }
        }
    }
    return result;
}
WUPS_MUST_REPLACE(VPADRead, WUPS_LOADER_LIBRARY_VPAD, VPADRead);

// Keep this hook state-neutral while recording the actual scan-copy target and dimensions.
DECL_FUNCTION(void, GX2CopyColorBufferToScanBuffer, GX2ColorBuffer* colorBuffer, GX2ScanTarget scanTarget) {
    ++gx2CopyHookCallCount;
    if (scanTarget == GX2_SCAN_TARGET_TV) {
        ++gx2TvCopyHookCallCount;
    } else if (scanTarget == GX2_SCAN_TARGET_DRC) {
        ++gx2DrcCopyHookCallCount;
    }
    if (colorBuffer != nullptr) {
        gx2LastColorBufferWidth = colorBuffer->surface.width;
        gx2LastColorBufferHeight = colorBuffer->surface.height;
    }
    real_GX2CopyColorBufferToScanBuffer(colorBuffer, scanTarget);
}
WUPS_MUST_REPLACE(GX2CopyColorBufferToScanBuffer, WUPS_LOADER_LIBRARY_GX2, GX2CopyColorBufferToScanBuffer);

// Third diagnostic: observe the active GX2 context without changing it.
DECL_FUNCTION(void, GX2SetContextState, GX2ContextState* state) {
    if (state != nullptr) gx2ContextObserved = true;
    real_GX2SetContextState(state);
}
WUPS_MUST_REPLACE(GX2SetContextState, WUPS_LOADER_LIBRARY_GX2, GX2SetContextState);

// Fourth diagnostic: observe context setup without altering the requested operation.
DECL_FUNCTION(void, GX2SetupContextStateEx, GX2ContextState* state, BOOL enable) {
    real_GX2SetupContextStateEx(state, enable);
    if (state != nullptr) gx2ContextSetupObserved = true;
}
WUPS_MUST_REPLACE(GX2SetupContextStateEx, WUPS_LOADER_LIBRARY_GX2, GX2SetupContextStateEx);

// Second diagnostic: observe scan-buffer swaps without changing rendering state.
DECL_FUNCTION(void, GX2SwapScanBuffers) {
    ++gx2SwapHookCallCount;
    real_GX2SwapScanBuffers();
}
WUPS_MUST_REPLACE(GX2SwapScanBuffers, WUPS_LOADER_LIBRARY_GX2, GX2SwapScanBuffers);

INITIALIZE_PLUGIN() {
    ACPInitialize();
    acpReady = true;
    loadRecentApps();
    resolveAllRecentAppNames();
    inspectRecentTitleIcon(0);

    const NotificationModuleStatus notificationStatus = NotificationModule_InitLibrary();
    notificationLibraryReady = notificationStatus == NOTIFICATION_MODULE_RESULT_SUCCESS;
    if (notificationLibraryReady) {
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
    std::snprintf(comboRegistrationLine, sizeof(comboRegistrationLine),
                  "TV + L: %s / %s",
                  WUPSButtonComboAPI_GetStatusStr(comboError),
                  WUPSButtonComboAPI_GetComboStatusStr(comboStatus));

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

    std::snprintf(navigationRegistrationLine, sizeof(navigationRegistrationLine),
                  "TV + Up: %s/%s; Down: %s/%s",
                  WUPSButtonComboAPI_GetStatusStr(previousError),
                  WUPSButtonComboAPI_GetComboStatusStr(previousStatus),
                  WUPSButtonComboAPI_GetStatusStr(nextError),
                  WUPSButtonComboAPI_GetComboStatusStr(nextStatus));

    WUPSButtonCombo_ComboStatus launchStatus = WUPS_BUTTON_COMBO_COMBO_STATUS_INVALID_STATUS;
    const WUPSButtonCombo_Error launchError = WUPSButtonComboAPI_AddButtonComboPressDown(
        "Recent Apps: launch selected title (TV + A)",
        static_cast<WUPSButtonCombo_Buttons>(WUPS_BUTTON_COMBO_BUTTON_TV | WUPS_BUTTON_COMBO_BUTTON_A),
        RecentAppsLaunchCallback,
        nullptr,
        &launchComboHandle,
        &launchStatus);

    std::snprintf(launchRegistrationLine, sizeof(launchRegistrationLine),
                  "TV + A: %s / %s",
                  WUPSButtonComboAPI_GetStatusStr(launchError),
                  WUPSButtonComboAPI_GetComboStatusStr(launchStatus));

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

