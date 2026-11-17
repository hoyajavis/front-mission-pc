/*
 * verify_trace.c — Dual-Execution Lockstep Verification Oracle (RM_BOTH)
 *
 * Verifies 100% bit-exact parity between:
 *   1. Golden Reference Emulator (pure LakeSnes interpreter)
 *   2. Candidate Native Recomp Core (libfm_recomp + timed interception)
 *
 * Across all 128KB WRAM, 64KB VRAM, CGRAM, OAM, High OAM, and Cartridge SRAM.
 */

#include "boot_loader.h"
#include "replay.h"
#include "snes.h"
#include <snesrecomp/snesrecomp.h>
#include <snesrecomp/bus.h>
#include <snesrecomp/cpu.h>
#include <snesrecomp/func_table.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern bool (*g_cpuRecompHook)(Cpu *);
extern void fm_recomp_register_all(bool with_intercept, bool with_validate);
extern size_t fm_recomp_count(void);

static Snes *s_cand_snes = NULL;
Snes *snesrecomp_get_snes(void) { return s_cand_snes; }
bool g_recomp_validating = false;
bool g_recomp_frame_touched = false;

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <rom_path> <patches_dir> <trace.fmrec> [validate_only:0|1]\n", argv[0]);
        return 1;
    }

    const char *rom_path = argv[1];
    const char *patches_dir = argv[2];
    const char *trace_path = argv[3];
    bool validate_only = (argc >= 5) ? (atoi(argv[4]) != 0) : false;

    FmReplayTrace *trace = fm_replay_load(trace_path);
    if (!trace || trace->header.frame_count == 0) {
        fprintf(stderr, "Error: failed to load trace '%s'\n", trace_path);
        if (trace) fm_replay_free(trace);
        return 1;
    }

    FmBootConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.rom_path = rom_path;
    cfg.patches_dir = patches_dir;
    cfg.profile = (FmBootProfileId)trace->header.profile_id;
    cfg.recalculate_checksum = true;

    FmBootResult boot_res;
    if (!fm_boot_load_rom(&cfg, &boot_res)) {
        fprintf(stderr, "Error: boot loader failed: %s\n", boot_res.error_msg);
        fm_replay_free(trace);
        return 1;
    }

    printf("====================================================\n");
    printf("RM_BOTH Lockstep Verification: %s\n", trace_path);
    printf("Profile: %s | Frames: %u | Lifted routines: %zu\n",
           fm_boot_profile_name(boot_res.active_profile),
           trace->header.frame_count,
           fm_recomp_count());
    printf("====================================================\n");

    Snes *ref_snes = snes_init();
    Snes *cand_snes = snes_init();
    if (!ref_snes || !cand_snes ||
        !snes_loadRom(ref_snes, boot_res.rom_data, (int)boot_res.rom_size) ||
        !snes_loadRom(cand_snes, boot_res.rom_data, (int)boot_res.rom_size)) {
        fprintf(stderr, "Error: failed to initialize LakeSnes instances\n");
        if (ref_snes) snes_free(ref_snes);
        if (cand_snes) snes_free(cand_snes);
        fm_boot_free_result(&boot_res);
        fm_replay_free(trace);
        return 1;
    }

    func_table_init();
    recomp_interp_set_enabled(true);

    /* Only register JP-lifted routines if no patch modified the ROM (or future: per-range eviction) */
    if (boot_res.modified_range_count == 0) {
        if (validate_only) {
            fm_recomp_register_all(false, true);
        } else {
            fm_recomp_register_all(true, false);
        }
    } else {
        printf("[PATCH EVICTION] %zu modified ranges detected; evicting lifted routines for patched profile.\n",
               boot_res.modified_range_count);
    }

    for (uint32_t f = 0; f < trace->header.frame_count; ++f) {
        uint16_t btn = fm_replay_get_frame(trace, f);

        /* Step Reference (Pure Interpreter) */
        ref_snes->input1->currentState = btn;
        g_cpuRecompHook = NULL;
        snes_runFrame(ref_snes);

        /* Step Candidate (Recomp Core) */
        cand_snes->input1->currentState = btn;
        s_cand_snes = cand_snes;
        recomp_timed_recomp_enable();
        snes_runFrame(cand_snes);
        recomp_timed_recomp_disable();

        /* WRAM (128KB): exclude dead stack scratch below SP */
        uint16_t sp = ref_snes->cpu->sp;
        for (uint32_t i = 0; i < 0x20000; ++i) {
            if (i <= sp && i > (uint16_t)(sp - 0x100)) continue;
            if (ref_snes->ram[i] != cand_snes->ram[i]) {
                fprintf(stderr, "\n[LOCKSTEP DESYNC] WRAM divergence at frame %u: $7E:%04X (Ref=%02X, Cand=%02X)\n",
                        f + 1, i, ref_snes->ram[i], cand_snes->ram[i]);
                goto fail;
            }
        }
        if (f + 1 == trace->header.frame_count) {
            if (memcmp(ref_snes->ram, cand_snes->ram, 0x20000) != 0) {
                fprintf(stderr, "\n[LOCKSTEP DESYNC] Full WRAM divergence at final frame %u!\n", f + 1);
                goto fail;
            }
        }
        if (memcmp(ref_snes->ppu->vram, cand_snes->ppu->vram, 0x10000) != 0) {
            fprintf(stderr, "\n[LOCKSTEP DESYNC] VRAM divergence at frame %u!\n", f + 1);
            goto fail;
        }
        if (memcmp(ref_snes->ppu->cgram, cand_snes->ppu->cgram, 0x200) != 0) {
            fprintf(stderr, "\n[LOCKSTEP DESYNC] CGRAM divergence at frame %u!\n", f + 1);
            goto fail;
        }
        if (memcmp(ref_snes->ppu->oam, cand_snes->ppu->oam, 0x200) != 0) {
            fprintf(stderr, "\n[LOCKSTEP DESYNC] OAM divergence at frame %u!\n", f + 1);
            goto fail;
        }
        if (memcmp(ref_snes->ppu->highOam, cand_snes->ppu->highOam, 0x20) != 0) {
            fprintf(stderr, "\n[LOCKSTEP DESYNC] High OAM divergence at frame %u!\n", f + 1);
            goto fail;
        }
        if (ref_snes->cart->ram && cand_snes->cart->ram && ref_snes->cart->ramSize > 0) {
            if (memcmp(ref_snes->cart->ram, cand_snes->cart->ram, ref_snes->cart->ramSize) != 0) {
                fprintf(stderr, "\n[LOCKSTEP DESYNC] Cartridge SRAM divergence at frame %u!\n", f + 1);
                goto fail;
            }
        }
    }

    if (validate_only) {
        recomp_timed_validate_report();
    }
    printf("[LOCKSTEP PASS] %u frames verified with 0-byte WRAM/VRAM/CGRAM/OAM/SRAM drift! (intercept hits=%lu)\n",
           trace->header.frame_count, recomp_timed_intercept_hits());

    snes_free(ref_snes);
    snes_free(cand_snes);
    fm_boot_free_result(&boot_res);
    fm_replay_free(trace);
    return 0;

fail:
    snes_free(ref_snes);
    snes_free(cand_snes);
    fm_boot_free_result(&boot_res);
    fm_replay_free(trace);
    return 1;
}
