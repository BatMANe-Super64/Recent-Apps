#include <wups.h>
#include <coreinit/title.h>
#include <vpad/input.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

// Enable standard C file I/O through WUT's mounted filesystem.
WUPS_USE_WUT_DEVOPTAB();

// WUPS plugin metadata.
WUPS_PLUGIN_NAME("WiiUKioskRecentApps");
WUPS_PLUGIN_DESCRIPTION("A kiosk overlay restricted strictly to the Wii U Home Menu Plaza.");
WUPS_PLUGIN_VERSION("v0.3-alpha");
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

struct LayoutSlot {
    int x;
    int y;
    int width;
    int height;
};

constexpr LayoutSlot kSlots[5] = {
    {100, 200, 100, 100},
    {250, 250, 150, 150},
    {450, 300, 250, 250},
    {750, 250, 150, 150},
    {950, 200, 100, 100},
};

uint64_t recentApps[kHistoryCount] = {};
int currentCenterIndex = 0;
bool isMenuOverlayOpen = false;

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
        // Keep the in-memory history working if storage is unavailable.
        return;
    }

    const bool saved = std::fwrite(&history, 1, sizeof(history), file) == sizeof(history);
    std::fclose(file);
    (void)saved;
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
} // namespace

bool isUserOnHomeMenu() {
    return isHomeMenuTitle(OSGetTitleID());
}

void drawHeroBackgroundWallpaper(uint64_t titleID) {
    if (titleID == 0) {
        return;
    }
    // TODO: Load and draw the selected title's hero artwork.
}

void drawAppIconTexture(uint64_t titleID, int x, int y, int width, int height) {
    if (titleID == 0 || width <= 0 || height <= 0) {
        return;
    }
    // TODO: Load and draw the app icon at the supplied position.
}

void handleControllerInput(VPADStatus* status) {
    if (status == nullptr || !isUserOnHomeMenu()) {
        isMenuOverlayOpen = false;
        return;
    }

    if (status->trigger & VPAD_BUTTON_MINUS) {
        isMenuOverlayOpen = !isMenuOverlayOpen;
    }

    if (isMenuOverlayOpen) {
        if ((status->trigger & VPAD_BUTTON_LEFT) && currentCenterIndex > 0) {
            --currentCenterIndex;
        }
        if ((status->trigger & VPAD_BUTTON_RIGHT) &&
            currentCenterIndex < static_cast<int>(kHistoryCount) - 1) {
            ++currentCenterIndex;
        }
    }
}

void renderKioskOverlay() {
    if (!isMenuOverlayOpen || !isUserOnHomeMenu()) {
        return;
    }

    const uint64_t activeTitleID = recentApps[currentCenterIndex];
    drawHeroBackgroundWallpaper(activeTitleID);

    for (int i = 0; i < 5; ++i) {
        const int appTargetIndex = currentCenterIndex - 2 + i;
        if (appTargetIndex < 0 || appTargetIndex >= static_cast<int>(kHistoryCount)) {
            continue;
        }

        const uint64_t titleId = recentApps[appTargetIndex];
        drawAppIconTexture(titleId, kSlots[i].x, kSlots[i].y,
                           kSlots[i].width, kSlots[i].height);
    }
}

INITIALIZE_PLUGIN() {
    loadRecentApps();
}

ON_APPLICATION_START() {
    rememberTitle(OSGetTitleID());
}

// History persistence and Home Menu title detection are implemented. Actual
// controller/render hooks and icon/wallpaper drawing remain future work.
