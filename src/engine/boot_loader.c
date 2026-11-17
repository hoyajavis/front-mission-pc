#include "../../include/boot_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * CRC32 (IEEE 802.3) & SHA-1 (RFC 3174)
 * ============================================================================ */

uint32_t fm_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint32_t)data[i];
        for (int b = 0; b < 8; ++b) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static uint32_t rotl32(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

void fm_sha1(const uint8_t *data, size_t len, uint8_t out_digest[20]) {
    uint32_t h0 = 0x67452301u;
    uint32_t h1 = 0xEFCDAB89u;
    uint32_t h2 = 0x98BADCFEu;
    uint32_t h3 = 0x10325476u;
    uint32_t h4 = 0xC3D2E1F0u;

    uint64_t total_bits = (uint64_t)len * 8u;
    size_t padded_len = ((len + 8u) / 64u + 1u) * 64u;
    uint8_t block[64];
    uint32_t w[80];

    for (size_t offset = 0; offset < padded_len; offset += 64u) {
        memset(block, 0, sizeof(block));
        for (size_t i = 0; i < 64u; ++i) {
            size_t idx = offset + i;
            if (idx < len) {
                block[i] = data[idx];
            } else if (idx == len) {
                block[i] = 0x80u;
            } else if (idx >= padded_len - 8u) {
                size_t shift = (padded_len - 1u - idx) * 8u;
                block[i] = (uint8_t)((total_bits >> shift) & 0xFFu);
            }
        }

        for (int i = 0; i < 16; ++i) {
            w[i] = ((uint32_t)block[i * 4 + 0] << 24) |
                   ((uint32_t)block[i * 4 + 1] << 16) |
                   ((uint32_t)block[i * 4 + 2] << 8)  |
                   ((uint32_t)block[i * 4 + 3]);
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999u;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1u;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDCu;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6u;
            }
            uint32_t temp = rotl32(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rotl32(b, 30);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    uint32_t hs[5] = { h0, h1, h2, h3, h4 };
    for (int i = 0; i < 5; ++i) {
        out_digest[i * 4 + 0] = (uint8_t)((hs[i] >> 24) & 0xFFu);
        out_digest[i * 4 + 1] = (uint8_t)((hs[i] >> 16) & 0xFFu);
        out_digest[i * 4 + 2] = (uint8_t)((hs[i] >> 8) & 0xFFu);
        out_digest[i * 4 + 3] = (uint8_t)(hs[i] & 0xFFu);
    }
}

/* ============================================================================
 * HiROM Checksum Updater (0xFFC0 Header)
 * ============================================================================ */

bool fm_update_hirom_checksum(uint8_t *rom, size_t rom_size) {
    if (!rom || rom_size < 0x10000u) return false;

    /* Reset complement & checksum bytes before summing */
    rom[0xFFDC] = 0xFFu;
    rom[0xFFDD] = 0xFFu;
    rom[0xFFDE] = 0x00u;
    rom[0xFFDF] = 0x00u;

    uint32_t sum = 0u;
    if (rom_size == FM_ROM_JP_SIZE) {
        /* 24 Mbit (3 MB): 2 MB base + 2x 1 MB mirror */
        for (size_t i = 0; i < 0x200000u; ++i) {
            sum += rom[i];
        }
        uint32_t tail_sum = 0u;
        for (size_t i = 0x200000u; i < 0x300000u; ++i) {
            tail_sum += rom[i];
        }
        sum += tail_sum * 2u;
    } else {
        /* Linear sum (e.g. 32 Mbit / 4 MB) */
        for (size_t i = 0; i < rom_size; ++i) {
            sum += rom[i];
        }
    }

    uint16_t checksum = (uint16_t)(sum & 0xFFFFu);
    uint16_t complement = (uint16_t)(checksum ^ 0xFFFFu);

    rom[0xFFDC] = (uint8_t)(complement & 0xFFu);
    rom[0xFFDD] = (uint8_t)((complement >> 8) & 0xFFu);
    rom[0xFFDE] = (uint8_t)(checksum & 0xFFu);
    rom[0xFFDF] = (uint8_t)((checksum >> 8) & 0xFFu);
    return true;
}

/* ============================================================================
 * Modified Range Tracking
 * ============================================================================ */

static void record_range(
    FmPatchRange *out_ranges,
    size_t max_ranges,
    size_t *io_range_count,
    uint32_t offset,
    uint32_t length
) {
    if (!out_ranges || !io_range_count || length == 0) return;
    if (*io_range_count > 0) {
        FmPatchRange *prev = &out_ranges[*io_range_count - 1];
        if (prev->offset + prev->length == offset) {
            prev->length += length;
            return;
        }
    }
    if (*io_range_count < max_ranges) {
        out_ranges[*io_range_count].offset = offset;
        out_ranges[*io_range_count].length = length;
        (*io_range_count)++;
    }
}

/* ============================================================================
 * Native C IPS Patcher
 * ============================================================================ */

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
) {
    if (!rom || !io_size || !patch_data) {
        snprintf(err_buf, err_buf_size, "Invalid NULL argument to fm_apply_ips_memory");
        return false;
    }
    if (patch_size < 8 || memcmp(patch_data, "PATCH", 5) != 0) {
        snprintf(err_buf, err_buf_size, "Invalid IPS header (missing PATCH magic)");
        return false;
    }

    size_t pos = 5;
    while (pos + 3 <= patch_size) {
        if (patch_data[pos] == 'E' && patch_data[pos + 1] == 'O' && patch_data[pos + 2] == 'F') {
            pos += 3;
            if (pos + 3 <= patch_size) {
                uint32_t trunc_size = ((uint32_t)patch_data[pos] << 16) |
                                      ((uint32_t)patch_data[pos + 1] << 8) |
                                      (uint32_t)patch_data[pos + 2];
                if (trunc_size <= max_capacity) {
                    *io_size = trunc_size;
                }
            }
            return true;
        }

        if (pos + 5 > patch_size) {
            snprintf(err_buf, err_buf_size, "Truncated IPS record header");
            return false;
        }

        uint32_t offset = ((uint32_t)patch_data[pos] << 16) |
                          ((uint32_t)patch_data[pos + 1] << 8) |
                          (uint32_t)patch_data[pos + 2];
        uint16_t rec_size = (uint16_t)(((uint16_t)patch_data[pos + 3] << 8) |
                                       (uint16_t)patch_data[pos + 4]);
        pos += 5;

        if (rec_size > 0) {
            if (pos + rec_size > patch_size) {
                snprintf(err_buf, err_buf_size, "Truncated IPS data payload");
                return false;
            }
            if ((size_t)offset + rec_size > max_capacity) {
                snprintf(err_buf, err_buf_size, "IPS write exceeds ROM capacity (0x%X)", offset + rec_size);
                return false;
            }
            memcpy(rom + offset, patch_data + pos, rec_size);
            if ((size_t)offset + rec_size > *io_size) {
                *io_size = (size_t)offset + rec_size;
            }
            record_range(out_ranges, max_ranges, io_range_count, offset, rec_size);
            pos += rec_size;
        } else {
            if (pos + 3 > patch_size) {
                snprintf(err_buf, err_buf_size, "Truncated IPS RLE record");
                return false;
            }
            uint16_t rle_len = (uint16_t)(((uint16_t)patch_data[pos] << 8) |
                                          (uint16_t)patch_data[pos + 1]);
            uint8_t rle_val = patch_data[pos + 2];
            pos += 3;

            if ((size_t)offset + rle_len > max_capacity) {
                snprintf(err_buf, err_buf_size, "IPS RLE write exceeds ROM capacity");
                return false;
            }
            memset(rom + offset, rle_val, rle_len);
            if ((size_t)offset + rle_len > *io_size) {
                *io_size = (size_t)offset + rle_len;
            }
            record_range(out_ranges, max_ranges, io_range_count, offset, rle_len);
        }
    }

    snprintf(err_buf, err_buf_size, "IPS patch missing EOF marker");
    return false;
}

/* ============================================================================
 * Native C BPS Patcher
 * ============================================================================ */

static uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static bool bps_read_varint(const uint8_t *data, size_t size, size_t *io_pos, uint64_t *out_val) {
    uint64_t val = 0;
    uint64_t shift = 1;
    while (*io_pos < size) {
        uint8_t x = data[(*io_pos)++];
        val += (uint64_t)(x & 0x7Fu) * shift;
        if (x & 0x80u) {
            *out_val = val;
            return true;
        }
        shift <<= 7;
        val += shift;
    }
    return false;
}

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
) {
    if (!src_rom || !patch_data || !dst_rom || !out_dst_size) {
        snprintf(err_buf, err_buf_size, "Invalid NULL argument to fm_apply_bps_memory");
        return false;
    }
    if (patch_size < 16 || memcmp(patch_data, "BPS1", 4) != 0) {
        snprintf(err_buf, err_buf_size, "Invalid BPS header (missing BPS1 magic)");
        return false;
    }

    uint32_t expected_patch_crc = read_le32(patch_data + patch_size - 4);
    uint32_t actual_patch_crc = fm_crc32(patch_data, patch_size - 4);
    if (expected_patch_crc != actual_patch_crc) {
        snprintf(err_buf, err_buf_size, "BPS patch CRC32 mismatch");
        return false;
    }

    uint32_t expected_src_crc = read_le32(patch_data + patch_size - 12);
    uint32_t expected_dst_crc = read_le32(patch_data + patch_size - 8);
    uint32_t actual_src_crc = fm_crc32(src_rom, src_size);
    if (expected_src_crc != actual_src_crc) {
        snprintf(err_buf, err_buf_size, "BPS source CRC32 mismatch (expected %08X, got %08X)",
                 expected_src_crc, actual_src_crc);
        return false;
    }

    size_t pos = 4;
    size_t cmd_end = patch_size - 12;
    uint64_t bps_src_size = 0, bps_dst_size = 0, meta_size = 0;
    if (!bps_read_varint(patch_data, cmd_end, &pos, &bps_src_size) ||
        !bps_read_varint(patch_data, cmd_end, &pos, &bps_dst_size) ||
        !bps_read_varint(patch_data, cmd_end, &pos, &meta_size)) {
        snprintf(err_buf, err_buf_size, "Truncated BPS header metadata");
        return false;
    }

    if (bps_src_size != src_size) {
        snprintf(err_buf, err_buf_size, "BPS source size mismatch");
        return false;
    }
    if (bps_dst_size > dst_capacity) {
        snprintf(err_buf, err_buf_size, "BPS target size exceeds ROM buffer capacity");
        return false;
    }
    if (pos + meta_size > cmd_end) {
        snprintf(err_buf, err_buf_size, "BPS metadata overflows patch");
        return false;
    }
    pos += (size_t)meta_size;

    size_t out_pos = 0;
    int64_t src_rel_off = 0;
    int64_t dst_rel_off = 0;

    while (pos < cmd_end) {
        uint64_t data = 0;
        if (!bps_read_varint(patch_data, cmd_end, &pos, &data)) {
            snprintf(err_buf, err_buf_size, "Truncated BPS command stream");
            return false;
        }
        uint64_t cmd = data & 3u;
        uint64_t len = (data >> 2) + 1u;
        if (out_pos + len > bps_dst_size) {
            snprintf(err_buf, err_buf_size, "BPS output overflow");
            return false;
        }

        if (cmd == 0) { /* SourceRead */
            for (uint64_t i = 0; i < len; ++i) {
                dst_rom[out_pos] = (out_pos < src_size) ? src_rom[out_pos] : 0u;
                out_pos++;
            }
        } else if (cmd == 1) { /* TargetRead */
            if (pos + len > cmd_end) {
                snprintf(err_buf, err_buf_size, "BPS TargetRead overflow");
                return false;
            }
            memcpy(dst_rom + out_pos, patch_data + pos, (size_t)len);
            pos += (size_t)len;
            out_pos += (size_t)len;
        } else if (cmd == 2) { /* SourceCopy */
            uint64_t off_raw = 0;
            if (!bps_read_varint(patch_data, cmd_end, &pos, &off_raw)) {
                snprintf(err_buf, err_buf_size, "BPS SourceCopy missing offset");
                return false;
            }
            int64_t delta = (int64_t)(off_raw >> 1);
            if (off_raw & 1u) delta = -delta;
            src_rel_off += delta;
            for (uint64_t i = 0; i < len; ++i) {
                if (src_rel_off < 0 || (uint64_t)src_rel_off >= src_size) {
                    snprintf(err_buf, err_buf_size, "BPS SourceCopy out of bounds");
                    return false;
                }
                dst_rom[out_pos++] = src_rom[src_rel_off++];
            }
        } else { /* TargetCopy */
            uint64_t off_raw = 0;
            if (!bps_read_varint(patch_data, cmd_end, &pos, &off_raw)) {
                snprintf(err_buf, err_buf_size, "BPS TargetCopy missing offset");
                return false;
            }
            int64_t delta = (int64_t)(off_raw >> 1);
            if (off_raw & 1u) delta = -delta;
            dst_rel_off += delta;
            for (uint64_t i = 0; i < len; ++i) {
                if (dst_rel_off < 0 || (uint64_t)dst_rel_off >= out_pos) {
                    snprintf(err_buf, err_buf_size, "BPS TargetCopy out of bounds");
                    return false;
                }
                dst_rom[out_pos++] = dst_rom[dst_rel_off++];
            }
        }
    }

    if (out_pos != bps_dst_size) {
        snprintf(err_buf, err_buf_size, "BPS output size mismatch");
        return false;
    }

    uint32_t actual_dst_crc = fm_crc32(dst_rom, out_pos);
    if (expected_dst_crc != actual_dst_crc) {
        snprintf(err_buf, err_buf_size, "BPS target CRC32 mismatch (expected %08X, got %08X)",
                 expected_dst_crc, actual_dst_crc);
        return false;
    }

    /* Record modified spans relative to source ROM */
    if (out_ranges && io_range_count) {
        size_t i = 0;
        while (i < out_pos) {
            bool changed = (i >= src_size) || (dst_rom[i] != src_rom[i]);
            if (!changed) {
                i++;
                continue;
            }
            size_t start = i;
            while (i < out_pos && ((i >= src_size) || (dst_rom[i] != src_rom[i]))) {
                i++;
            }
            record_range(out_ranges, max_ranges, io_range_count, (uint32_t)start, (uint32_t)(i - start));
        }
    }

    *out_dst_size = out_pos;
    return true;
}

/* ============================================================================
 * File Helpers & Boot Loader
 * ============================================================================ */

static bool read_file_alloc(const char *path, uint8_t **out_buf, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    long sz = ftell(f);
    if (sz <= 0) {
        fclose(f);
        return false;
    }
    rewind(f);
    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        return false;
    }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (n != (size_t)sz) {
        free(buf);
        return false;
    }
    *out_buf = buf;
    *out_size = (size_t)sz;
    return true;
}

const char *fm_boot_profile_name(FmBootProfileId profile) {
    switch (profile) {
        case FM_PROFILE_JP_ORIGINAL:     return "Japanese Original (3 MB)";
        case FM_PROFILE_EN_FH_10B:       return "English (F.H. v1.0b)";
        case FM_PROFILE_EN_ADDENDUM_103: return "English + Addendum v1.03";
        default:                         return "Unknown";
    }
}

bool fm_boot_load_rom(const FmBootConfig *cfg, FmBootResult *out_result) {
    if (!cfg || !out_result || !cfg->rom_path) return false;
    memset(out_result, 0, sizeof(*out_result));
    out_result->active_profile = cfg->profile;

    uint8_t *raw_file = NULL;
    size_t raw_size = 0;
    if (!read_file_alloc(cfg->rom_path, &raw_file, &raw_size)) {
        snprintf(out_result->error_msg, sizeof(out_result->error_msg),
                 "Failed to open ROM: %s", cfg->rom_path);
        return false;
    }

    /* Strip optional 512-byte SMC copier header */
    const uint8_t *rom_src = raw_file;
    size_t rom_len = raw_size;
    if ((raw_size % 1024u) == 512u && raw_size > 512u) {
        rom_src += 512u;
        rom_len -= 512u;
    }

    if (rom_len < FM_ROM_JP_SIZE || rom_len > FM_ROM_EN_MAX_SIZE) {
        snprintf(out_result->error_msg, sizeof(out_result->error_msg),
                 "Unexpected ROM size: %zu bytes", rom_len);
        free(raw_file);
        return false;
    }

    uint8_t *rom_buf = (uint8_t *)calloc(1, FM_ROM_EN_MAX_SIZE);
    FmPatchRange *ranges = (FmPatchRange *)calloc(FM_MAX_PATCH_RANGES, sizeof(FmPatchRange));
    if (!rom_buf || !ranges) {
        snprintf(out_result->error_msg, sizeof(out_result->error_msg), "Out of memory");
        free(raw_file);
        free(rom_buf);
        free(ranges);
        return false;
    }
    memcpy(rom_buf, rom_src, rom_len);
    free(raw_file);

    uint32_t initial_crc = fm_crc32(rom_buf, rom_len);

    /* Pre-patched ROM fallback detection */
    if (initial_crc != FM_CRC32_JP_ORIGINAL && cfg->profile != FM_PROFILE_JP_ORIGINAL) {
        out_result->pre_patched_fallback = true;
    } else if (cfg->profile == FM_PROFILE_EN_FH_10B || cfg->profile == FM_PROFILE_EN_ADDENDUM_103) {
        if (!cfg->patches_dir) {
            snprintf(out_result->error_msg, sizeof(out_result->error_msg), "patches_dir is NULL");
            free(rom_buf);
            free(ranges);
            return false;
        }

        char fh_path[1024];
        snprintf(fh_path, sizeof(fh_path), "%s/FMSNE10B.IPS", cfg->patches_dir);
        uint8_t *fh_patch = NULL;
        size_t fh_size = 0;
        if (!read_file_alloc(fh_path, &fh_patch, &fh_size)) {
            snprintf(out_result->error_msg, sizeof(out_result->error_msg),
                     "Missing patch: %.200s", fh_path);
            free(rom_buf);
            free(ranges);
            return false;
        }

        bool ok = fm_apply_ips_memory(
            rom_buf, &rom_len, FM_ROM_EN_MAX_SIZE,
            fh_patch, fh_size,
            ranges, FM_MAX_PATCH_RANGES, &out_result->modified_range_count,
            out_result->error_msg, sizeof(out_result->error_msg)
        );
        free(fh_patch);
        if (!ok) {
            free(rom_buf);
            free(ranges);
            return false;
        }

        if (cfg->profile == FM_PROFILE_EN_ADDENDUM_103) {
            char bps_path[1024];
            char ips_path[1024];
            snprintf(bps_path, sizeof(bps_path), "%s/frontmission_addendum_v103.bps", cfg->patches_dir);
            snprintf(ips_path, sizeof(ips_path), "%s/frontmission_addendum_v103.ips", cfg->patches_dir);

            uint8_t *add_patch = NULL;
            size_t add_size = 0;
            if (read_file_alloc(bps_path, &add_patch, &add_size)) {
                uint8_t *dst_buf = (uint8_t *)calloc(1, FM_ROM_EN_MAX_SIZE);
                if (!dst_buf) {
                    free(add_patch);
                    free(rom_buf);
                    free(ranges);
                    return false;
                }
                size_t new_len = 0;
                ok = fm_apply_bps_memory(
                    rom_buf, rom_len, add_patch, add_size,
                    dst_buf, FM_ROM_EN_MAX_SIZE, &new_len,
                    ranges, FM_MAX_PATCH_RANGES, &out_result->modified_range_count,
                    out_result->error_msg, sizeof(out_result->error_msg)
                );
                free(add_patch);
                if (!ok) {
                    free(dst_buf);
                    free(rom_buf);
                    free(ranges);
                    return false;
                }
                free(rom_buf);
                rom_buf = dst_buf;
                rom_len = new_len;
            } else if (read_file_alloc(ips_path, &add_patch, &add_size)) {
                ok = fm_apply_ips_memory(
                    rom_buf, &rom_len, FM_ROM_EN_MAX_SIZE,
                    add_patch, add_size,
                    ranges, FM_MAX_PATCH_RANGES, &out_result->modified_range_count,
                    out_result->error_msg, sizeof(out_result->error_msg)
                );
                free(add_patch);
                if (!ok) {
                    free(rom_buf);
                    free(ranges);
                    return false;
                }
            } else {
                snprintf(out_result->error_msg, sizeof(out_result->error_msg),
                         "Missing Addendum v1.03 patch (.bps or .ips) in %.180s", cfg->patches_dir);
                free(rom_buf);
                free(ranges);
                return false;
            }
        }
    }

    if (cfg->recalculate_checksum) {
        fm_update_hirom_checksum(rom_buf, rom_len);
    }

    out_result->ok = true;
    out_result->rom_data = rom_buf;
    out_result->rom_size = rom_len;
    out_result->crc32 = fm_crc32(rom_buf, rom_len);
    fm_sha1(rom_buf, rom_len, out_result->sha1);
    out_result->modified_ranges = ranges;
    return true;
}

void fm_boot_free_result(FmBootResult *result) {
    if (!result) return;
    free(result->rom_data);
    free(result->modified_ranges);
    memset(result, 0, sizeof(*result));
}
