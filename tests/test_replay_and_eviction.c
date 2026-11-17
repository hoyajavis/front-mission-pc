#include "boot_loader.h"
#include "replay.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_replay_roundtrip(void) {
    FmReplayTrace *t = fm_replay_create(0, 0xFE27E061u, 16u);
    assert(t != NULL);
    assert(fm_replay_push_frame(t, 0x0000u));
    assert(fm_replay_push_frame(t, 0x1000u)); /* START */
    assert(fm_replay_push_frame(t, 0x0080u)); /* A */
    assert(t->header.frame_count == 3);

    const char *tmp_path = "/tmp/fm_test_trace.fmrec";
    assert(fm_replay_save(t, tmp_path));
    fm_replay_free(t);

    FmReplayTrace *loaded = fm_replay_load(tmp_path);
    assert(loaded != NULL);
    assert(loaded->header.frame_count == 3);
    assert(loaded->header.profile_id == 0);
    assert(loaded->header.rom_crc32 == 0xFE27E061u);
    assert(fm_replay_get_frame(loaded, 0) == 0x0000u);
    assert(fm_replay_get_frame(loaded, 1) == 0x1000u);
    assert(fm_replay_get_frame(loaded, 2) == 0x0080u);
    assert(fm_replay_get_frame(loaded, 99) == 0x0000u);
    fm_replay_free(loaded);
    remove(tmp_path);
}

static void test_hirom_mapping_and_eviction(void) {
    uint32_t off = 0;

    /* 3 MB HiROM: $C1:00DE -> 0x0100DE */
    assert(fm_hirom_addr_to_offset(0xC100DEu, FM_ROM_JP_SIZE, &off));
    assert(off == 0x0100DEu);

    /* 3 MB HiROM mirror: $EA:0997 -> 0x2A0997 */
    assert(fm_hirom_addr_to_offset(0xEA0997u, FM_ROM_JP_SIZE, &off));
    assert(off == 0x2A0997u);

    /* 3 MB HiROM mirror: $F0:0000 -> 0x200000 */
    assert(fm_hirom_addr_to_offset(0xF00000u, FM_ROM_JP_SIZE, &off));
    assert(off == 0x200000u);

    /* WRAM $7E:1DFD is not ROM */
    assert(!fm_hirom_addr_to_offset(0x7E1DFDu, FM_ROM_JP_SIZE, &off));

    /* Low-bank sub-8000 is not ROM */
    assert(!fm_hirom_addr_to_offset(0x002000u, FM_ROM_JP_SIZE, &off));

    /* Patch overlap checks */
    FmPatchRange ranges[2] = {
        { 0x010100u, 16u },
        { 0x200000u, 32u },
    };
    assert(fm_patch_range_overlaps(ranges, 2, 0x0100DEu, 256u));
    assert(!fm_patch_range_overlaps(ranges, 2, 0x020000u, 256u));
    assert(fm_patch_range_overlaps(ranges, 2, 0x200010u, 4u));
}

int main(void) {
    test_replay_roundtrip();
    test_hirom_mapping_and_eviction();
    printf("All Phase 1 replay & patch eviction tests PASSED.\n");
    return 0;
}
