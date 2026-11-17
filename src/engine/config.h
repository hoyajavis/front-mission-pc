#ifndef FM_ENGINE_CONFIG_H
#define FM_ENGINE_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include "../../include/boot_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FM_CONFIG_PATH_MAX 512

typedef struct FmUserConfig {
    /* General / Boot */
    char rom_path[FM_CONFIG_PATH_MAX];
    char patches_dir[FM_CONFIG_PATH_MAX];
    FmBootProfileId boot_profile;
    bool auto_boot;

    /* Video */
    uint8_t display_mode;   /* 0=16:9 Wings, 1=16:9 Horizon, 2=4:3 Classic */
    bool fullscreen;
    uint8_t window_scale;   /* 1..4 (default 2 -> 1600x896) */
    bool crt_scanlines;
    bool vsync;

    /* Audio */
    uint8_t master_volume;  /* 0..100 */
    bool mute;

    /* Controls */
    bool mouse_enabled;
    bool edge_pan_enabled;
    uint8_t edge_pan_speed; /* 2..8 px/frame */
    uint8_t fast_forward;   /* 1, 2, or 4 */
} FmUserConfig;

void fm_config_init_defaults(FmUserConfig *cfg);
bool fm_config_load(FmUserConfig *cfg, const char *ini_path);
bool fm_config_save(const FmUserConfig *cfg, const char *ini_path);
void fm_config_to_boot_config(const FmUserConfig *cfg, FmBootConfig *out_boot);
const char *fm_config_profile_name(FmBootProfileId id);
const char *fm_config_profile_key(FmBootProfileId id);

#ifdef __cplusplus
}
#endif

#endif /* FM_ENGINE_CONFIG_H */
