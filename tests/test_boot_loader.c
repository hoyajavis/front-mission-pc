#include "boot_loader.h"
#include "config.h"
#include "gui_menu.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_crc32_and_sha1(void) {
    const uint8_t msg[] = "123456789";
    uint32_t crc = fm_crc32(msg, 9);
    assert(crc == 0xCBF43926u);

    const uint8_t abc[] = "abc";
    uint8_t digest[20];
    fm_sha1(abc, 3, digest);
    const uint8_t expected[20] = {
        0xA9, 0x99, 0x3E, 0x36, 0x47, 0x06, 0x81, 0x6A, 0xBA, 0x3E,
        0x25, 0x71, 0x78, 0x50, 0xC2, 0x6C, 0x9C, 0xD0, 0xD8, 0x9D
    };
    assert(memcmp(digest, expected, 20) == 0);
}

static void test_ips_patcher(void) {
    uint8_t rom[64];
    memset(rom, 0, sizeof(rom));
    size_t sz = 16;

    /* PATCH + normal record @ 0x04 ("HI") + RLE @ 0x10 (4x 'A') + EOF */
    const uint8_t ips[] = {
        'P', 'A', 'T', 'C', 'H',
        0x00, 0x00, 0x04, 0x00, 0x02, 'H', 'I',
        0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x04, 'A',
        'E', 'O', 'F'
    };

    FmPatchRange ranges[8];
    size_t range_count = 0;
    char err[128] = "";
    bool ok = fm_apply_ips_memory(rom, &sz, sizeof(rom), ips, sizeof(ips),
                                  ranges, 8, &range_count, err, sizeof(err));
    assert(ok);
    assert(sz == 20);
    assert(rom[4] == 'H' && rom[5] == 'I');
    assert(rom[16] == 'A' && rom[17] == 'A' && rom[18] == 'A' && rom[19] == 'A');
    assert(range_count == 2);
}

static void test_hirom_checksum(void) {
    static uint8_t rom[FM_ROM_JP_SIZE];
    memset(rom, 0x01, sizeof(rom));
    assert(fm_update_hirom_checksum(rom, sizeof(rom)));
    uint16_t comp = (uint16_t)(rom[0xFFDC] | (rom[0xFFDD] << 8));
    uint16_t sum  = (uint16_t)(rom[0xFFDE] | (rom[0xFFDF] << 8));
    assert((comp ^ sum) == 0xFFFFu);
}

static void test_config_roundtrip(void) {
    FmUserConfig cfg;
    fm_config_init_defaults(&cfg);
    assert(cfg.boot_profile == FM_PROFILE_EN_ADDENDUM_103);
    assert(fm_config_save(&cfg, "/tmp/test_fm_config.ini"));

    FmUserConfig loaded;
    memset(&loaded, 0, sizeof(loaded));
    assert(fm_config_load(&loaded, "/tmp/test_fm_config.ini"));
    assert(loaded.boot_profile == FM_PROFILE_EN_ADDENDUM_103);
    assert(loaded.master_volume == 80);
    assert(loaded.window_scale == 2);
}

static void test_gui_menu_headless(void) {
    FmUserConfig cfg;
    fm_config_init_defaults(&cfg);
    FmGuiMenuState gui;
    fm_gui_menu_init(&gui, FM_GUI_SCREEN_LAUNCHER, true, FM_ROM_JP_SIZE);

    assert(fm_gui_menu_handle_key(&gui, &cfg, FM_GUI_KEY_ENTER) == FM_GUI_ACTION_CONFIG_CHANGED);
    assert(cfg.boot_profile == FM_PROFILE_JP_ORIGINAL);

    uint32_t canvas[800 * 448];
    memset(canvas, 0, sizeof(canvas));
    fm_gui_menu_render(&gui, &cfg, canvas);
    assert(canvas[0] != 0);
}

int main(void) {
    test_crc32_and_sha1();
    test_ips_patcher();
    test_hirom_checksum();
    test_config_roundtrip();
    test_gui_menu_headless();
    printf("All Phase 0 boot_loader/config/gui_menu unit tests passed.\n");
    return 0;
}
