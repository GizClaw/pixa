#ifndef PIXA_READER_H
#define PIXA_READER_H

#include "pixa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int pixa_open_memory(const void *data, size_t len, pixa_asset_t *out_asset);
int pixa_clip_at(const pixa_asset_t *asset, uint16_t index,
                 pixa_clip_t *out_clip);
int pixa_find_clip(const pixa_asset_t *asset, const char *name,
                   pixa_clip_t *out_clip);
int pixa_find_clip_or_default(const pixa_asset_t *asset, const char *name,
                              pixa_clip_t *out_clip);
int pixa_frame_at(const pixa_asset_t *asset, uint32_t index,
                  pixa_frame_t *out_frame);
int pixa_frame_index_at_ms(const pixa_asset_t *asset, const pixa_clip_t *clip,
                           uint64_t time_ms, uint32_t *out_index);

#ifdef __cplusplus
}
#endif

#endif
