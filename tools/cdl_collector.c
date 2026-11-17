#include "boot_loader.h"
#include "opcodes_65816.h"
#include "replay.h"
#include "snes.h"
#include "cpu.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern bool (*g_cpuRecompHook)(Cpu *);

/* Mesen-S standard CDL bitmask constants */
#define CDL_CODE         0x01u
#define CDL_DATA         0x02u
#define CDL_VIDEO        0x04u
#define CDL_AUDIO        0x08u
#define CDL_JUMP_TARGET  0x10u
#define CDL_SUB_ENTRY    0x20u
#define CDL_FLAG_M8      0x40u
#define CDL_FLAG_X8      0x80u

typedef struct RoutineEntry {
    uint32_t addr24;
    uint8_t  p_flags;
    bool     is_long;
    bool     mixed_width;
    uint64_t hit_count;
} RoutineEntry;

static uint8_t *g_cdl = NULL;
static uint32_t g_rom_size = 0;

static uint32_t s_expected_next_pc = 0xFFFFFFFFu;
static uint8_t  s_last_opcode = 0;
static uint64_t s_opcode_count = 0;

#define MAX_ROUTINES 16384
static RoutineEntry s_routines[MAX_ROUTINES];
static size_t s_routine_count = 0;

static inline int snes_to_hirom(uint32_t pb_pc, uint32_t rom_size) {
    uint8_t bank = (uint8_t)((pb_pc >> 16) & 0xFFu);
    uint16_t adr = (uint16_t)(pb_pc & 0xFFFFu);
    if (bank == 0x7Eu || bank == 0x7Fu) return -1;
    /* HiROM: banks $40-$7D, $C0-$FF map full 64KB; $00-$3F, $80-$BF map $8000-$FFFF */
    if ((bank & 0x40u) != 0 || adr >= 0x8000u) {
        uint32_t off = (((uint32_t)(bank & 0x3Fu)) << 16) | adr;
        if (off < rom_size) {
            return (int)off;
        }
    }
    return -1;
}

static inline uint32_t normalize_hirom_addr(uint32_t pb_pc) {
    uint8_t bank = (uint8_t)((pb_pc >> 16) & 0xFFu);
    uint16_t adr = (uint16_t)(pb_pc & 0xFFFFu);
    if (bank == 0x7Eu || bank == 0x7Fu) return pb_pc;
    if ((bank & 0x40u) != 0 || adr >= 0x8000u) {
        return (uint32_t)(((0xC0u | (bank & 0x3Fu)) << 16) | adr);
    }
    return pb_pc;
}

static void record_routine_entry(uint32_t addr24, uint8_t p_mx, bool is_long) {
    for (size_t i = 0; i < s_routine_count; ++i) {
        if (s_routines[i].addr24 == addr24) {
            s_routines[i].hit_count++;
            if ((s_routines[i].p_flags & 0x30u) != (p_mx & 0x30u)) {
                s_routines[i].mixed_width = true;
            }
            if (is_long) {
                s_routines[i].is_long = true;
            }
            return;
        }
    }
    if (s_routine_count < MAX_ROUTINES) {
        s_routines[s_routine_count].addr24 = addr24;
        s_routines[s_routine_count].p_flags = p_mx;
        s_routines[s_routine_count].is_long = is_long;
        s_routines[s_routine_count].mixed_width = false;
        s_routines[s_routine_count].hit_count = 1;
        s_routine_count++;
    }
}

static bool on_cpu_opcode_hook(Cpu *cpu) {
    uint32_t pb_pc = ((uint32_t)cpu->k << 16) | cpu->pc;
    uint8_t opcode = cpu->read(cpu->mem, pb_pc);
    bool mf = cpu->mf;
    bool xf = cpu->xf;

    s_opcode_count++;
    int rom_off = snes_to_hirom(pb_pc, g_rom_size);
    if (rom_off >= 0 && (uint32_t)rom_off < g_rom_size) {
        uint8_t flags = CDL_CODE;
        if (mf) flags |= CDL_FLAG_M8;
        if (xf) flags |= CDL_FLAG_X8;

        if (s_last_opcode == 0x20 || s_last_opcode == 0x22 || s_last_opcode == 0xFC) {
            flags |= CDL_SUB_ENTRY;
            uint8_t p_mx = (uint8_t)((mf ? 0x20u : 0u) | (xf ? 0x10u : 0u));
            bool is_long = (s_last_opcode == 0x22);
            record_routine_entry(normalize_hirom_addr(pb_pc), p_mx, is_long);
        }

        if (pb_pc != s_expected_next_pc && s_expected_next_pc != 0xFFFFFFFFu) {
            flags |= CDL_JUMP_TARGET;
        }

        g_cdl[rom_off] |= flags;
    }

    int len = get_65816_insn_len(opcode, mf, xf);
    s_last_opcode = opcode;

    switch (opcode) {
        case 0x4C:
        case 0x5C:
        case 0x6C:
        case 0x7C:
        case 0xDC:
        case 0x80:
        case 0x82:
        case 0x60:
        case 0x6B:
        case 0x40:
        case 0xDB:
            s_expected_next_pc = 0xFFFFFFFFu;
            break;
        default:
            s_expected_next_pc = (pb_pc & 0xFF0000u) | ((pb_pc + (uint32_t)len) & 0xFFFFu);
            break;
    }

    return false; /* Let LakeSnes execute the instruction normally */
}

static int cmp_routine_addr(const void *a, const void *b) {
    const RoutineEntry *ra = (const RoutineEntry *)a;
    const RoutineEntry *rb = (const RoutineEntry *)b;
    if (ra->addr24 < rb->addr24) return -1;
    if (ra->addr24 > rb->addr24) return 1;
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <rom_path> <patches_dir> <out.cdl> <out_manifest.txt> <trace1.fmrec> [trace2.fmrec ...]\n", argv[0]);
        return 1;
    }

    const char *rom_path = argv[1];
    const char *patches_dir = argv[2];
    const char *cdl_out_path = argv[3];
    const char *manifest_out_path = argv[4];

    FmBootConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.rom_path = rom_path;
    cfg.patches_dir = patches_dir;
    cfg.profile = FM_PROFILE_JP_ORIGINAL;
    cfg.recalculate_checksum = true;

    FmBootResult res;
    if (!fm_boot_load_rom(&cfg, &res)) {
        fprintf(stderr, "Boot loader failed: %s\n", res.error_msg);
        return 1;
    }

    g_rom_size = (uint32_t)res.rom_size;
    g_cdl = (uint8_t *)calloc(g_rom_size, 1);
    if (!g_cdl) {
        fm_boot_free_result(&res);
        return 1;
    }

    Snes *snes = snes_init();
    if (!snes || !snes_loadRom(snes, res.rom_data, (int)res.rom_size)) {
        fprintf(stderr, "Failed to initialize LakeSnes with ROM %s\n", rom_path);
        free(g_cdl);
        if (snes) snes_free(snes);
        fm_boot_free_result(&res);
        return 1;
    }

    g_cpuRecompHook = on_cpu_opcode_hook;

    int num_traces = argc - 5;
    for (int i = 0; i < num_traces; ++i) {
        const char *trace_path = argv[5 + i];
        FmReplayTrace *tr = fm_replay_load(trace_path);
        if (!tr) {
            fprintf(stderr, "Warning: failed to load %s, skipping\n", trace_path);
            continue;
        }
        printf("[%d/%d] Replaying %s (%u frames)...\n", i + 1, num_traces, trace_path, tr->header.frame_count);
        snes_reset(snes, true);
        s_expected_next_pc = 0xFFFFFFFFu;
        s_last_opcode = 0;

        for (uint32_t f = 0; f < tr->header.frame_count; ++f) {
            snes->input1->currentState = fm_replay_get_frame(tr, f);
            snes_runFrame(snes);
        }
        fm_replay_free(tr);
    }

    g_cpuRecompHook = NULL;

    /* Write CDL file */
    FILE *f_cdl = fopen(cdl_out_path, "wb");
    if (!f_cdl) {
        fprintf(stderr, "Failed to write %s\n", cdl_out_path);
        snes_free(snes);
        free(g_cdl);
        fm_boot_free_result(&res);
        return 1;
    }
    fwrite(g_cdl, 1, g_rom_size, f_cdl);
    fclose(f_cdl);

    /* Sort and write routine manifest */
    qsort(s_routines, s_routine_count, sizeof(RoutineEntry), cmp_routine_addr);
    FILE *f_man = fopen(manifest_out_path, "w");
    if (!f_man) {
        fprintf(stderr, "Failed to write %s\n", manifest_out_path);
        snes_free(snes);
        free(g_cdl);
        fm_boot_free_result(&res);
        return 1;
    }
    fprintf(f_man, "# Front Mission (JP) CDL Routine Manifest (BBAAAA P JSR|JSL)\n");
    size_t clean_count = 0;
    for (size_t i = 0; i < s_routine_count; ++i) {
        const RoutineEntry *r = &s_routines[i];
        if (r->mixed_width) {
            fprintf(f_man, "# %06X %02X %s # skipped: mixed M/X entry widths (hits=%llu)\n",
                    r->addr24, r->p_flags, r->is_long ? "JSL" : "JSR",
                    (unsigned long long)r->hit_count);
        } else {
            fprintf(f_man, "%06X %02X %s # hits=%llu\n",
                    r->addr24, r->p_flags, r->is_long ? "JSL" : "JSR",
                    (unsigned long long)r->hit_count);
            clean_count++;
        }
    }
    fclose(f_man);

    uint32_t total_code = 0;
    uint32_t bank_code[64] = {0};
    for (uint32_t i = 0; i < g_rom_size; ++i) {
        if (g_cdl[i] & CDL_CODE) {
            total_code++;
            uint32_t b = i >> 16;
            if (b < 64) bank_code[b]++;
        }
    }

    printf("\n====================================================================\n");
    printf("Front Mission HiROM CDL Summary\n");
    printf("====================================================================\n");
    printf("Total Instructions Decoded: %llu\n", (unsigned long long)s_opcode_count);
    printf("Distinct Code Bytes:        %u (%.2f%% of 3MB ROM)\n",
           total_code, (double)total_code / g_rom_size * 100.0);
    printf("Unique Subroutine Entries:  %zu (%zu single-width)\n", s_routine_count, clean_count);
    for (uint32_t b = 0; b < 48; ++b) {
        if (bank_code[b] > 0) {
            printf("  Bank $%02X ($%02X): %5u B (%5.1f%%)\n",
                   0xC0u + b, b, bank_code[b], (double)bank_code[b] / 65536.0 * 100.0);
        }
    }
    printf("====================================================================\n");

    snes_free(snes);
    free(g_cdl);
    fm_boot_free_result(&res);
    return 0;
}
