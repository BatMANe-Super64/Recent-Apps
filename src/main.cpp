#include <wups.h>
#include <coreinit/screen.h>
#include <vpad/input.h>
#include <sysapp/title.h>

// WUPS plugin metadata.
WUPS_PLUGIN_NAME("WiiUKioskRecentApps");
WUPS_PLUGIN_DESCRIPTION("A kiosk overlay restricted strictly to the Wii U Home Menu Plaza.");
WUPS_PLUGIN_VERSION("v0.2-alpha");
WUPS_PLUGIN_AUTHOR("Developer");
WUPS_PLUGIN_LICENSE("GPL-3.0");

// Native Wii U Menu title IDs by region.
const uint64_t WIIU_HOME_MENU_USA = 0x0005001010040100ULL;
const uint64_t WIIU_HOME_MENU_EUR = 0x0005001010040200ULL;
const uint64_t WIIU_HOME_MENU_JPN = 0x0005001010040000ULL;

struct LayoutSlot {
    int x;
    int y;
    int width;
    int height;
};

const LayoutSlot slots[5] = {
    {100, 200, 100, 100},
    {250, 250, 150, 150},
    {450, 300, 250, 250},
    {750, 250, 150, 150},
    {950, 200, 100, 100},
};

uint64_t recentApps[10] = {0};
int currentCenterIndex = 2;
bool isMenuOverlayOpen = false;

bool isUserOnHomeMenu() {
    const uint64_t activeTitleID = SYSGetActiveTitleId();
    return activeTitleID == WIIU_HOME_MENU_USA ||
           activeTitleID == WIIU_HOME_MENU_EUR ||
           activeTitleID == WIIU_HOME_MENU_JPN;
}

void drawHeroBackgroundWallpaper(uint64_t titleID) {
    if (titleID == 0) {
        return;
    }
    // TODO: Load and draw the selected title's hero artwork.
}

void drawAppIconTexture(uint64_t titleID, int x, int y, int width, int height) {
    if (titleID == 0) {
        return;
    }
    // TODO: Load and draw the app icon at the supplied position.
}

void handleControllerInput(VPADStatus* status) {
    if (!isUserOnHomeMenu()) {
        isMenuOverlayOpen = false;
        return;
    }

    if (status->trigger & VPAD_BUTTON_MINUS) {
        isMenuOverlayOpen = !isMenuOverlayOpen;
    }

    if (isMenuOverlayOpen) {
        if ((status->trigger & VPAD_BUTTON_LEFT) && currentCenterIndex > 2) {
            currentCenterIndex--;
        }
        if ((status->trigger & VPAD_BUTTON_RIGHT) && currentCenterIndex < 7) {
            currentCenterIndex++;
        }
    }
}

void renderKioskOverlay() {
    if (!isMenuOverlayOpen || !isUserOnHomeMenu()) {
        return;
    }

    const uint64_t activeTitleID = recentApps[currentCenterIndex];
    drawHeroBackgroundWallpaper(activeTitleID);

    for (int i = 0; i < 5; i++) {
        const int appTargetIndex = currentCenterIndex - 2 + i;
        const uint64_t loopTitleID = recentApps[appTargetIndex];
        drawAppIconTexture(loopTitleID, slots[i].x, slots[i].y,
                           slots[i].width, slots[i].height);
    }
}

// Entry point. Hook installation and app-history tracking are not implemented yet.
extern "C" WUPSInitResult WUPSPluginInit() {
    return WUPS_INIT_RESULT_SUCCESS;
}
