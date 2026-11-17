#ifndef FM_REPLAY_H
#define FM_REPLAY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FM_REPLAY_MAGIC_STR "FMREC\0\0\0"
#define FM_REPLAY_VERSION   1u

/* LakeSnes joypad bitmask constants (snes->input1->currentState) */
#define FM_BTN_NONE   0x0000u
#define FM_BTN_B      (1u << 0)
#define FM_BTN_Y      (1u << 1)
#define FM_BTN_SELECT (1u << 2)
#define FM_BTN_START  (1u << 3)
#define FM_BTN_UP     (1u << 4)
#define FM_BTN_DOWN   (1u << 5)
#define FM_BTN_LEFT   (1u << 6)
#define FM_BTN_RIGHT  (1u << 7)
#define FM_BTN_A      (1u << 8)
#define FM_BTN_X      (1u << 9)
#define FM_BTN_L      (1u << 10)
#define FM_BTN_R      (1u << 11)

typedef struct FmReplayHeader {
    char     magic[8];
    uint32_t version;
    uint32_t profile_id;
    uint32_t rom_crc32;
    uint32_t frame_count;
    uint32_t flags;
    uint32_t reserved[3];
} FmReplayHeader;

typedef struct FmReplayTrace {
    FmReplayHeader header;
    uint16_t      *inputs;
    uint32_t       capacity;
} FmReplayTrace;

FmReplayTrace *fm_replay_create(uint32_t profile_id, uint32_t rom_crc32, uint32_t initial_capacity);
bool           fm_replay_push_frame(FmReplayTrace *trace, uint16_t buttons);
bool           fm_replay_save(const FmReplayTrace *trace, const char *path);
FmReplayTrace *fm_replay_load(const char *path);
uint16_t       fm_replay_get_frame(const FmReplayTrace *trace, uint32_t frame_idx);
void           fm_replay_free(FmReplayTrace *trace);

#ifdef __cplusplus
}
#endif

#endif /* FM_REPLAY_H */
