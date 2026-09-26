```
#include <wups.h>
#include <coreinit/screen.h>
#include <coreinit/filesystem.h>
#include <vpad/input.h>
#include <string>
#include <vector>
#include <dirent.h>
#include <cstdint>
#include <cstddef>
#include <algorithm>

WUPS_PLUGIN_NAME("PVP Client Free");
WUPS_PLUGIN_DESCRIPTION("Minecraft Wii U PVP Client Free Edition");
WUPS_PLUGIN_VERSION("v1.1");
WUPS_PLUGIN_AUTHOR("Developer");
WUPS_PLUGIN_LICENSE("GPL");

enum MenuTab {
    TAB_COMBAT,
    TAB_SETTING,
    TAB_COSMETIC
};

enum SettingItem {
    ITEM_HUD = 0,
    ITEM_UHC,
    ITEM_MODS,
    ITEM_SOUND,
    ITEM_ORIGINAL,
    ITEM_BOOST,
    ITEM_MAX_COUNT
};

enum RightSubMenu {
    SUBMENU_NONE,
    SUBMENU_HUD,
    SUBMENU_UHC,
    SUBMENU_MODS,
    SUBMENU_SOUND,
    SUBMENU_ORIGINAL,
    SUBMENU_BOOST
};

struct MenuState {
    bool isOpen = false;
    bool isLeftMenuFocused = true;
    RightSubMenu currentSubMenu = SUBMENU_NONE;
    int selectedLeftIndex = 0;
    int gridX = 0;
    int gridY = 0;
};

MenuState g_Menu;

struct HudModules {
    bool fpsdisplay = false;
    bool keyStroke = false;
    bool cpsdisplay = false;
    bool armorhub = false;
    bool potiondisplay = false;
    bool nametag = false;
    bool tntTimer = false;
    bool reachhub = false;
    bool combohub = false;
};

HudModules g_HudModules;

struct UhcModules {
    bool guardCancel = false;
    bool customFov = false;
    bool noEffects = false;
};

UhcModules g_UhcModules;

struct ModsModules {
    bool godbridge = false;
    bool badwars = false;
    bool skywars = false;
    bool bilduhc = false;
};

ModsModules g_ModsModules;

struct SoundModules {
    bool play = false;
    bool stop = false;
};

SoundModules g_SoundModules;

struct OriginalModules {
    bool worldedit = false;
    bool tradeModification = false;
};

OriginalModules g_OriginalModules;

struct BoostModules {
    bool fpsboost = false;
    bool cpsboost = false;
};

BoostModules g_BoostModules;

struct MinecraftPlayer {
    int inventory = 0;
    int enderChest = 0;
    int armor = 0;
    int protectionLevel = 0;
    int sharpnessLevel = 0;
    int pickaxeTier = 0;
    int axeTier = 0;
    bool isDead = false;
    int respawnTimer = 0;
    int killCount = 0;
    bool hasPerkBulldozer = false;
    int gappleCount = 0;
    int headCount = 0;
};

struct ItemGenerator {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    int blockType = 0;
    int tickCounter = 0;
};

std::vector<ItemGenerator> g_Generators;
std::vector<std::string> g_MusicPlaylist;

size_t g_CurrentTrackIndex = 0;
bool g_IsPlayingMusic = false;

#define MCA_IS_BLOCKING_PATCH_ADDR   0x03501234u
#define MCA_DAMAGE_REDUCE_PATCH_ADDR 0x03505678u
#define MCA_VSYNC_PATCH_ADDR         0x03E01122u
#define MCA_FPS_LIMIT_PATCH_ADDR     0x03E03344u

uint32_t originalBlockingInstruction = 0;
uint32_t originalDamageInstruction = 0;
uint32_t originalVsyncInstruction = 0;
uint32_t originalFpsLimitValue = 0;

static inline uint32_t readMemory32(uint32_t address) {
    volatile uint32_t* ptr =
        reinterpret_cast<volatile uint32_t*>(static_cast<uintptr_t>(address));
    return *ptr;
}

static inline void writeMemory32(uint32_t address, uint32_t value) {
    volatile uint32_t* ptr =
        reinterpret_cast<volatile uint32_t*>(static_cast<uintptr_t>(address));
    *ptr = value;
}

void applyGuardCancelPatch(bool enable) {
    if (enable) {
        if (originalBlockingInstruction == 0) {
            originalBlockingInstruction =
                readMemory32(MCA_IS_BLOCKING_PATCH_ADDR);
        }

        if (originalDamageInstruction == 0) {
            originalDamageInstruction =
                readMemory32(MCA_DAMAGE_REDUCE_PATCH_ADDR);
        }

        writeMemory32(MCA_IS_BLOCKING_PATCH_ADDR, 0x60000000u);
        writeMemory32(MCA_DAMAGE_REDUCE_PATCH_ADDR, 0x38600001u);
    } else {
        if (originalBlockingInstruction != 0) {
            writeMemory32(
                MCA_IS_BLOCKING_PATCH_ADDR,
                originalBlockingInstruction
            );
        }

        if (originalDamageInstruction != 0) {
            writeMemory32(
                MCA_DAMAGE_REDUCE_PATCH_ADDR,
                originalDamageInstruction
            );
        }
    }
}

void applyFpsBoostPatch(bool enable) {
    if (enable) {
        if (originalVsyncInstruction == 0) {
            originalVsyncInstruction =
                readMemory32(MCA_VSYNC_PATCH_ADDR);
        }

        if (originalFpsLimitValue == 0) {
            originalFpsLimitValue =
                readMemory32(MCA_FPS_LIMIT_PATCH_ADDR);
        }

        writeMemory32(MCA_VSYNC_PATCH_ADDR, 0x60000000u);
        writeMemory32(MCA_FPS_LIMIT_PATCH_ADDR, 600u);
    } else {
        if (originalVsyncInstruction != 0) {
            writeMemory32(
                MCA_VSYNC_PATCH_ADDR,
                originalVsyncInstruction
            );
        }

        if (originalFpsLimitValue != 0) {
            writeMemory32(
                MCA_FPS_LIMIT_PATCH_ADDR,
                originalFpsLimitValue
            );
        }
    }
}

void scanMusicFolder() {
    g_MusicPlaylist.clear();

    const char* dirPath = "sd:/WIIU/Azeraclient/music";
    DIR* dir = opendir(dirPath);

    if (dir == nullptr) {
        return;
    }

    struct dirent* entry = nullptr;

    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name == nullptr) {
            continue;
        }

        std::string fileName(entry->d_name);

        if (fileName.length() < 4) {
            continue;
        }

        std::string extension =
            fileName.substr(fileName.length() - 4);

        std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            }
        );

        if (extension == ".mp3") {
            g_MusicPlaylist.emplace_back(
                std::string(dirPath) + "/" + fileName
            );
        }
    }

    closedir(dir);
}

void playTrack(size_t index) {
    if (index >= g_MusicPlaylist.size()) {
        g_IsPlayingMusic = false;
        return;
    }

    g_CurrentTrackIndex = index;
    g_IsPlayingMusic = true;
}

void updateMusicLoop() {
    if (!g_IsPlayingMusic || g_MusicPlaylist.empty()) {
        return;
    }

    /*
     * Track-end detection/playback must be connected to the
     * actual game's audio system. This function only maintains
     * playlist state.
     */
}

void toggleHudModule(int x, int y) {
    if (x < 0 || x > 2 || y < 0 || y > 2) {
        return;
    }

    if (y == 0 && x == 0)
        g_HudModules.fpsdisplay = !g_HudModules.fpsdisplay;
    else if (y == 0 && x == 1)
        g_HudModules.keyStroke = !g_HudModules.keyStroke;
    else if (y == 0 && x == 2)
        g_HudModules.cpsdisplay = !g_HudModules.cpsdisplay;
    else if (y == 1 && x == 0)
        g_HudModules.armorhub = !g_HudModules.armorhub;
    else if (y == 1 && x == 1)
        g_HudModules.potiondisplay = !g_HudModules.potiondisplay;
    else if (y == 1 && x == 2)
        g_HudModules.nametag = !g_HudModules.nametag;
    else if (y == 2 && x == 0)
        g_HudModules.tntTimer = !g_HudModules.tntTimer;
    else if (y == 2 && x == 1)
        g_HudModules.reachhub = !g_HudModules.reachhub;
    else if (y == 2 && x == 2)
        g_HudModules.combohub = !g_HudModules.combohub;
}

void toggleUhcModule(int x) {
    switch (x) {
        case 0:
            g_UhcModules.guardCancel = !g_UhcModules.guardCancel;
            applyGuardCancelPatch(g_UhcModules.guardCancel);
            break;

        case 1:
            g_UhcModules.customFov = !g_UhcModules.customFov;
            break;

        case 2:
            g_UhcModules.noEffects = !g_UhcModules.noEffects;
            break;

        default:
            break;
    }
}

void toggleModsModule(int x, int y) {
    if (x < 0 || x > 2 || y < 0 || y > 1) {
        return;
    }

    if (y == 0 && x == 0)
        g_ModsModules.godbridge = !g_ModsModules.godbridge;
    else if (y == 0 && x == 1)
        g_ModsModules.badwars = !g_ModsModules.badwars;
    else if (y == 0 && x == 2)
        g_ModsModules.skywars = !g_ModsModules.skywars;
    else if (y == 1 && x == 0)
        g_ModsModules.bilduhc = !g_ModsModules.bilduhc;
}

void toggleSoundModule(int x) {
    if (x == 0) {
        if (g_MusicPlaylist.empty()) {
            scanMusicFolder();
        }

        if (!g_MusicPlaylist.empty()) {
            g_CurrentTrackIndex = 0;
            g_SoundModules.play = true;
            g_SoundModules.stop = false;
            playTrack(g_CurrentTrackIndex);
        }
    } else if (x == 1) {
        g_SoundModules.stop = true;
        g_SoundModules.play = false;
        g_IsPlayingMusic = false;
    }
}

void toggleOriginalModule(int x) {
    if (x == 0)
        g_OriginalModules.worldedit =
            !g_OriginalModules.worldedit;
    else if (x == 1)
        g_OriginalModules.tradeModification =
            !g_OriginalModules.tradeModification;
}

void toggleBoostModule(int x) {
    if (x == 0) {
        g_BoostModules.fpsboost =
            !g_BoostModules.fpsboost;

        applyFpsBoostPatch(g_BoostModules.fpsboost);
    } else if (x == 1) {
        g_BoostModules.cpsboost =
            !g_BoostModules.cpsboost;
    }
}

void handleGodbridge(
    MinecraftPlayer* player,
    VPADStatus* vpad
) {
    if (player == nullptr || vpad == nullptr) {
        return;
    }

    if (!g_ModsModules.godbridge) {
        return;
    }

    if (vpad->leftStickY < -0.5f &&
        (vpad->hold & VPAD_BUTTON_ZL)) {
        // Game-specific godbridge implementation goes here.
    }
}

void updateBedwarsGenerators() {
    if (!g_ModsModules.badwars) {
        return;
    }

    for (auto& gen : g_Generators) {
        if (gen.tickCounter < 0) {
            gen.tickCounter = 0;
        }

        ++gen.tickCounter;

        int interval = 0;

        switch (gen.blockType) {
            case 1:
                interval = 10;
                break;

            case 2:
                interval = 70;
                break;

            case 3:
                interval = 600;
                break;

            case 4:
                interval = 580;
                break;

            default:
                break;
        }

        if (interval > 0 &&
            gen.tickCounter >= interval) {
            gen.tickCounter = 0;
        }
    }
}

void handleQuickEnderChestDeposit(
    MinecraftPlayer* player,
    VPADStatus* vpad
) {
    if (player == nullptr || vpad == nullptr) {
        return;
    }

    if (!g_ModsModules.badwars) {
        return;
    }

    if (vpad->trigger & VPAD_BUTTON_X) {
        // Game-specific inventory operation goes here.
    }
}

void updateRespawnLogic(MinecraftPlayer* player) {
    if (player == nullptr) {
        return;
    }

    if (!g_ModsModules.badwars ||
        !player->isDead) {
        return;
    }

    if (player->respawnTimer > 0) {
        --player->respawnTimer;
    } else {
        player->isDead = false;
    }
}

bool onCalculateDamage(
    void* attackerEntity,
    void* victimEntity,
    int damageType
) {
    (void)attackerEntity;
    (void)victimEntity;

    if (g_ModsModules.badwars &&
        damageType == 3) {
        return false;
    }

    return true;
}

void resetRightMenuSelection() {
    g_Menu.gridX = 0;
    g_Menu.gridY = 0;
}

void closeRightMenu() {
    g_Menu.isLeftMenuFocused = true;
    g_Menu.currentSubMenu = SUBMENU_NONE;
    resetRightMenuSelection();
}

void openSubMenu(RightSubMenu submenu) {
    g_Menu.isLeftMenuFocused = false;
    g_Menu.currentSubMenu = submenu;
    resetRightMenuSelection();
}

void handleLeftMenuInput(uint32_t pressed) {
    if (pressed & VPAD_BUTTON_UP) {
        if (g_Menu.selectedLeftIndex > 0) {
            --g_Menu.selectedLeftIndex;
        }
    }

    if (pressed & VPAD_BUTTON_DOWN) {
        if (g_Menu.selectedLeftIndex <
            ITEM_MAX_COUNT - 1) {
            ++g_Menu.selectedLeftIndex;
        }
    }

    if (pressed & VPAD_BUTTON_RIGHT ||
        pressed & VPAD_BUTTON_A) {
        switch (g_Menu.selectedLeftIndex) {
            case ITEM_HUD:
                openSubMenu(SUBMENU_HUD);
                break;

            case ITEM_UHC:
                openSubMenu(SUBMENU_UHC);
                break;

            case ITEM_MODS:
                openSubMenu(SUBMENU_MODS);
                break;

            case ITEM_SOUND:
                openSubMenu(SUBMENU_SOUND);
                break;

            case ITEM_ORIGINAL:
                openSubMenu(SUBMENU_ORIGINAL);
                break;

            case ITEM_BOOST:
                openSubMenu(SUBMENU_BOOST);
                break;

            default:
                break;
        }
    }
}

void handleHudMenuInput(uint32_t pressed) {
    if (pressed & VPAD_BUTTON_UP) {
        if (g_Menu.gridY > 0) {
            --g_Menu.gridY;
        }
    }

    if (pressed & VPAD_BUTTON_DOWN) {
        if (g_Menu.gridY < 2) {
            ++g_Menu.gridY;
        }
    }

    if (pressed & VPAD_BUTTON_RIGHT) {
        if (g_Menu.gridX < 2) {
            ++g_Menu.gridX;
        }
    }

    if (pressed & VPAD_BUTTON_LEFT) {
        if (g_Menu.gridX > 0) {
            --g_Menu.gridX;
        } else {
            closeRightMenu();
            return;
        }
    }

    if (pressed & VPAD_BUTTON_A) {
        toggleHudModule(
            g_Menu.gridX,
            g_Menu.gridY
        );
    }
}

void handleUhcMenuInput(uint32_t pressed) {
    if (pressed & VPAD_BUTTON_RIGHT) {
        if (g_Menu.gridX < 2) {
            ++g_Menu.gridX;
        }
    }

    if (pressed & VPAD_BUTTON_LEFT) {
        if (g_Menu.gridX > 0) {
            --g_Menu.gridX;
        } else {
            closeRightMenu();
            return;
        }
    }

    if (pressed & VPAD_BUTTON_A) {
        toggleUhcModule(g_Menu.gridX);
    }
}

void handleModsMenuInput(uint32_t pressed) {
    if (pressed & VPAD_BUTTON_UP) {
        if (g_Menu.gridY > 0) {
            --g_Menu.gridY;

            if (g_Menu.gridY == 0 &&
                g_Menu.gridX > 2) {
                g_Menu.gridX = 2;
            }
        }
    }

    if (pressed & VPAD_BUTTON_DOWN) {
        if (g_Menu.gridY < 1) {
            ++g_Menu.gridY;

            if (g_Menu.gridY == 1 &&
                g_Menu.gridX > 0) {
                g_Menu.gridX = 0;
            }
        }
    }

    if (pressed & VPAD_BUTTON_RIGHT) {
        if (g_Menu.gridY == 0 &&
            g_Menu.gridX < 2) {
            ++g_Menu.gridX;
        }
    }

    if (pressed & VPAD_BUTTON_LEFT) {
        if (g_Menu.gridX > 0) {
            --g_Menu.gridX;
        } else {
            closeRightMenu();
            return;
        }
    }

    if (pressed & VPAD_BUTTON_A) {
        toggleModsModule(
            g_Menu.gridX,
            g_Menu.gridY
        );
    }
}

void handleSoundMenuInput(uint32_t pressed) {
    if (pressed & VPAD_BUTTON_RIGHT) {
        if (g_Menu.gridX < 1) {
            ++g_Menu.gridX;
        }
    }

    if (pressed & VPAD_BUTTON_LEFT) {
        if (g_Menu.gridX > 0) {
            --g_Menu.gridX;
        } else {
            closeRightMenu();
            return;
        }
    }

    if (pressed & VPAD_BUTTON_A) {
        toggleSoundModule(g_Menu.gridX);
    }
}

void handleOriginalMenuInput(uint32_t pressed) {
    if (pressed & VPAD_BUTTON_RIGHT) {
        if (g_Menu.gridX < 1) {
            ++g_Menu.gridX;
        }
    }

    if (pressed & VPAD_BUTTON_LEFT) {
        if (g_Menu.gridX > 0) {
            --g_Menu.gridX;
        } else {
            closeRightMenu();
            return;
        }
    }

    if (pressed & VPAD_BUTTON_A) {
        toggleOriginalModule(g_Menu.gridX);
    }
}

void handleBoostMenuInput(uint32_t pressed) {
    if (pressed & VPAD_BUTTON_RIGHT) {
        if (g_Menu.gridX < 1) {
            ++g_Menu.gridX;
        }
    }

    if (pressed & VPAD_BUTTON_LEFT) {
        if (g_Menu.gridX > 0) {
            --g_Menu.gridX;
        } else {
            closeRightMenu();
            return;
        }
    }

    if (pressed & VPAD_BUTTON_A) {
        toggleBoostModule(g_Menu.gridX);
    }
}

void handleRightMenuInput(uint32_t pressed) {
    switch (g_Menu.currentSubMenu) {
        case SUBMENU_HUD:
            handleHudMenuInput(pressed);
            break;

        case SUBMENU_UHC:
            handleUhcMenuInput(pressed);
            break;

        case SUBMENU_MODS:
            handleModsMenuInput(pressed);
            break;

        case SUBMENU_SOUND:
            handleSoundMenuInput(pressed);
            break;

        case SUBMENU_ORIGINAL:
            handleOriginalMenuInput(pressed);
            break;

        case SUBMENU_BOOST:
            handleBoostMenuInput(pressed);
            break;

        default:
            closeRightMenu();
            break;
    }
}

void onGameUpdate() {
    VPADStatus vpad{};
    VPADReadError error = VPAD_READ_SUCCESS;

    VPADRead(
        VPAD_CHAN_0,
        &vpad,
        1,
        &error
    );

    if (error != VPAD_READ_SUCCESS) {
        return;
    }

    const uint32_t pressed = vpad.trigger;

    if ((vpad.hold & VPAD_BUTTON_L) &&
        (vpad.hold & VPAD_BUTTON_R)) {
        if ((pressed & VPAD_BUTTON_L) ||
            (pressed & VPAD_BUTTON_R)) {
            g_Menu.isOpen = !g_Menu.isOpen;

            if (!g_Menu.isOpen) {
                closeRightMenu();
            }

            return;
        }
    }

    if (!g_Menu.isOpen) {
        updateMusicLoop();
        updateBedwarsGenerators();
        return;
    }

    if (g_Menu.isLeftMenuFocused) {
        handleLeftMenuInput(pressed);
    } else {
        handleRightMenuInput(pressed);
    }

    updateMusicLoop();
    updateBedwarsGenerators();
}
```
