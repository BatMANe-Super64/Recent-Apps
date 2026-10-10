#include <wups.h>
#include <coreinit/title.h>
#include <coreinit/screen.h>
#include <vpad/input.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

WUPS_USE_WUT_DEVOPTAB();

WUPS_PLUGIN_NAME("WiiUKioskRecentApps");
WUPS_PLUGIN_DESCRIPTION("A recent-apps overlay restricted to the Wii U Home Menu.");
WUPS_PLUGIN_VERSION("v0.4-alpha");
WUPS_PLUGIN_AUTHOR("Developer");
WUPS_PLUGIN_LICENSE("GPL-3.0");

namespace {
constexpr uint64_t kHomeMenuUsa = 0x0005001010040100ULL;
constexpr uint64_t kHomeMenuEur = 0x0005001010040200ULL;
constexpr uint64_t kHomeMenuJpn = 0x0005001010040000ULL;
constexpr char kHistoryPath[] = "/vol/external01/wiiu/plugins/recent-apps.dat";
constexpr uint32_t kHistoryMagic = 0x52415050; // "RAPP"
constexpr uint32_t kHistoryVersion = 1;
constexpr size_t kHistoryCount = 10;

struct HistoryFile {
    uint32_t magic;
    uint32_t version;
    uint64_t titleIds[kHistoryCount];
};

uint64_t recentApps[kHistoryCount] = {};
int selectedIndex = 0;
uint8_t overlayCooldown = 0;

bool isHomeMenuTitle(uint64_t titleId) {
    return titleId == kHomeMenuUsa ||
           titleId == kHomeMenuEur ||
           titleId == kHomeMenuJpn;
}

void loadRecentApps() {
    FILE* file = std::fopen(kHistoryPath, "rb");
    if (file == nullptr) {
        return;
    }

    HistoryFile history = {};
    const size_t bytesRead = std::fread(&history, 1, sizeof(history), file);
    std::fclose(file);

    if (bytesRead != sizeof(history) ||
        history.magic != kHistoryMagic ||
        history.version != kHistoryVersion) {
        return;
    }

    std::memcpy(recentApps, history.titleIds, sizeof(recentApps));
}

void saveRecentApps() {
    HistoryFile history = {};
    history.magic = kHistoryMagic;
    history.version = kHistoryVersion;
    std::memcpy(history.titleIds, recentApps, sizeof(recentApps));

    FILE* file = std::fopen(kHistoryPath, "wb");
    if (file == nullptr) {
        return;
    }

    const size_t bytesWritten = std::fwrite(&history, 1, sizeof(history), file);
    std::fclose(file);
    (void)bytesWritten;
}

void rememberTitle(uint64_t titleId) {
    if (titleId == 0 || isHomeMenuTitle(titleId)) {
        return;
    }

    uint64_t updated[kHistoryCount] = {};
    updated[0] = titleId;
    size_t next = 1;

    for (size_t i = 0; i < kHistoryCount && next < kHistoryCount; ++i) {
        if (recentApps[i] == 0 || recentApps[i] == titleId) {
            continue;
        }
        updated[next++] = recentApps[i];
    }

    if (std::memcmp(recentApps, updated, sizeof(recentApps)) != 0) {
        std::memcpy(recentApps, updated, sizeof(recentApps));
        saveRecentApps();
    }
}

void drawRecentAppsOverlay(wups_overlay_options_type_t screen, void*) {
    bool running = true;

    while (running) {
        WUPS_Overlay_OSScreenClear(screen);
        WUPS_Overlay_PrintTextOnScreen(screen, 2, 1, "RECENT APPS");
        WUPS_Overlay_PrintTextOnScreen(screen, 2, 3, "D-Pad Up/Down: select");
        WUPS_Overlay_PrintTextOnScreen(screen, 2, 4, "B or Minus: close");

        bool hasApps = false;
        for (size_t i = 0; i < kHistoryCount; ++i) {
            if (recentApps[i] != 0) {
                hasApps = true;
                break;
            }
        }

        if (!hasApps) {
            WUPS_Overlay_PrintTextOnScreen(screen, 2, 7, "No recent titles recorded yet.");
        } else {
            for (size_t i = 0; i < kHistoryCount; ++i) {
                char line[64];
                if (recentApps[i] == 0) {
                    std::snprintf(line, sizeof(line), "%c %2u. --",
                                  static_cast<int>(i) == selectedIndex ? '>' : ' ',
                                  static_cast<unsigned>(i + 1));
                } else {
                    std::snprintf(line, sizeof(line), "%c %2u. %016llX",
                                  static_cast<int>(i) == selectedIndex ? '>' : ' ',
                                  static_cast<unsigned>(i + 1),
                                  static_cast<unsigned long long>(recentApps[i]));
                }
                WUPS_Overlay_PrintTextOnScreen(screen, 2, 7 + static_cast<int>(i), line);
            }
        }

        WUPS_Overlay_FlipBuffers(screen);

        VPADStatus status = {};
        int32_t error = 0;
        const int32_t readResult = real_VPADRead(0, &status, 1, &error);
        if (readResult > 0) {
            if ((status.trigger & VPAD_BUTTON_B) ||
                (status.trigger & VPAD_BUTTON_MINUS)) {
                running = false;
            } else if ((status.trigger & VPAD_BUTTON_UP) && selectedIndex > 0) {
                --selectedIndex;
            } else if ((status.trigger & VPAD_BUTTON_DOWN) &&
                       selectedIndex < static_cast<int>(kHistoryCount) - 1) {
                ++selectedIndex;
            }
        }
    }
}
} // namespace

INITIALIZE_PLUGIN() {
    loadRecentApps();
}

ON_APPLICATION_START() {
    rememberTitle(OSGetTitleID());
}

DECL_FUNCTION(int32_t, VPADRead, int32_t chan, VPADStatus* buffer,
              uint32_t buffer_size, int32_t* error) {
    const int32_t result = real_VPADRead(chan, buffer, buffer_size, error);

    if (result > 0 && buffer != nullptr && chan == 0) {
        if (overlayCooldown > 0) {
            --overlayCooldown;
        }

        if (isHomeMenuTitle(OSGetTitleID()) &&
            (buffer[0].trigger & VPAD_BUTTON_MINUS) &&
            overlayCooldown == 0) {
            overlayCooldown = 30;
            WUPS_OpenOverlay(WUPS_OVERLAY_DRC_AND_TV, drawRecentAppsOverlay, nullptr);
        }
    }

    return result;
}

WUPS_MUST_REPLACE(VPADRead, WUPS_LOADER_LIBRARY_VPAD, VPADRead);
