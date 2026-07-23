#ifndef PIXA_BLIT_H
#define PIXA_BLIT_H

#include "pixa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int pixa_blit_argb4444_to_rgb565(uint16_t *dst_rgb565, uint16_t dst_width,
                                 uint16_t dst_height,
                                 uint16_t dst_stride_pixels,
                                 const uint8_t *src_argb4444,
                                 uint16_t src_width, uint16_t src_height,
                                 int16_t dst_x, int16_t dst_y);

int pixa_blit_argb4444_scaled_to_rgb565(
    uint16_t *dst_rgb565, uint16_t dst_width, uint16_t dst_height,
    uint16_t dst_stride_pixels, const uint8_t *src_argb4444, uint16_t src_width,
    uint16_t src_height, int16_t dst_x, int16_t dst_y, uint16_t scaled_width,
    uint16_t scaled_height);

#ifdef __cplusplus
}
#endif

#endif
