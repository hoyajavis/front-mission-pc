#include "gui_menu.h"

#include <stdio.h>
#include <string.h>

#define CANVAS_W 800
#define CANVAS_H 448

typedef struct GuiRect {
    int x, y, w, h;
} GuiRect;

static bool pt_in_rect(int px, int py, GuiRect r) {
    return px >= r.x && px < (r.x + r.w) && py >= r.y && py < (r.y + r.h);
}

static void dim_framebuffer(uint32_t *rgba, int w, int h) {
    int count = w * h;
    for (int i = 0; i < count; ++i) {
        uint32_t c = rgba[i];
        uint32_t r = ((c >> 16) & 0xFFu) >> 1;
        uint32_t g = ((c >> 8) & 0xFFu) >> 1;
        uint32_t b = (c & 0xFFu) >> 1;
        rgba[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
}

static void draw_button(FmSnesCanvas *c, GuiRect r, const char *label, bool selected, bool hovered, FmSnesPaletteId pal) {
    draw_snes_9slice_box(c, r.x, r.y, r.w, r.h, true);
    if (selected || hovered) {
        draw_snes_rect_outline(c, r.x + 4, r.y + 4, r.w - 8, r.h - 8,
                               fm_snes_palette_color(selected ? FM_PAL_GOLD : FM_PAL_CYAN));
    }
    int text_w = (int)strlen(label) * 8;
    int tx = r.x + (r.w - text_w) / 2;
    int ty = r.y + (r.h - 8) / 2;
    draw_snes_tile_text(c, tx, ty, label, (selected || hovered) ? FM_PAL_GOLD : pal);
}

static void draw_selectable_row_bg(FmSnesCanvas *c, GuiRect r, bool active, bool hovered) {
    if (active) {
        draw_snes_rect_fill(c, r.x, r.y, r.w, r.h, 0xFF304888u);
        draw_snes_rect_outline(c, r.x, r.y, r.w, r.h, fm_snes_palette_color(FM_PAL_GOLD));
    } else if (hovered) {
        draw_snes_rect_fill(c, r.x, r.y, r.w, r.h, 0xFF22366Eu);
        draw_snes_rect_outline(c, r.x, r.y, r.w, r.h, fm_snes_palette_color(FM_PAL_CYAN));
    } else {
        draw_snes_rect_fill(c, r.x, r.y, r.w, r.h, 0xFF101848u);
        draw_snes_rect_outline(c, r.x, r.y, r.w, r.h, 0xFF384878u);
    }
}

/* Launcher: rows 0..2 profiles, 3 auto_boot, 4 Launch, 5 Settings, 6 Quit */
#define LAUNCHER_ROWS 7

static GuiRect launcher_profile_rect(int i) {
    return (GuiRect){ 104, 116 + i * 38, 300, 30 };
}

static GuiRect launcher_autoboot_rect(void) {
    return (GuiRect){ 424, 154, 272, 30 };
}

static const GuiRect k_btn_launch   = { 128, 352, 216, 40 };
static const GuiRect k_btn_settings = { 368, 352, 160, 40 };
static const GuiRect k_btn_quit     = { 552, 352, 120, 40 };

static GuiRect settings_tab_rect(int t) {
    return (GuiRect){ 136 + t * 134, 80, 126, 24 };
}

static GuiRect settings_row_rect(int row) {
    return (GuiRect){ 144, 124 + row * 40, 512, 32 };
}

static const GuiRect k_btn_resume = { 288, 360, 224, 32 };

static int settings_row_count(FmSettingsTab tab) {
    switch (tab) {
        case FM_TAB_VIDEO:    return 5;
        case FM_TAB_AUDIO:    return 2;
        case FM_TAB_CONTROLS: return 4;
        case FM_TAB_SYSTEM:   return 3;
        default:              return 0;
    }
}

void fm_gui_menu_init(FmGuiMenuState *gui, FmGuiScreen initial_screen, bool rom_verified, size_t rom_size_bytes) {
    if (!gui) return;
    memset(gui, 0, sizeof(*gui));
    gui->screen = initial_screen;
    gui->return_screen = FM_GUI_SCREEN_NONE;
    gui->active_tab = FM_TAB_VIDEO;
    gui->selected_row = 0;
    gui->hover_id = -1;
    gui->rom_verified = rom_verified;
    gui->rom_size_bytes = rom_size_bytes;
}

void fm_gui_menu_toggle_settings(FmGuiMenuState *gui) {
    if (!gui) return;
    if (gui->screen == FM_GUI_SCREEN_NONE) {
        gui->screen = FM_GUI_SCREEN_SETTINGS;
        gui->return_screen = FM_GUI_SCREEN_NONE;
        gui->selected_row = 0;
        gui->hover_id = -1;
    } else if (gui->screen == FM_GUI_SCREEN_SETTINGS) {
        gui->screen = gui->return_screen;
        gui->return_screen = FM_GUI_SCREEN_NONE;
        gui->selected_row = 0;
        gui->hover_id = -1;
    }
}

static FmGuiAction activate_launcher_row(FmGuiMenuState *gui, FmUserConfig *cfg, int row) {
    if (row >= 0 && row < FM_PROFILE_COUNT) {
        cfg->boot_profile = (FmBootProfileId)row;
        return FM_GUI_ACTION_CONFIG_CHANGED;
    }
    if (row == 3) {
        cfg->auto_boot = !cfg->auto_boot;
        return FM_GUI_ACTION_CONFIG_CHANGED;
    }
    if (row == 4) {
        return FM_GUI_ACTION_LAUNCH_GAME;
    }
    if (row == 5) {
        gui->return_screen = FM_GUI_SCREEN_LAUNCHER;
        gui->screen = FM_GUI_SCREEN_SETTINGS;
        gui->selected_row = 0;
        gui->hover_id = -1;
        return FM_GUI_ACTION_NONE;
    }
    if (row == 6) {
        return FM_GUI_ACTION_QUIT;
    }
    return FM_GUI_ACTION_NONE;
}

static FmGuiAction adjust_settings_row(FmGuiMenuState *gui, FmUserConfig *cfg, int row, int dir) {
    switch (gui->active_tab) {
        case FM_TAB_VIDEO:
            if (row == 0) {
                cfg->display_mode = (uint8_t)((cfg->display_mode + 3 + dir) % 3);
                return FM_GUI_ACTION_CONFIG_CHANGED;
            }
            if (row == 1) {
                int s = (int)cfg->window_scale + (dir == 0 ? 1 : dir);
                if (s > 4) s = 1;
                if (s < 1) s = 4;
                cfg->window_scale = (uint8_t)s;
                return FM_GUI_ACTION_CONFIG_CHANGED;
            }
            if (row == 2) { cfg->fullscreen = !cfg->fullscreen; return FM_GUI_ACTION_CONFIG_CHANGED; }
            if (row == 3) { cfg->crt_scanlines = !cfg->crt_scanlines; return FM_GUI_ACTION_CONFIG_CHANGED; }
            if (row == 4) { cfg->vsync = !cfg->vsync; return FM_GUI_ACTION_CONFIG_CHANGED; }
            break;
        case FM_TAB_AUDIO:
            if (row == 0) {
                int step = (dir == 0) ? 10 : (dir * 10);
                int v = (int)cfg->master_volume + step;
                if (v > 100) v = (dir == 0) ? 0 : 100;
                if (v < 0) v = 0;
                cfg->master_volume = (uint8_t)v;
                return FM_GUI_ACTION_CONFIG_CHANGED;
            }
            if (row == 1) { cfg->mute = !cfg->mute; return FM_GUI_ACTION_CONFIG_CHANGED; }
            break;
        case FM_TAB_CONTROLS:
            if (row == 0) { cfg->mouse_enabled = !cfg->mouse_enabled; return FM_GUI_ACTION_CONFIG_CHANGED; }
            if (row == 1) { cfg->edge_pan_enabled = !cfg->edge_pan_enabled; return FM_GUI_ACTION_CONFIG_CHANGED; }
            if (row == 2) {
                int sp = (int)cfg->edge_pan_speed + (dir == 0 ? 1 : dir);
                if (sp > 8) sp = 2;
                if (sp < 2) sp = 8;
                cfg->edge_pan_speed = (uint8_t)sp;
                return FM_GUI_ACTION_CONFIG_CHANGED;
            }
            if (row == 3) {
                if (cfg->fast_forward == 1) cfg->fast_forward = 2;
                else if (cfg->fast_forward == 2) cfg->fast_forward = 4;
                else cfg->fast_forward = 1;
                return FM_GUI_ACTION_CONFIG_CHANGED;
            }
            break;
        case FM_TAB_SYSTEM:
            if (row == 0) {
                cfg->auto_boot = !cfg->auto_boot;
                return FM_GUI_ACTION_CONFIG_CHANGED;
            }
            if (row == 1) {
                gui->screen = FM_GUI_SCREEN_LAUNCHER;
                gui->return_screen = FM_GUI_SCREEN_NONE;
                gui->selected_row = 0;
                return FM_GUI_ACTION_RETURN_LAUNCHER;
            }
            if (row == 2) {
                return FM_GUI_ACTION_QUIT;
            }
            break;
        default:
            break;
    }
    return FM_GUI_ACTION_NONE;
}

FmGuiAction fm_gui_menu_handle_key(FmGuiMenuState *gui, FmUserConfig *cfg, FmGuiKey key) {
    if (!gui || !cfg || gui->screen == FM_GUI_SCREEN_NONE) return FM_GUI_ACTION_NONE;

    if (gui->screen == FM_GUI_SCREEN_LAUNCHER) {
        if (key == FM_GUI_KEY_UP) {
            gui->selected_row = (gui->selected_row + LAUNCHER_ROWS - 1) % LAUNCHER_ROWS;
        } else if (key == FM_GUI_KEY_DOWN) {
            gui->selected_row = (gui->selected_row + 1) % LAUNCHER_ROWS;
        } else if (key == FM_GUI_KEY_ENTER) {
            return activate_launcher_row(gui, cfg, gui->selected_row);
        } else if (key == FM_GUI_KEY_ESC) {
            return FM_GUI_ACTION_QUIT;
        }
        return FM_GUI_ACTION_NONE;
    }

    int rows = settings_row_count(gui->active_tab) + 1; /* +1 for Resume/Back */
    if (key == FM_GUI_KEY_UP) {
        gui->selected_row = (gui->selected_row + rows - 1) % rows;
    } else if (key == FM_GUI_KEY_DOWN) {
        gui->selected_row = (gui->selected_row + 1) % rows;
    } else if (key == FM_GUI_KEY_TAB) {
        gui->active_tab = (FmSettingsTab)((gui->active_tab + 1) % FM_TAB_COUNT);
        gui->selected_row = 0;
    } else if (key == FM_GUI_KEY_LEFT) {
        if (gui->selected_row < rows - 1) {
            return adjust_settings_row(gui, cfg, gui->selected_row, -1);
        }
    } else if (key == FM_GUI_KEY_RIGHT) {
        if (gui->selected_row < rows - 1) {
            return adjust_settings_row(gui, cfg, gui->selected_row, 1);
        }
    } else if (key == FM_GUI_KEY_ENTER) {
        if (gui->selected_row == rows - 1) {
            gui->screen = gui->return_screen;
            gui->return_screen = FM_GUI_SCREEN_NONE;
            gui->selected_row = 0;
            return (gui->screen == FM_GUI_SCREEN_NONE) ? FM_GUI_ACTION_RESUME_GAME : FM_GUI_ACTION_NONE;
        }
        return adjust_settings_row(gui, cfg, gui->selected_row, 0);
    } else if (key == FM_GUI_KEY_ESC) {
        gui->screen = gui->return_screen;
        gui->return_screen = FM_GUI_SCREEN_NONE;
        gui->selected_row = 0;
        return (gui->screen == FM_GUI_SCREEN_NONE) ? FM_GUI_ACTION_RESUME_GAME : FM_GUI_ACTION_NONE;
    }
    return FM_GUI_ACTION_NONE;
}

FmGuiAction fm_gui_menu_handle_mouse(FmGuiMenuState *gui, FmUserConfig *cfg, int canvas_x, int canvas_y, bool left_clicked) {
    if (!gui || !cfg || gui->screen == FM_GUI_SCREEN_NONE) return FM_GUI_ACTION_NONE;
    gui->hover_id = -1;

    if (gui->screen == FM_GUI_SCREEN_LAUNCHER) {
        for (int i = 0; i < FM_PROFILE_COUNT; ++i) {
            if (pt_in_rect(canvas_x, canvas_y, launcher_profile_rect(i))) {
                gui->hover_id = i;
                if (left_clicked) {
                    gui->selected_row = i;
                    return activate_launcher_row(gui, cfg, i);
                }
            }
        }
        if (pt_in_rect(canvas_x, canvas_y, launcher_autoboot_rect())) {
            gui->hover_id = 3;
            if (left_clicked) {
                gui->selected_row = 3;
                return activate_launcher_row(gui, cfg, 3);
            }
        }
        if (pt_in_rect(canvas_x, canvas_y, k_btn_launch)) {
            gui->hover_id = 4;
            if (left_clicked) return FM_GUI_ACTION_LAUNCH_GAME;
        }
        if (pt_in_rect(canvas_x, canvas_y, k_btn_settings)) {
            gui->hover_id = 5;
            if (left_clicked) return activate_launcher_row(gui, cfg, 5);
        }
        if (pt_in_rect(canvas_x, canvas_y, k_btn_quit)) {
            gui->hover_id = 6;
            if (left_clicked) return FM_GUI_ACTION_QUIT;
        }
        return FM_GUI_ACTION_NONE;
    }

    for (int t = 0; t < FM_TAB_COUNT; ++t) {
        if (pt_in_rect(canvas_x, canvas_y, settings_tab_rect(t))) {
            gui->hover_id = 100 + t;
            if (left_clicked) {
                gui->active_tab = (FmSettingsTab)t;
                gui->selected_row = 0;
                return FM_GUI_ACTION_NONE;
            }
        }
    }
    int rows = settings_row_count(gui->active_tab);
    for (int r = 0; r < rows; ++r) {
        if (pt_in_rect(canvas_x, canvas_y, settings_row_rect(r))) {
            gui->hover_id = r;
            if (left_clicked) {
                gui->selected_row = r;
                return adjust_settings_row(gui, cfg, r, 0);
            }
        }
    }
    if (pt_in_rect(canvas_x, canvas_y, k_btn_resume)) {
        gui->hover_id = 200;
        if (left_clicked) {
            gui->screen = gui->return_screen;
            gui->return_screen = FM_GUI_SCREEN_NONE;
            gui->selected_row = 0;
            return (gui->screen == FM_GUI_SCREEN_NONE) ? FM_GUI_ACTION_RESUME_GAME : FM_GUI_ACTION_NONE;
        }
    }
    return FM_GUI_ACTION_NONE;
}

static void render_launcher(const FmGuiMenuState *gui, const FmUserConfig *cfg, FmSnesCanvas *c) {
    draw_snes_rect_fill(c, 0, 0, CANVAS_W, CANVAS_H, FM_COLOR_CANVAS_BACKDROP);
    draw_snes_9slice_box(c, 80, 24, 640, 400, true);
    draw_snes_header_plate(c, 104, 42, 592, "FRONT MISSION (SNES) - NATIVE PC PORT", FM_PAL_GOLD);

    draw_snes_tile_text(c, 104, 72, "ROM:", FM_PAL_MUTED);
    if (gui->rom_verified) {
        char buf[64];
        snprintf(buf, sizeof(buf), "VERIFIED (%.1f MB)", (double)gui->rom_size_bytes / (1024.0 * 1024.0));
        draw_snes_tile_text(c, 144, 72, buf, FM_PAL_EMERALD);
    } else {
        draw_snes_tile_text(c, 144, 72, "NOT FOUND (CHECK front_mission.ini)", FM_PAL_CRIMSON);
    }

    draw_snes_tile_text(c, 104, 98, "BOOT PROFILE", FM_PAL_GOLD);
    for (int i = 0; i < FM_PROFILE_COUNT; ++i) {
        GuiRect r = launcher_profile_rect(i);
        bool active = ((int)cfg->boot_profile == i);
        bool hovered = (gui->hover_id == i || gui->selected_row == i);
        draw_selectable_row_bg(c, r, active, hovered);
        char line[64];
        snprintf(line, sizeof(line), "[%c] %s", active ? '*' : ' ', fm_config_profile_name((FmBootProfileId)i));
        draw_snes_compact_text(c, r.x + 8, r.y + 11, line, active ? FM_PAL_GOLD : FM_PAL_IVORY);
    }

    draw_snes_tile_text(c, 424, 98, "PATCHES & BOOT", FM_PAL_GOLD);
    draw_snes_compact_text(c, 424, 118, "In-memory IPS/BPS patching", FM_PAL_CYAN);
    draw_snes_compact_text(c, 424, 132, "from patches/ before reset.", FM_PAL_MUTED);

    {
        GuiRect r = launcher_autoboot_rect();
        bool hovered = (gui->hover_id == 3 || gui->selected_row == 3);
        draw_selectable_row_bg(c, r, cfg->auto_boot, hovered);
        char line[48];
        snprintf(line, sizeof(line), "[%c] Auto-Boot Next Time", cfg->auto_boot ? 'X' : ' ');
        draw_snes_compact_text(c, r.x + 8, r.y + 11, line, cfg->auto_boot ? FM_PAL_EMERALD : FM_PAL_IVORY);
    }

    draw_button(c, k_btn_launch, "LAUNCH GAME", gui->selected_row == 4, gui->hover_id == 4, FM_PAL_EMERALD);
    draw_button(c, k_btn_settings, "SETTINGS", gui->selected_row == 5, gui->hover_id == 5, FM_PAL_IVORY);
    draw_button(c, k_btn_quit, "QUIT", gui->selected_row == 6, gui->hover_id == 6, FM_PAL_CRIMSON);
}

static void render_settings(const FmGuiMenuState *gui, const FmUserConfig *cfg, FmSnesCanvas *c) {
    dim_framebuffer(c->pixels, CANVAS_W, CANVAS_H);
    draw_snes_9slice_box(c, 112, 32, 576, 384, true);
    draw_snes_header_plate(c, 136, 48, 528, "SYSTEM SETTINGS [ESC TO CLOSE]", FM_PAL_GOLD);

    static const char *k_tabs[FM_TAB_COUNT] = { "VIDEO", "AUDIO", "CONTROLS", "SYSTEM" };
    for (int t = 0; t < FM_TAB_COUNT; ++t) {
        GuiRect tr = settings_tab_rect(t);
        bool active = ((int)gui->active_tab == t);
        bool hovered = (gui->hover_id == 100 + t);
        draw_selectable_row_bg(c, tr, active, hovered);
        draw_snes_compact_text(c, tr.x + 12, tr.y + 8, k_tabs[t], active ? FM_PAL_GOLD : FM_PAL_IVORY);
    }

    int rows = settings_row_count(gui->active_tab);
    for (int r = 0; r < rows; ++r) {
        GuiRect rr = settings_row_rect(r);
        bool sel = (gui->selected_row == r);
        bool hov = (gui->hover_id == r);
        draw_selectable_row_bg(c, rr, sel, hov);

        char label[64] = "";
        char val[48] = "";
        if (gui->active_tab == FM_TAB_VIDEO) {
            if (r == 0) {
                snprintf(label, sizeof(label), "Display Mode");
                const char *modes[3] = { "16:9 WINGS", "16:9 HORIZON", "4:3 CLASSIC" };
                snprintf(val, sizeof(val), "%s", modes[cfg->display_mode % 3]);
            } else if (r == 1) {
                snprintf(label, sizeof(label), "Window Scale");
                snprintf(val, sizeof(val), "%ux (%ux%u)", cfg->window_scale, 800u * cfg->window_scale, 448u * cfg->window_scale);
            } else if (r == 2) {
                snprintf(label, sizeof(label), "Fullscreen");
                snprintf(val, sizeof(val), "%s", cfg->fullscreen ? "ON" : "OFF");
            } else if (r == 3) {
                snprintf(label, sizeof(label), "CRT Scanlines");
                snprintf(val, sizeof(val), "%s", cfg->crt_scanlines ? "ON" : "OFF");
            } else if (r == 4) {
                snprintf(label, sizeof(label), "VSync");
                snprintf(val, sizeof(val), "%s", cfg->vsync ? "ON" : "OFF");
            }
        } else if (gui->active_tab == FM_TAB_AUDIO) {
            if (r == 0) {
                snprintf(label, sizeof(label), "Master Volume");
                snprintf(val, sizeof(val), "%u%%", cfg->master_volume);
            } else if (r == 1) {
                snprintf(label, sizeof(label), "Mute Audio");
                snprintf(val, sizeof(val), "%s", cfg->mute ? "ON" : "OFF");
            }
        } else if (gui->active_tab == FM_TAB_CONTROLS) {
            if (r == 0) {
                snprintf(label, sizeof(label), "Direct-State Mouse");
                snprintf(val, sizeof(val), "%s", cfg->mouse_enabled ? "ON" : "OFF");
            } else if (r == 1) {
                snprintf(label, sizeof(label), "Camera Edge Pan");
                snprintf(val, sizeof(val), "%s", cfg->edge_pan_enabled ? "ON" : "OFF");
            } else if (r == 2) {
                snprintf(label, sizeof(label), "Edge Pan Speed");
                snprintf(val, sizeof(val), "%u px/f", cfg->edge_pan_speed);
            } else if (r == 3) {
                snprintf(label, sizeof(label), "Fast Forward");
                snprintf(val, sizeof(val), "%ux", cfg->fast_forward);
            }
        } else if (gui->active_tab == FM_TAB_SYSTEM) {
            if (r == 0) {
                snprintf(label, sizeof(label), "Auto-Boot");
                snprintf(val, sizeof(val), "%s", cfg->auto_boot ? "ON" : "OFF");
            } else if (r == 1) {
                snprintf(label, sizeof(label), "Return to Launcher");
                snprintf(val, sizeof(val), "[ENTER]");
            } else if (r == 2) {
                snprintf(label, sizeof(label), "Quit Application");
                snprintf(val, sizeof(val), "[ENTER]");
            }
        }
        draw_snes_tile_text(c, rr.x + 12, rr.y + 12, label, FM_PAL_IVORY);
        draw_snes_tile_text(c, rr.x + 320, rr.y + 12, val, FM_PAL_CYAN);
    }

    const char *btn_lbl = (gui->return_screen == FM_GUI_SCREEN_LAUNCHER) ? "BACK TO LAUNCHER" : "RESUME GAME";
    draw_button(c, k_btn_resume, btn_lbl, gui->selected_row == rows, gui->hover_id == 200, FM_PAL_EMERALD);
}

void fm_gui_menu_render(const FmGuiMenuState *gui, const FmUserConfig *cfg, uint32_t *rgba_800x448) {
    if (!gui || !cfg || !rgba_800x448 || gui->screen == FM_GUI_SCREEN_NONE) return;
    FmSnesCanvas canvas;
    fm_snes_canvas_init(&canvas, rgba_800x448, CANVAS_W, CANVAS_H);
    if (gui->screen == FM_GUI_SCREEN_LAUNCHER) {
        render_launcher(gui, cfg, &canvas);
    } else if (gui->screen == FM_GUI_SCREEN_SETTINGS) {
        render_settings(gui, cfg, &canvas);
    }
}
