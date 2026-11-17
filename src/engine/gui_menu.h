#ifndef FM_ENGINE_GUI_MENU_H
#define FM_ENGINE_GUI_MENU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "config.h"
#include "snes_ui_rasterizer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum FmGuiScreen {
    FM_GUI_SCREEN_NONE     = 0,
    FM_GUI_SCREEN_LAUNCHER = 1,
    FM_GUI_SCREEN_SETTINGS = 2
} FmGuiScreen;

typedef enum FmSettingsTab {
    FM_TAB_VIDEO    = 0,
    FM_TAB_AUDIO    = 1,
    FM_TAB_CONTROLS = 2,
    FM_TAB_SYSTEM   = 3,
    FM_TAB_COUNT    = 4
} FmSettingsTab;

typedef enum FmGuiAction {
    FM_GUI_ACTION_NONE            = 0,
    FM_GUI_ACTION_LAUNCH_GAME     = 1,
    FM_GUI_ACTION_RESUME_GAME     = 2,
    FM_GUI_ACTION_RETURN_LAUNCHER = 3,
    FM_GUI_ACTION_QUIT            = 4,
    FM_GUI_ACTION_CONFIG_CHANGED  = 5
} FmGuiAction;

typedef enum FmGuiKey {
    FM_GUI_KEY_NONE  = 0,
    FM_GUI_KEY_UP    = 1,
    FM_GUI_KEY_DOWN  = 2,
    FM_GUI_KEY_LEFT  = 3,
    FM_GUI_KEY_RIGHT = 4,
    FM_GUI_KEY_ENTER = 5,
    FM_GUI_KEY_ESC   = 6,
    FM_GUI_KEY_TAB   = 7
} FmGuiKey;

typedef struct FmGuiMenuState {
    FmGuiScreen screen;
    FmGuiScreen return_screen;
    FmSettingsTab active_tab;
    int selected_row;
    int hover_id;
    bool rom_verified;
    size_t rom_size_bytes;
} FmGuiMenuState;

void fm_gui_menu_init(FmGuiMenuState *gui, FmGuiScreen initial_screen, bool rom_verified, size_t rom_size_bytes);
void fm_gui_menu_toggle_settings(FmGuiMenuState *gui);
FmGuiAction fm_gui_menu_handle_key(FmGuiMenuState *gui, FmUserConfig *cfg, FmGuiKey key);
FmGuiAction fm_gui_menu_handle_mouse(FmGuiMenuState *gui, FmUserConfig *cfg, int canvas_x, int canvas_y, bool left_clicked);
void fm_gui_menu_render(const FmGuiMenuState *gui, const FmUserConfig *cfg, uint32_t *rgba_800x448);

#ifdef __cplusplus
}
#endif

#endif /* FM_ENGINE_GUI_MENU_H */
