#include "pixa_decode.h"

#include "pixa_reader.h"

#include <string.h>

#define PIXA_FRAME_KEY 0u
#define PIXA_FRAME_DIFF 1u
#define PIXA_KEY_ENCODING_LEGACY 0u
#define PIXA_KEY_ENCODING_PALETTE_RLE 1u
#define PIXA_KEY_ENCODING_RGB565 2u

static uint16_t read_u16(const uint8_t *data, size_t offset) {
  return (uint16_t)data[offset] | ((uint16_t)data[offset + 1u] << 8);
}

static uint32_t read_u32(const uint8_t *data, size_t offset) {
  return (uint32_t)data[offset] | ((uint32_t)data[offset + 1u] << 8) |
         ((uint32_t)data[offset + 2u] << 16) |
         ((uint32_t)data[offset + 3u] << 24);
}

static int palette_color(const pixa_asset_t *asset, uint8_t index,
                         uint16_t *out_color) {
  if ((uint16_t)index >= asset->color_count) {
    return PIXA_ERR_INVALID_FORMAT;
  }
  *out_color =
      read_u16(asset->data, (size_t)asset->palette_offset + (size_t)index * 2u);
  return PIXA_OK;
}

static void write_rgb565_as_bgra(uint16_t color, uint8_t *out) {
  {
    uint8_t r5 = (uint8_t)((color >> 11) & 0x1fu);
    uint8_t g6 = (uint8_t)((color >> 5) & 0x3fu);
    uint8_t b5 = (uint8_t)(color & 0x1fu);
    out[0] = (uint8_t)((b5 << 3) | (b5 >> 2));
    out[1] = (uint8_t)((g6 << 2) | (g6 >> 4));
    out[2] = (uint8_t)((r5 << 3) | (r5 >> 2));
    out[3] = 255u;
  }
}

static int decode_rgb565_frame_bgra(const pixa_asset_t *asset,
                                    const uint8_t *payload, size_t payload_len,
                                    uint8_t *out_bgra) {
  const size_t pixel_count = pixa_canvas_pixel_count(asset->canvas);
  if (pixel_count > SIZE_MAX / PIXA_ARGB4444_PIXEL_BYTES ||
      payload_len != pixel_count * PIXA_ARGB4444_PIXEL_BYTES) {
    return PIXA_ERR_INVALID_FORMAT;
  }
  for (size_t index = 0u; index < pixel_count; ++index) {
    write_rgb565_as_bgra(read_u16(payload, index * 2u),
                         out_bgra + index * PIXA_PIXEL_BYTES);
  }
  return PIXA_OK;
}

static int decode_rle_rect_bgra(const pixa_asset_t *asset, const uint8_t *rle,
                                size_t rle_len, uint16_t x, uint16_t y,
                                uint16_t w, uint16_t h, uint8_t *out_bgra) {
  size_t rect_pixel = 0u;
  size_t rect_pixels = (size_t)w * (size_t)h;

  if ((rle_len & 1u) != 0u) {
    return PIXA_ERR_INVALID_FORMAT;
  }
  for (size_t i = 0u; i < rle_len; i += 2u) {
    uint8_t count = rle[i];
    uint8_t color_index = rle[i + 1u];
    uint16_t color;
    int rc = palette_color(asset, color_index, &color);
    if (count == 0u || (size_t)count > rect_pixels - rect_pixel) {
      return PIXA_ERR_INVALID_FORMAT;
    }
    if (rc != PIXA_OK) {
      return rc;
    }
    for (uint8_t n = 0u; n < count; ++n) {
      size_t dx = rect_pixel % w;
      size_t dy = rect_pixel / w;
      size_t pixel_index =
          ((size_t)y + dy) * asset->canvas.width + ((size_t)x + dx);
      write_rgb565_as_bgra(color, out_bgra + pixel_index * PIXA_PIXEL_BYTES);
      ++rect_pixel;
    }
  }

  return rect_pixel == rect_pixels ? PIXA_OK : PIXA_ERR_INVALID_FORMAT;
}

static int apply_diff_bgra(const pixa_asset_t *asset, const uint8_t *payload,
                           size_t payload_len, uint8_t *out_bgra) {
  uint8_t rect_count;
  size_t pos = 1u;

  if (payload_len < 1u) {
    return PIXA_ERR_INVALID_FORMAT;
  }
  rect_count = payload[0];

  for (uint8_t i = 0u; i < rect_count; ++i) {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    uint32_t len;
    int rc;

    if (pos + 12u > payload_len) {
      return PIXA_ERR_INVALID_FORMAT;
    }
    x = read_u16(payload, pos + 0u);
    y = read_u16(payload, pos + 2u);
    w = read_u16(payload, pos + 4u);
    h = read_u16(payload, pos + 6u);
    len = read_u32(payload, pos + 8u);
    pos += 12u;

    if ((size_t)len > payload_len - pos ||
        (size_t)x + (size_t)w > asset->canvas.width ||
        (size_t)y + (size_t)h > asset->canvas.height) {
      return PIXA_ERR_INVALID_FORMAT;
    }

    rc = decode_rle_rect_bgra(asset, payload + pos, len, x, y, w, h, out_bgra);
    if (rc != PIXA_OK) {
      return rc;
    }
    pos += len;
  }

  return pos == payload_len ? PIXA_OK : PIXA_ERR_INVALID_FORMAT;
}

int pixa_apply_clip_frame_bgra(const pixa_asset_t *asset,
                               const pixa_clip_t *clip,
                               uint32_t local_frame_index, uint8_t *out_bgra,
                               size_t out_len) {
  pixa_frame_t frame;
  const uint8_t *payload;
  int rc;

  if (asset == NULL || clip == NULL || out_bgra == NULL) {
    return PIXA_ERR_INVALID_ARG;
  }
  if (out_len < pixa_canvas_bgra_bytes(asset->canvas)) {
    return PIXA_ERR_OUTPUT_TOO_SMALL;
  }
  if (local_frame_index >= clip->frame_count) {
    return PIXA_ERR_FRAME_NOT_FOUND;
  }

  rc = pixa_frame_at(asset, clip->first_frame + local_frame_index, &frame);
  if (rc != PIXA_OK) {
    return rc;
  }
  payload = asset->data + asset->payload_offset + frame.payload_offset;

  if (frame.frame_type == PIXA_FRAME_KEY) {
    memset(out_bgra, 0, pixa_canvas_bgra_bytes(asset->canvas));
    if (frame.encoding == PIXA_KEY_ENCODING_RGB565 ||
        (frame.encoding == PIXA_KEY_ENCODING_LEGACY &&
         frame.payload_len == pixa_canvas_argb4444_bytes(asset->canvas))) {
      return decode_rgb565_frame_bgra(asset, payload, frame.payload_len,
                                      out_bgra);
    }
    if (frame.encoding == PIXA_KEY_ENCODING_PALETTE_RLE ||
        frame.encoding == PIXA_KEY_ENCODING_LEGACY) {
      return decode_rle_rect_bgra(asset, payload, frame.payload_len, 0u, 0u,
                                  asset->canvas.width, asset->canvas.height,
                                  out_bgra);
    }
    return PIXA_ERR_UNSUPPORTED_FRAME;
  }
  if (frame.frame_type == PIXA_FRAME_DIFF) {
    return apply_diff_bgra(asset, payload, frame.payload_len, out_bgra);
  }
  return PIXA_ERR_UNSUPPORTED_FRAME;
}

int pixa_decode_clip_frame_bgra(const pixa_asset_t *asset,
                                const char *clip_name, uint32_t frame_index,
                                uint8_t *out_bgra, size_t out_len) {
  pixa_clip_t clip;
  int rc;

  if (asset == NULL || clip_name == NULL || out_bgra == NULL) {
    return PIXA_ERR_INVALID_ARG;
  }
  rc = pixa_find_clip_or_default(asset, clip_name, &clip);
  if (rc != PIXA_OK) {
    return rc;
  }
  if (clip.frame_count == 0u) {
    return PIXA_ERR_FRAME_NOT_FOUND;
  }
  if (out_len < pixa_canvas_bgra_bytes(asset->canvas)) {
    return PIXA_ERR_OUTPUT_TOO_SMALL;
  }

  memset(out_bgra, 0, pixa_canvas_bgra_bytes(asset->canvas));
  for (uint32_t i = 0u; i <= frame_index % clip.frame_count; ++i) {
    rc = pixa_apply_clip_frame_bgra(asset, &clip, i, out_bgra, out_len);
    if (rc != PIXA_OK) {
      return rc;
    }
  }
  return PIXA_OK;
}

int pixa_bgra_to_argb4444(const uint8_t *in_bgra, size_t in_len,
                          uint8_t *out_argb4444, size_t out_len) {
  size_t pixels;

  if (in_bgra == NULL || out_argb4444 == NULL) {
    return PIXA_ERR_INVALID_ARG;
  }
  pixels = in_len / PIXA_PIXEL_BYTES;
  if (out_len / PIXA_ARGB4444_PIXEL_BYTES < pixels) {
    return PIXA_ERR_OUTPUT_TOO_SMALL;
  }

  for (size_t i = 0u; i < pixels; ++i) {
    const uint8_t *bgra = in_bgra + i * PIXA_PIXEL_BYTES;
    uint16_t value = (uint16_t)(((uint16_t)(bgra[3] >> 4u) << 12) |
                                ((uint16_t)(bgra[2] >> 4u) << 8) |
                                ((uint16_t)(bgra[1] >> 4u) << 4) |
                                (uint16_t)(bgra[0] >> 4u));
    out_argb4444[i * 2u + 0u] = (uint8_t)(value & 0xffu);
    out_argb4444[i * 2u + 1u] = (uint8_t)(value >> 8);
  }
  return PIXA_OK;
}

int pixa_argb4444_to_bgra(const uint8_t *in_argb4444, size_t in_len,
                          uint8_t *out_bgra, size_t out_len) {
  size_t pixels;

  if (in_argb4444 == NULL || out_bgra == NULL) {
    return PIXA_ERR_INVALID_ARG;
  }
  pixels = in_len / PIXA_ARGB4444_PIXEL_BYTES;
  if (out_len / PIXA_PIXEL_BYTES < pixels) {
    return PIXA_ERR_OUTPUT_TOO_SMALL;
  }

  for (size_t i = 0u; i < pixels; ++i) {
    uint16_t value = (uint16_t)in_argb4444[i * 2u] |
                     ((uint16_t)in_argb4444[i * 2u + 1u] << 8);
    uint8_t a4 = (uint8_t)((value >> 12) & 0x0fu);
    uint8_t r4 = (uint8_t)((value >> 8) & 0x0fu);
    uint8_t g4 = (uint8_t)((value >> 4) & 0x0fu);
    uint8_t b4 = (uint8_t)(value & 0x0fu);
    out_bgra[i * 4u + 0u] = (uint8_t)((b4 << 4) | b4);
    out_bgra[i * 4u + 1u] = (uint8_t)((g4 << 4) | g4);
    out_bgra[i * 4u + 2u] = (uint8_t)((r4 << 4) | r4);
    out_bgra[i * 4u + 3u] = (uint8_t)((a4 << 4) | a4);
  }
  return PIXA_OK;
}
