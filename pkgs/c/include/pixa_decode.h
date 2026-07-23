#ifndef PIXA_DECODE_H
#define PIXA_DECODE_H

#include "pixa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int pixa_apply_clip_frame_bgra(const pixa_asset_t *asset,
                               const pixa_clip_t *clip,
                               uint32_t local_frame_index, uint8_t *out_bgra,
                               size_t out_len);
int pixa_decode_clip_frame_bgra(const pixa_asset_t *asset,
                                const char *clip_name, uint32_t frame_index,
                                uint8_t *out_bgra, size_t out_len);
int pixa_bgra_to_argb4444(const uint8_t *in_bgra, size_t in_len,
                          uint8_t *out_argb4444, size_t out_len);
int pixa_argb4444_to_bgra(const uint8_t *in_argb4444, size_t in_len,
                          uint8_t *out_bgra, size_t out_len);

#ifdef __cplusplus
}
#endif

#endif
