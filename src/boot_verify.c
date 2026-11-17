#include "boot_loader.h"
#include "config.h"
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

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <rom_path> <patches_dir> [profile:0|1|2] [frames] [out.ppm]\n", argv[0]);
        return 1;
    }

    const char *rom_path = argv[1];
    const char *patches_dir = argv[2];
    int profile_id = (argc >= 4) ? atoi(argv[3]) : FM_PROFILE_EN_ADDENDUM_103;
    int frames = (argc >= 5) ? atoi(argv[4]) : 420;
    const char *out_ppm = (argc >= 6) ? argv[5] : NULL;

    FmBootConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.rom_path = rom_path;
    cfg.patches_dir = patches_dir;
    cfg.profile = (FmBootProfileId)profile_id;
    cfg.recalculate_checksum = true;

    FmBootResult res;
    if (!fm_boot_load_rom(&cfg, &res)) {
        fprintf(stderr, "[GATE 0] Boot loader failed: %s\n", res.error_msg);
        return 1;
    }

    printf("[GATE 0] Profile: %s | Size: %zu bytes | CRC32: %08X | Modified ranges: %zu\n",
           fm_boot_profile_name(res.active_profile),
           res.rom_size,
           res.crc32,
           res.modified_range_count);

    Snes *snes = snes_init();
    if (!snes) {
        fprintf(stderr, "[GATE 0] snes_init() failed\n");
        fm_boot_free_result(&res);
        return 1;
    }

    if (!snes_loadRom(snes, res.rom_data, (int)res.rom_size)) {
        fprintf(stderr, "[GATE 0] snes_loadRom() failed\n");
        snes_free(snes);
        fm_boot_free_result(&res);
        return 1;
    }

    uint8_t *pixels = (uint8_t *)calloc(1, 512 * 480 * 4);
    int16_t *samples = (int16_t *)calloc(1, 735 * 2 * sizeof(int16_t));
    if (!pixels || !samples) {
        free(pixels);
        free(samples);
        snes_free(snes);
        fm_boot_free_result(&res);
        return 1;
    }

    snes_setPixelFormat(snes, pixelFormatXRGB);

    uint64_t peak_non_black = 0;
    uint64_t non_zero_audio_samples = 0;

    for (int f = 0; f < frames; ++f) {
        snes_runFrame(snes);
        snes_setPixels(snes, pixels);
        snes_setSamples(snes, samples, 735);

        uint64_t frame_non_black = 0;
        for (int i = 0; i < 512 * 480; ++i) {
            uint8_t b = pixels[i * 4 + 0];
            uint8_t g = pixels[i * 4 + 1];
            uint8_t r = pixels[i * 4 + 2];
            if (r > 4 || g > 4 || b > 4) {
                frame_non_black++;
            }
        }
        if (frame_non_black > peak_non_black) {
            peak_non_black = frame_non_black;
        }
        for (int i = 0; i < 735 * 2; ++i) {
            if (samples[i] != 0) {
                non_zero_audio_samples++;
            }
        }
    }

    if (out_ppm) {
        if (write_ppm(out_ppm, pixels + (16 * 512 * 4), 512, 448)) {
            printf("[GATE 0] Wrote snapshot: %s\n", out_ppm);
        }
    }

    printf("[GATE 0] Frames: %d | Peak non-black pixels: %llu | Non-zero audio samples: %llu\n",
           frames,
           (unsigned long long)peak_non_black,
           (unsigned long long)non_zero_audio_samples);

    bool passed = (peak_non_black > 500) && (non_zero_audio_samples > 100);
    if (!passed) {
        fprintf(stderr, "[GATE 0] FAILED: insufficient video or audio output!\n");
    } else {
        printf("[GATE 0] PASSED: Video + SPC700 audio active.\n");
    }

    free(pixels);
    free(samples);
    snes_free(snes);
    fm_boot_free_result(&res);
    return passed ? 0 : 1;
}
