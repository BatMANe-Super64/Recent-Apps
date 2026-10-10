#include <wups.h>
#include <wups/config/WUPSConfigCategory.h>
#include <wups/config/WUPSConfigItemStub.h>
#include <wups/config_api.h>
#include <wups/button_combo/api.h>
#include <coreinit/title.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

WUPS_USE_WUT_DEVOPTAB();
WUPS_ALLOW_OVERLAY();

WUPS_PLUGIN_NAME("WiiUKioskRecentApps");
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
// Keep history in an existing SD directory so it survives title changes/reboots.
constexpr char kHistoryPath[] = "sd:/wiiu/recent-apps.dat";
constexpr uint32_t kHistoryMagic = 0x52415050;
constexpr uint32_t kHistoryVersion = 1;
constexpr size_t kHistoryCount = 10;

struct HistoryFile {
    uint32_t magic;
    uint32_t version;
    uint64_t titleIds[kHistoryCount];
};

uint64_t recentApps[kHistoryCount] = {};
WUPSButtonCombo_ComboHandle overlayComboHandle = {};

bool isHomeMenuTitle(uint64_t titleId) {
    return titleId == kHomeMenuUsa ||
           titleId == kHomeMenuEur ||
           titleId == kHomeMenuJpn ||
           titleId == kHomeMenuKor ||
           titleId == kHomeMenuTwn ||
           titleId == kHomeMenuChn;
}

void loadRecentApps() {
    FILE* file = std::fopen(kHistoryPath, "rb");
    if (file == nullptr) return;

    HistoryFile history = {};
    const size_t bytesRead = std::fread(&history, 1, sizeof(history), file);
    std::fclose(file);

    if (bytesRead == sizeof(history) &&
        history.magic == kHistoryMagic &&
        history.version == kHistoryVersion) {
        std::memcpy(recentApps, history.titleIds, sizeof(recentApps));
    }
}

void saveRecentApps() {
    HistoryFile history = {};
    history.magic = kHistoryMagic;
    history.version = kHistoryVersion;
    std::memcpy(history.titleIds, recentApps, sizeof(recentApps));

    FILE* file = std::fopen(kHistoryPath, "wb");
    if (file == nullptr) return;
    (void)std::fwrite(&history, 1, sizeof(history), file);
    std::fclose(file);
}

void rememberTitle(uint64_t titleId) {
    if (titleId == 0 || isHomeMenuTitle(titleId)) return;

    uint64_t updated[kHistoryCount] = {};
    updated[0] = titleId;
    size_t next = 1;

    for (size_t i = 0; i < kHistoryCount && next < kHistoryCount; ++i) {
        if (recentApps[i] == 0 || recentApps[i] == titleId) continue;
        updated[next++] = recentApps[i];
    }

    if (std::memcmp(recentApps, updated, sizeof(recentApps)) != 0) {
        std::memcpy(recentApps, updated, sizeof(recentApps));
        saveRecentApps();
    }
}

void RecentAppsOverlayCallback(wups_overlay_options_type_t screen, void*) {
    for (int frame = 0; frame < 120; ++frame) {
        WUPS_Overlay_OSScreenClear(screen);
        WUPS_Overlay_PrintTextOnScreen(screen, 1, 1, "RECENT APPS");
        WUPS_Overlay_PrintTextOnScreen(screen, 1, 3, "Newest titles first");
        bool anyTitles = false;
        for (size_t i = 0; i < kHistoryCount; ++i) {
            char line[64];
            if (recentApps[i] == 0) {
                std::snprintf(line, sizeof(line), "%02u. --", static_cast<unsigned>(i + 1));
            } else {
                std::snprintf(line, sizeof(line), "%02u. %016llX",
                              static_cast<unsigned>(i + 1),
                              static_cast<unsigned long long>(recentApps[i]));
                anyTitles = true;
            }
            WUPS_Overlay_PrintTextOnScreen(screen, 2, 5 + static_cast<int>(i), line);
        }
        if (!anyTitles) {
            WUPS_Overlay_PrintTextOnScreen(screen, 2, 7, "Launch a game to populate history.");
        }
        WUPS_Overlay_PrintTextOnScreen(screen, 1, 17, "Close: wait for overlay to finish");
        WUPS_Overlay_FlipBuffers(screen);
    }
}

void RecentAppsComboCallback(WUPSButtonCombo_ControllerTypes, WUPSButtonCombo_ComboHandle, void*) {
    WUPS_OpenOverlay(WUPS_OVERLAY_DRC_AND_TV, RecentAppsOverlayCallback, nullptr);
}

WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle rootHandle) {
    try {
        WUPSConfigCategory root(rootHandle);
        root.add(WUPSConfigItemStub::Create("Recent titles."));
        bool anyTitles = false;

        for (size_t i = 0; i < kHistoryCount; ++i) {
            char line[64];
            if (recentApps[i] == 0) {
                std::snprintf(line, sizeof(line), "%02u. (not recorded yet)",
                              static_cast<unsigned>(i + 1));
            } else {
                std::snprintf(line, sizeof(line), "%02u. Title ID %016llX",
                              static_cast<unsigned>(i + 1),
                              static_cast<unsigned long long>(recentApps[i]));
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

INITIALIZE_PLUGIN() {
    loadRecentApps();

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

    WUPSConfigAPIOptionsV1 configOptions = {.name = "Recent Apps"};
    (void)WUPSConfigAPI_Init(configOptions, ConfigMenuOpenedCallback, ConfigMenuClosedCallback);
}

DEINITIALIZE_PLUGIN() {
    if (overlayComboHandle.handle != nullptr) {
        (void)WUPSButtonComboAPI_RemoveButtonCombo(overlayComboHandle);
        overlayComboHandle.handle = nullptr;
    }
}

ON_APPLICATION_START() {
    rememberTitle(OSGetTitleID());
}
