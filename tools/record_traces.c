#include "boot_loader.h"
#include "replay.h"
#include "snes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool write_ppm(const char *path, const uint8_t *xrgb_pixels, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; ++i) {
        uint8_t b = xrgb_pixels[i * 4 + 0];
        uint8_t g = xrgb_pixels[i * 4 + 1];
        uint8_t r = xrgb_pixels[i * 4 + 2];
        uint8_t rgb[3] = { r, g, b };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    return true;
}

static void step_and_record(Snes *snes, FmReplayTrace *trace, uint16_t btn) {
    snes->input1->currentState = btn;
    snes_runFrame(snes);
    if (trace) {
        fm_replay_push_frame(trace, btn);
    }
}

static void run_frames(Snes *snes, FmReplayTrace *trace, uint16_t btn, int count) {
    for (int i = 0; i < count; ++i) {
        step_and_record(snes, trace, btn);
    }
}

static void tap_button(Snes *snes, FmReplayTrace *trace, uint16_t btn, int hold, int wait) {
    run_frames(snes, trace, btn, hold);
    run_frames(snes, trace, FM_BTN_NONE, wait);
}

static void save_snapshot(Snes *snes, uint8_t *pixels, const char *path) {
    snes_setPixels(snes, pixels);
    write_ppm(path, pixels + (16 * 512 * 4), 512, 448);
    printf("[SNAPSHOT] %s\n", path);
}

static Snes *init_snes_profile(const char *rom_path, const char *patches_dir, FmBootProfileId profile, FmBootResult *out_res) {
    FmBootConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.rom_path = rom_path;
    cfg.patches_dir = patches_dir;
    cfg.profile = profile;
    cfg.recalculate_checksum = true;

    if (!fm_boot_load_rom(&cfg, out_res)) {
        fprintf(stderr, "Boot loader failed: %s\n", out_res->error_msg);
        return NULL;
    }

    Snes *snes = snes_init();
    if (!snes) {
        fm_boot_free_result(out_res);
        return NULL;
    }
    if (!snes_loadRom(snes, out_res->rom_data, (int)out_res->rom_size)) {
        snes_free(snes);
        fm_boot_free_result(out_res);
        return NULL;
    }
    snes_setPixelFormat(snes, pixelFormatXRGB);
    return snes;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <rom_path> <patches_dir>\n", argv[0]);
        return 1;
    }
    const char *rom_path = argv[1];
    const char *patches_dir = argv[2];
    uint8_t *pixels = (uint8_t *)calloc(1, 512 * 480 * 4);
    if (!pixels) return 1;

    /* 1. JP Trace 01: Boot -> Title Screen */
    {
        FmBootResult res;
        Snes *snes = init_snes_profile(rom_path, patches_dir, FM_PROFILE_JP_ORIGINAL, &res);
        if (!snes) { free(pixels); return 1; }
        FmReplayTrace *tr = fm_replay_create(FM_PROFILE_JP_ORIGINAL, res.crc32, 1024);
        run_frames(snes, tr, FM_BTN_NONE, 200);
        tap_button(snes, tr, FM_BTN_START, 4, 180);
        tap_button(snes, tr, FM_BTN_START, 4, 180);
        save_snapshot(snes, pixels, "traces/snapshots/jp_01_title.ppm");
        fm_replay_save(tr, "traces/jp_trace_01_boot_title.fmrec");
        printf("[TRACE] Saved traces/jp_trace_01_boot_title.fmrec (%u frames)\n", tr->header.frame_count);
        fm_replay_free(tr);
        snes_free(snes);
        fm_boot_free_result(&res);
    }

    /* 2. JP Trace 02: Boot -> Title -> New Game -> Name Confirm -> Opening Cutscene -> Mission 1 */
    {
        FmBootResult res;
        Snes *snes = init_snes_profile(rom_path, patches_dir, FM_PROFILE_JP_ORIGINAL, &res);
        if (!snes) { free(pixels); return 1; }
        FmReplayTrace *tr = fm_replay_create(FM_PROFILE_JP_ORIGINAL, res.crc32, 16384);

        /* Skip intro to title screen, select New Game */
        run_frames(snes, tr, FM_BTN_NONE, 200);
        tap_button(snes, tr, FM_BTN_START, 4, 180);
        tap_button(snes, tr, FM_BTN_START, 4, 120);
        tap_button(snes, tr, FM_BTN_A, 4, 60); /* Title: Start / New Game */
        tap_button(snes, tr, FM_BTN_A, 4, 300); /* Confirm New Game & wait past Pilot Name Confirmation splash */
        save_snapshot(snes, pixels, "traces/snapshots/jp_02a_new_game.ppm");

        /* Name Entry screen: confirm default name */
        tap_button(snes, tr, FM_BTN_START, 6, 30);
        tap_button(snes, tr, FM_BTN_X, 6, 30);
        tap_button(snes, tr, FM_BTN_Y, 6, 30);
        tap_button(snes, tr, FM_BTN_SELECT, 6, 30);
        tap_button(snes, tr, FM_BTN_A, 6, 45);
        tap_button(snes, tr, FM_BTN_A, 6, 90);

        /* Advance through opening Larcus District prologue dialogue with steady A cadence */
        for (int i = 0; i < 300; ++i) {
            tap_button(snes, tr, FM_BTN_A, 3, 12);
        }
        save_snapshot(snes, pixels, "traces/snapshots/jp_02b_prologue_mid.ppm");

        for (int i = 0; i < 350; ++i) {
            tap_button(snes, tr, FM_BTN_A, 3, 12);
        }
        save_snapshot(snes, pixels, "traces/snapshots/jp_02c_mission1.ppm");

        fm_replay_save(tr, "traces/jp_trace_02_prologue_mission1.fmrec");
        printf("[TRACE] Saved traces/jp_trace_02_prologue_mission1.fmrec (%u frames)\n", tr->header.frame_count);

        /* 3. JP Trace 03: Continue into Mission 1 Tactical Grid, Menus, Movement & Combat */
        tap_button(snes, tr, FM_BTN_R, 4, 20);
        tap_button(snes, tr, FM_BTN_L, 4, 20);
        tap_button(snes, tr, FM_BTN_B, 4, 30);
        save_snapshot(snes, pixels, "traces/snapshots/jp_03a_unit_menu.ppm");
        tap_button(snes, tr, FM_BTN_B, 4, 20); /* close menu */

        /* Move cursor & select unit */
        tap_button(snes, tr, FM_BTN_UP, 4, 15);
        tap_button(snes, tr, FM_BTN_RIGHT, 4, 15);
        tap_button(snes, tr, FM_BTN_A, 4, 25);
        tap_button(snes, tr, FM_BTN_DOWN, 4, 15);
        tap_button(snes, tr, FM_BTN_LEFT, 4, 15);
        tap_button(snes, tr, FM_BTN_A, 4, 30);
        tap_button(snes, tr, FM_BTN_B, 4, 20);

        /* Open System Menu with Start and close */
        tap_button(snes, tr, FM_BTN_START, 4, 30);
        save_snapshot(snes, pixels, "traces/snapshots/jp_03b_sys_menu.ppm");
        tap_button(snes, tr, FM_BTN_B, 4, 20);

        /* Run 120 idle frames */
        run_frames(snes, tr, FM_BTN_NONE, 120);
        fm_replay_save(tr, "traces/jp_trace_03_mission1_tactical.fmrec");
        printf("[TRACE] Saved traces/jp_trace_03_mission1_tactical.fmrec (%u frames)\n", tr->header.frame_count);

        fm_replay_free(tr);
        snes_free(snes);
        fm_boot_free_result(&res);
    }

    /* 4. EN Trace 01: English + Addendum v1.03 Boot -> Title -> VWF Prologue */
    {
        FmBootResult res;
        Snes *snes = init_snes_profile(rom_path, patches_dir, FM_PROFILE_EN_ADDENDUM_103, &res);
        if (!snes) { free(pixels); return 1; }
        FmReplayTrace *tr = fm_replay_create(FM_PROFILE_EN_ADDENDUM_103, res.crc32, 4096);

        run_frames(snes, tr, FM_BTN_NONE, 200);
        tap_button(snes, tr, FM_BTN_START, 4, 180);
        tap_button(snes, tr, FM_BTN_START, 4, 120);
        save_snapshot(snes, pixels, "traces/snapshots/en_01a_title.ppm");
        tap_button(snes, tr, FM_BTN_A, 4, 60);
        tap_button(snes, tr, FM_BTN_A, 4, 300);
        tap_button(snes, tr, FM_BTN_START, 6, 30);
        tap_button(snes, tr, FM_BTN_X, 6, 30);
        tap_button(snes, tr, FM_BTN_Y, 6, 30);
        tap_button(snes, tr, FM_BTN_SELECT, 6, 30);
        tap_button(snes, tr, FM_BTN_A, 6, 45);
        tap_button(snes, tr, FM_BTN_A, 6, 90);
        for (int i = 0; i < 80; ++i) {
            tap_button(snes, tr, FM_BTN_A, 3, 12);
        }
        save_snapshot(snes, pixels, "traces/snapshots/en_01b_vwf_prologue.ppm");

        fm_replay_save(tr, "traces/en_trace_01_boot_vwf.fmrec");
        printf("[TRACE] Saved traces/en_trace_01_boot_vwf.fmrec (%u frames)\n", tr->header.frame_count);

        fm_replay_free(tr);
        snes_free(snes);
        fm_boot_free_result(&res);
    }

    free(pixels);
    return 0;
}
