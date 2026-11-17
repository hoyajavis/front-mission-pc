#ifndef FM_BOOT_LOADER_H
#define FM_BOOT_LOADER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FM_ROM_JP_SIZE          (3u * 1024u * 1024u) /* 3.0 MB (24 Mbit) */
#define FM_ROM_EN_MAX_SIZE      (4u * 1024u * 1024u) /* 4.0 MB (32 Mbit) */

#define FM_CRC32_JP_ORIGINAL    0xFE27E061u
#define FM_CRC32_EN_FH_10B      0x2B4CD5A3u

#define FM_MAX_PATCH_RANGES     4096

typedef enum FmBootProfileId {
    FM_PROFILE_JP_ORIGINAL      = 0,
    FM_PROFILE_EN_FH_10B        = 1,
    FM_PROFILE_EN_ADDENDUM_103  = 2,
    FM_PROFILE_COUNT            = 3
} FmBootProfileId;

typedef struct FmPatchRange {
    uint32_t offset;
    uint32_t length;
} FmPatchRange;

typedef struct FmBootConfig {
    const char *rom_path;
    const char *patches_dir;
    FmBootProfileId profile;
    bool recalculate_checksum;
} FmBootConfig;

typedef struct FmBootResult {
    bool ok;
    uint8_t *rom_data;          /* Caller owns via fm_boot_free_result() */
    size_t rom_size;
    uint32_t crc32;
    uint8_t sha1[20];
    FmBootProfileId active_profile;
    bool pre_patched_fallback;
    FmPatchRange *modified_ranges;
    size_t modified_range_count;
    char error_msg[256];
} FmBootResult;

/* Compute standard IEEE 802.3 CRC32 */
uint32_t fm_crc32(const uint8_t *data, size_t len);

/* Compute SHA-1 digest (20 bytes) */
void fm_sha1(const uint8_t *data, size_t len, uint8_t out_digest[20]);

/* Update SNES HiROM header checksum/complement at 0xFFDC..0xFFDF.
 * Handles 24Mbit (3 MB: 2MB + 2x1MB mirror) and 32Mbit (4 MB: linear) ROMs. */
bool fm_update_hirom_checksum(uint8_t *rom, size_t rom_size);

/* Apply an IPS patch in memory. Expands *io_size up to max_capacity if needed.
 * Records modified byte ranges if out_ranges != NULL. */
bool fm_apply_ips_memory(
    uint8_t *rom,
    size_t *io_size,
    size_t max_capacity,
    const uint8_t *patch_data,
    size_t patch_size,
    FmPatchRange *out_ranges,
    size_t max_ranges,
    size_t *io_range_count,
    char *err_buf,
    size_t err_buf_size
);

/* Apply a BPS patch in memory. Verifies source, target, and patch CRC32s.
 * Records modified byte ranges (diff against source) if out_ranges != NULL. */
bool fm_apply_bps_memory(
    const uint8_t *src_rom,
    size_t src_size,
    const uint8_t *patch_data,
    size_t patch_size,
    uint8_t *dst_rom,
    size_t dst_capacity,
    size_t *out_dst_size,
    FmPatchRange *out_ranges,
    size_t max_ranges,
    size_t *io_range_count,
    char *err_buf,
    size_t err_buf_size
);

/* Load ROM from disk, apply selected profile's IPS/BPS patch chain in memory,
 * update HiROM checksum if requested, and return buffer in out_result. */
bool fm_boot_load_rom(const FmBootConfig *cfg, FmBootResult *out_result);

/* Free heap buffers inside FmBootResult */
void fm_boot_free_result(FmBootResult *result);

/* Human-readable name for FmBootProfileId */
const char *fm_boot_profile_name(FmBootProfileId profile);

/* Map a 24-bit SNES HiROM address to a linear ROM file offset */
bool fm_hirom_addr_to_offset(uint32_t snes_addr, size_t rom_size, uint32_t *out_offset);

/* Return true if [offset, offset + length) overlaps any modified patch range */
bool fm_patch_range_overlaps(const FmPatchRange *ranges, size_t range_count, uint32_t offset, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif /* FM_BOOT_LOADER_H */
