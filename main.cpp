#include <wups.h>
#include <coreinit/screen.h>
#include <vpad/input.h>
#include <sysapp/title.h>

// WUPS Plugin Metadata Configuration
WUPS_PLUGIN_NAME("WiiUKioskRecentApps");
WUPS_PLUGIN_DESCRIPTION("A kiosk overlay restricted strictly to the Wii U Home Menu Plaza.");
WUPS_PLUGIN_VERSION("v0.2-alpha");
WUPS_PLUGIN_AUTHOR("Developer");
WUPS_PLUGIN_LICENSE("GPL-3.0");

// Hardcoded Title IDs for the native Wii U Home Menu dashboard layers across regions
const uint64_t WIIU_HOME_MENU_USA = 0x0005001010040100ULL;
const uint64_t WIIU_HOME_MENU_EUR = 0x0005001010040200ULL;
const uint64_t WIIU_HOME_MENU_JPN = 0x0005001010040000ULL;

// Struct to store our bare-bones 2D layout coordinate slots
struct LayoutSlot {
    int x;
    int y;
    int width;
    int height;
};

// Fixed visual curve layout slots mapping out position profiles flat on screen
const LayoutSlot slots[5] = {
    { 100, 200, 100, 100 }, // Slot 1: Far Left (Small, high up)
    { 250, 250, 150, 150 }, // Slot 2: Mid Left (Medium, slightly lower)
    { 450, 300, 250, 250 }, // Slot 3: ACTIVE CENTER (Large, lowest point)
    { 750, 250, 150, 150 }, // Slot 4: Mid Right (Medium, higher up)
    { 950, 200, 100, 100 }  // Slot 5: Far Right (Small, high up)
};

// Global state variables tracking plugin status
uint64_t recentApps[10] = {0};   // Array storing the rolling historical log of the last 10 launched Title IDs
int currentCenterIndex = 2;      // Bounds-checked index pointer tracking the active center item
bool isMenuOverlayOpen = false;  // Visibility flag for drawing the UI onto the frame buffer

// Internal safety check verifying if the user is currently sitting on the main dashboard plaza
bool isUserOnHomeMenu() {
    uint64_t activeTitleID = SYSGetActiveTitleId(); // Query the native OS kernel process
    
    if (activeTitleID == WIIU_HOME_MENU_USA || 
        activeTitleID == WIIU_HOME_MENU_EUR || 
        activeTitleID == WIIU_HOME_MENU_JPN) {
        return true;
    }
    return false;
}

// Mock helper function to draw the full-screen "Hero Art" backdrop texture
void drawHeroBackgroundWallpaper(uint64_t titleID) {
    if (titleID == 0) {
        // Fallback placeholder logic if no game profile exists in the history log slot
        return;
    }
    // Code linking with GX2 graphics pipe to safely load and paint 'bootTvTex.tga' goes here
}

// Mock helper function to draw a single 2D game icon texture onto a specific slot assignment
void drawAppIconTexture(uint64_t titleID, int x, int y, int width, int height) {
    if (titleID == 0) {
        // Fallback placeholder boundary check if index slot references a null array profile
        return;
    }
    // Code passing position variables down to the hardware display pipelines goes here
}

// Controller intercept pipeline hook (Called natively by WUPS on every input loop pass)
void handleControllerInput(VPADStatus* status) {
    // CRITICAL SAFETY SHIELD: If a game is active, instantly force-close overlay parameters 
    // and skip input tracking to prevent memory allocation collisions or storage bottlenecks.
    if (!isUserOnHomeMenu()) {
        isMenuOverlayOpen = false;
        return; 
    }

    // Intercepts the physical MINUS button tap to instantly toggle visual overlays
    if (status->trigger & VPAD_BUTTON_MINUS) {
        isMenuOverlayOpen = !isMenuOverlayOpen; 
    }

    // Navigation execution paths active ONLY while the dashboard framework layer is pulled up
    if (isMenuOverlayOpen) {
        // Tap Left: Instantly shift layout array profile left
        if (status->trigger & VPAD_BUTTON_LEFT) {
            if (currentCenterIndex > 2) currentCenterIndex--; 
        }
        // Tap Right: Instantly shift layout array profile right
        if (status->trigger & VPAD_BUTTON_RIGHT) {
            if (currentCenterIndex < 7) currentCenterIndex++; 
        }
    }
}

// System frame buffer rendering callback engine (Called natively by WUPS during screen draw cycles)
void renderKioskOverlay() {
    // HARDWARE INTERLOCK: Instantly kill the render process loop if menu visibility 
    // is toggled off OR if the user leaves the home menu environment to launch software.
    if (!isMenuOverlayOpen || !isUserOnHomeMenu()) {
        return;
    }

    // Grab the active Title ID assignment resting directly inside the center index
    uint64_t activeTitleID = recentApps[currentCenterIndex];
    drawHeroBackgroundWallpaper(activeTitleID); 

    // Render loop assigning targeted digital assets straight down into hardcoded slots
    for (int i = 0; i < 5; i++) {
        // Offset mapping to calculate array slots relative to current active center point
        int appTargetIndex = currentCenterIndex - 2 + i;
        uint64_t loopTitleID = recentApps[appTargetIndex];

        // Pipeline execution to draw textures flat using our fixed blueprint configuration
        drawAppIconTexture(
            loopTitleID, 
            slots[i].x, 
            slots[i].y, 
            slots[i].width, 
            slots[i].height
        );
    }
}

// Mandatory WUPS Entry Initialization Point called natively on Aroma plugin activation
extern "C" WUPSInitResult WUPSPluginInit() {
    // Hook installation routines and backend function setup procedures go here
    return WUPS_INIT_RESULT_SUCCESS;
}
