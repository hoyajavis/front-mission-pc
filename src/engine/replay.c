#include "replay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FmReplayTrace *fm_replay_create(uint32_t profile_id, uint32_t rom_crc32, uint32_t initial_capacity) {
    FmReplayTrace *trace = (FmReplayTrace *)calloc(1, sizeof(FmReplayTrace));
    if (!trace) return NULL;

    memcpy(trace->header.magic, FM_REPLAY_MAGIC_STR, 8);
    trace->header.version = FM_REPLAY_VERSION;
    trace->header.profile_id = profile_id;
    trace->header.rom_crc32 = rom_crc32;
    trace->header.frame_count = 0;

    if (initial_capacity == 0) initial_capacity = 1024;
    trace->inputs = (uint16_t *)calloc(initial_capacity, sizeof(uint16_t));
    if (!trace->inputs) {
        free(trace);
        return NULL;
    }
    trace->capacity = initial_capacity;
    return trace;
}

bool fm_replay_push_frame(FmReplayTrace *trace, uint16_t buttons) {
    if (!trace) return false;
    if (trace->header.frame_count >= trace->capacity) {
        uint32_t new_cap = trace->capacity ? trace->capacity * 2u : 1024u;
        uint16_t *new_buf = (uint16_t *)realloc(trace->inputs, (size_t)new_cap * sizeof(uint16_t));
        if (!new_buf) return false;
        trace->inputs = new_buf;
        trace->capacity = new_cap;
    }
    trace->inputs[trace->header.frame_count++] = buttons;
    return true;
}

bool fm_replay_save(const FmReplayTrace *trace, const char *path) {
    if (!trace || !path) return false;
    FILE *f = fopen(path, "wb");
    if (!f) return false;

    if (fwrite(&trace->header, sizeof(FmReplayHeader), 1, f) != 1) {
        fclose(f);
        return false;
    }
    if (trace->header.frame_count > 0) {
        if (fwrite(trace->inputs, sizeof(uint16_t), trace->header.frame_count, f) != trace->header.frame_count) {
            fclose(f);
            return false;
        }
    }
    fclose(f);
    return true;
}

FmReplayTrace *fm_replay_load(const char *path) {
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    FmReplayHeader hdr;
    if (fread(&hdr, sizeof(FmReplayHeader), 1, f) != 1) {
        fclose(f);
        return NULL;
    }
    if (memcmp(hdr.magic, FM_REPLAY_MAGIC_STR, 8) != 0 || hdr.version != FM_REPLAY_VERSION) {
        fclose(f);
        return NULL;
    }

    FmReplayTrace *trace = (FmReplayTrace *)calloc(1, sizeof(FmReplayTrace));
    if (!trace) {
        fclose(f);
        return NULL;
    }
    trace->header = hdr;
    trace->capacity = hdr.frame_count > 0 ? hdr.frame_count : 1u;
    trace->inputs = (uint16_t *)calloc(trace->capacity, sizeof(uint16_t));
    if (!trace->inputs) {
        free(trace);
        fclose(f);
        return NULL;
    }

    if (hdr.frame_count > 0) {
        if (fread(trace->inputs, sizeof(uint16_t), hdr.frame_count, f) != hdr.frame_count) {
            fm_replay_free(trace);
            fclose(f);
            return NULL;
        }
    }
    fclose(f);
    return trace;
}

uint16_t fm_replay_get_frame(const FmReplayTrace *trace, uint32_t frame_idx) {
    if (!trace || frame_idx >= trace->header.frame_count) return FM_BTN_NONE;
    return trace->inputs[frame_idx];
}

void fm_replay_free(FmReplayTrace *trace) {
    if (!trace) return;
    free(trace->inputs);
    free(trace);
}
