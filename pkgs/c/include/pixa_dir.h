#ifndef PIXA_DIR_H
#define PIXA_DIR_H

#include "pixa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opens and validates a borrowed binary directory index.
 *
 * data remains owned by the caller and must outlive out_asset and every clip
 * view obtained from it. On failure out_asset is cleared.
 */
int pixa_dir_open_memory(const void *data, size_t len,
                         pixa_dir_asset_t *out_asset);
/** Returns a borrowed clip view. On failure out_clip is cleared. */
int pixa_dir_clip_at(const pixa_dir_asset_t *asset, uint32_t index,
                     pixa_dir_clip_t *out_clip);
/** Finds a borrowed clip view by safe identifier. On failure out_clip is
 * cleared. */
int pixa_dir_find_clip(const pixa_dir_asset_t *asset, const char *name,
                       pixa_dir_clip_t *out_clip);
/** Reads one frame duration from a validated borrowed clip view. */
int pixa_dir_clip_duration_at(const pixa_dir_clip_t *clip, uint32_t index,
                              uint16_t *out_duration_ms);
/** Selects a frame using the clip loop and duration metadata. */
int pixa_dir_frame_index_at_ms(const pixa_dir_clip_t *clip, uint64_t time_ms,
                               uint32_t *out_index);
/**
 * Encodes one validated directory index into caller-owned storage.
 *
 * Passing NULL out or insufficient capacity returns OUTPUT_TOO_SMALL and
 * reports the required size through out_len. Input names and duration buffers
 * are borrowed only for the call.
 */
int pixa_dir_encode(const pixa_canvas_t *canvas, const pixa_dir_clip_t *clips,
                    uint32_t clip_count, void *out, size_t capacity,
                    size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif
