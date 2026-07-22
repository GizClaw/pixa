#include "pixa_blit.h"

static uint16_t read_le16(const uint8_t *data) {
  return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint16_t argb4444_to_rgb565(uint16_t value) {
  uint8_t r4 = (uint8_t)((value >> 8) & 0x0fu);
  uint8_t g4 = (uint8_t)((value >> 4) & 0x0fu);
  uint8_t b4 = (uint8_t)(value & 0x0fu);
  uint8_t r5 = (uint8_t)((r4 << 1) | (r4 >> 3));
  uint8_t g6 = (uint8_t)((g4 << 2) | (g4 >> 2));
  uint8_t b5 = (uint8_t)((b4 << 1) | (b4 >> 3));
  return (uint16_t)(((uint16_t)r5 << 11) | ((uint16_t)g6 << 5) | b5);
}

static uint16_t blend_rgb565_a4(uint16_t dst, uint16_t src, uint8_t a4) {
  uint8_t inv = (uint8_t)(15u - a4);
  uint16_t sr = (uint16_t)((src >> 11) & 0x1fu);
  uint16_t sg = (uint16_t)((src >> 5) & 0x3fu);
  uint16_t sb = (uint16_t)(src & 0x1fu);
  uint16_t dr = (uint16_t)((dst >> 11) & 0x1fu);
  uint16_t dg = (uint16_t)((dst >> 5) & 0x3fu);
  uint16_t db = (uint16_t)(dst & 0x1fu);
  uint16_t r = (uint16_t)((sr * a4 + dr * inv) / 15u);
  uint16_t g = (uint16_t)((sg * a4 + dg * inv) / 15u);
  uint16_t b = (uint16_t)((sb * a4 + db * inv) / 15u);
  return (uint16_t)((r << 11) | (g << 5) | b);
}

int pixa_blit_argb4444_to_rgb565(uint16_t *dst_rgb565, uint16_t dst_width,
                                 uint16_t dst_height,
                                 uint16_t dst_stride_pixels,
                                 const uint8_t *src_argb4444,
                                 uint16_t src_width, uint16_t src_height,
                                 int16_t dst_x, int16_t dst_y) {
  if (dst_rgb565 == 0 || src_argb4444 == 0 || dst_width == 0u ||
      dst_height == 0u || dst_stride_pixels < dst_width || src_width == 0u ||
      src_height == 0u) {
    return PIXA_ERR_INVALID_ARG;
  }

  for (uint16_t sy = 0u; sy < src_height; ++sy) {
    int32_t y = (int32_t)dst_y + (int32_t)sy;
    if (y < 0 || y >= (int32_t)dst_height) {
      continue;
    }

    for (uint16_t sx = 0u; sx < src_width; ++sx) {
      int32_t x = (int32_t)dst_x + (int32_t)sx;
      uint16_t value;
      uint8_t a4;
      uint16_t src;
      uint16_t *dst;

      if (x < 0 || x >= (int32_t)dst_width) {
        continue;
      }

      value = read_le16(src_argb4444 + ((size_t)sy * src_width + sx) *
                                           PIXA_ARGB4444_PIXEL_BYTES);
      a4 = (uint8_t)((value >> 12) & 0x0fu);
      if (a4 == 0u) {
        continue;
      }

      src = argb4444_to_rgb565(value);
      dst = dst_rgb565 + (size_t)y * dst_stride_pixels + (size_t)x;
      *dst = a4 == 15u ? src : blend_rgb565_a4(*dst, src, a4);
    }
  }

  return PIXA_OK;
}

int pixa_blit_argb4444_scaled_to_rgb565(
    uint16_t *dst_rgb565, uint16_t dst_width, uint16_t dst_height,
    uint16_t dst_stride_pixels, const uint8_t *src_argb4444, uint16_t src_width,
    uint16_t src_height, int16_t dst_x, int16_t dst_y, uint16_t scaled_width,
    uint16_t scaled_height) {
  if (dst_rgb565 == 0 || src_argb4444 == 0 || dst_width == 0u ||
      dst_height == 0u || dst_stride_pixels < dst_width || src_width == 0u ||
      src_height == 0u || scaled_width == 0u || scaled_height == 0u) {
    return PIXA_ERR_INVALID_ARG;
  }

  for (uint16_t dy = 0u; dy < scaled_height; ++dy) {
    const int32_t y = (int32_t)dst_y + (int32_t)dy;
    if (y < 0 || y >= (int32_t)dst_height) {
      continue;
    }
    const uint16_t sy = (uint16_t)(((uint32_t)dy * src_height) / scaled_height);
    for (uint16_t dx = 0u; dx < scaled_width; ++dx) {
      const int32_t x = (int32_t)dst_x + (int32_t)dx;
      uint16_t value;
      uint8_t a4;
      uint16_t src;
      uint16_t *dst;

      if (x < 0 || x >= (int32_t)dst_width) {
        continue;
      }
      const uint16_t sx = (uint16_t)(((uint32_t)dx * src_width) / scaled_width);
      value = read_le16(src_argb4444 + ((size_t)sy * src_width + sx) *
                                           PIXA_ARGB4444_PIXEL_BYTES);
      a4 = (uint8_t)((value >> 12) & 0x0fu);
      if (a4 == 0u) {
        continue;
      }
      src = argb4444_to_rgb565(value);
      dst = dst_rgb565 + (size_t)y * dst_stride_pixels + (size_t)x;
      *dst = a4 == 15u ? src : blend_rgb565_a4(*dst, src, a4);
    }
  }
  return PIXA_OK;
}
