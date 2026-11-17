#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void safe_strcpy(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static char *trim_whitespace(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
    return s;
}

static bool parse_bool(const char *val, bool fallback) {
    if (!val) return fallback;
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 || strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        return true;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 || strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        return false;
    }
    return fallback;
}

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void fm_config_init_defaults(FmUserConfig *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    safe_strcpy(cfg->rom_path, sizeof(cfg->rom_path), "Front Mission (Japan).sfc");
    safe_strcpy(cfg->patches_dir, sizeof(cfg->patches_dir), "patches");
    cfg->boot_profile = FM_PROFILE_EN_ADDENDUM_103;
    cfg->auto_boot = false;

    cfg->display_mode = 0; /* 16:9 Tactical Wings */
    cfg->fullscreen = false;
    cfg->window_scale = 2; /* 1600x896 */
    cfg->crt_scanlines = false;
    cfg->vsync = true;

    cfg->master_volume = 80;
    cfg->mute = false;

    cfg->mouse_enabled = true;
    cfg->edge_pan_enabled = true;
    cfg->edge_pan_speed = 4;
    cfg->fast_forward = 1;
}

const char *fm_config_profile_name(FmBootProfileId id) {
    return fm_boot_profile_name(id);
}

const char *fm_config_profile_key(FmBootProfileId id) {
    switch (id) {
        case FM_PROFILE_JP_ORIGINAL:     return "jp_original";
        case FM_PROFILE_EN_FH_10B:       return "en_fh_10b";
        case FM_PROFILE_EN_ADDENDUM_103: return "en_addendum_103";
        default:                         return "en_addendum_103";
    }
}

static FmBootProfileId parse_profile_key(const char *s) {
    if (!s) return FM_PROFILE_EN_ADDENDUM_103;
    if (strcmp(s, "jp_original") == 0) return FM_PROFILE_JP_ORIGINAL;
    if (strcmp(s, "en_fh_10b") == 0) return FM_PROFILE_EN_FH_10B;
    if (strcmp(s, "en_addendum_103") == 0) return FM_PROFILE_EN_ADDENDUM_103;
    return FM_PROFILE_EN_ADDENDUM_103;
}

bool fm_config_load(FmUserConfig *cfg, const char *ini_path) {
    if (!cfg || !ini_path) return false;
    FILE *f = fopen(ini_path, "r");
    if (!f) return false;

    char line[1024];
    char section[64] = "";

    while (fgets(line, sizeof(line), f)) {
        char *s = trim_whitespace(line);
        if (s[0] == '\0' || s[0] == '#' || s[0] == ';') continue;

        if (s[0] == '[') {
            char *end = strchr(s, ']');
            if (end) {
                *end = '\0';
                safe_strcpy(section, sizeof(section), trim_whitespace(s + 1));
            }
            continue;
        }

        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim_whitespace(s);
        char *val = trim_whitespace(eq + 1);

        if (strcmp(section, "boot") == 0) {
            if (strcmp(key, "rom_path") == 0) {
                safe_strcpy(cfg->rom_path, sizeof(cfg->rom_path), val);
            } else if (strcmp(key, "patches_dir") == 0) {
                safe_strcpy(cfg->patches_dir, sizeof(cfg->patches_dir), val);
            } else if (strcmp(key, "profile") == 0) {
                cfg->boot_profile = parse_profile_key(val);
            } else if (strcmp(key, "auto_boot") == 0) {
                cfg->auto_boot = parse_bool(val, cfg->auto_boot);
            }
        } else if (strcmp(section, "video") == 0) {
            if (strcmp(key, "display_mode") == 0) {
                cfg->display_mode = (uint8_t)clamp_int(atoi(val), 0, 2);
            } else if (strcmp(key, "fullscreen") == 0) {
                cfg->fullscreen = parse_bool(val, cfg->fullscreen);
            } else if (strcmp(key, "window_scale") == 0) {
                cfg->window_scale = (uint8_t)clamp_int(atoi(val), 1, 4);
            } else if (strcmp(key, "crt_scanlines") == 0) {
                cfg->crt_scanlines = parse_bool(val, cfg->crt_scanlines);
            } else if (strcmp(key, "vsync") == 0) {
                cfg->vsync = parse_bool(val, cfg->vsync);
            }
        } else if (strcmp(section, "audio") == 0) {
            if (strcmp(key, "master_volume") == 0) {
                cfg->master_volume = (uint8_t)clamp_int(atoi(val), 0, 100);
            } else if (strcmp(key, "mute") == 0) {
                cfg->mute = parse_bool(val, cfg->mute);
            }
        } else if (strcmp(section, "controls") == 0) {
            if (strcmp(key, "mouse_enabled") == 0) {
                cfg->mouse_enabled = parse_bool(val, cfg->mouse_enabled);
            } else if (strcmp(key, "edge_pan_enabled") == 0) {
                cfg->edge_pan_enabled = parse_bool(val, cfg->edge_pan_enabled);
            } else if (strcmp(key, "edge_pan_speed") == 0) {
                cfg->edge_pan_speed = (uint8_t)clamp_int(atoi(val), 2, 8);
            } else if (strcmp(key, "fast_forward") == 0) {
                int ff = atoi(val);
                cfg->fast_forward = (ff == 2 || ff == 4) ? (uint8_t)ff : 1u;
            }
        }
    }

    fclose(f);
    return true;
}

bool fm_config_save(const FmUserConfig *cfg, const char *ini_path) {
    if (!cfg || !ini_path) return false;
    FILE *f = fopen(ini_path, "w");
    if (!f) return false;

    fprintf(f, "# Front Mission (SNES) - PC Port Configuration\n\n");
    fprintf(f, "[boot]\n");
    fprintf(f, "rom_path = %s\n", cfg->rom_path);
    fprintf(f, "patches_dir = %s\n", cfg->patches_dir);
    fprintf(f, "profile = %s\n", fm_config_profile_key(cfg->boot_profile));
    fprintf(f, "auto_boot = %s\n\n", cfg->auto_boot ? "true" : "false");

    fprintf(f, "[video]\n");
    fprintf(f, "display_mode = %u\n", cfg->display_mode);
    fprintf(f, "fullscreen = %s\n", cfg->fullscreen ? "true" : "false");
    fprintf(f, "window_scale = %u\n", cfg->window_scale);
    fprintf(f, "crt_scanlines = %s\n", cfg->crt_scanlines ? "true" : "false");
    fprintf(f, "vsync = %s\n\n", cfg->vsync ? "true" : "false");

    fprintf(f, "[audio]\n");
    fprintf(f, "master_volume = %u\n", cfg->master_volume);
    fprintf(f, "mute = %s\n\n", cfg->mute ? "true" : "false");

    fprintf(f, "[controls]\n");
    fprintf(f, "mouse_enabled = %s\n", cfg->mouse_enabled ? "true" : "false");
    fprintf(f, "edge_pan_enabled = %s\n", cfg->edge_pan_enabled ? "true" : "false");
    fprintf(f, "edge_pan_speed = %u\n", cfg->edge_pan_speed);
    fprintf(f, "fast_forward = %u\n", cfg->fast_forward);

    fclose(f);
    return true;
}

void fm_config_to_boot_config(const FmUserConfig *cfg, FmBootConfig *out_boot) {
    if (!cfg || !out_boot) return;
    memset(out_boot, 0, sizeof(*out_boot));
    out_boot->rom_path = cfg->rom_path;
    out_boot->patches_dir = cfg->patches_dir;
    out_boot->profile = cfg->boot_profile;
    out_boot->recalculate_checksum = true;
}
