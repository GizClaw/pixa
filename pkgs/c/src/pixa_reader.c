#include "pixa_reader.h"

#include <string.h>

#define PIXA_HEADER_SIZE 40u
#define PIXA_CLIP_ENTRY_SIZE 56u
#define PIXA_FRAME_ENTRY_SIZE 16u

static uint16_t read_u16(const uint8_t *data, size_t offset) {
  return (uint16_t)data[offset] | ((uint16_t)data[offset + 1u] << 8);
}

static int16_t read_i16(const uint8_t *data, size_t offset) {
  return (int16_t)read_u16(data, offset);
}

static uint32_t read_u32(const uint8_t *data, size_t offset) {
  return (uint32_t)data[offset] | ((uint32_t)data[offset + 1u] << 8) |
         ((uint32_t)data[offset + 2u] << 16) |
         ((uint32_t)data[offset + 3u] << 24);
}

static int require_range(size_t data_len, uint32_t offset, size_t len) {
  return (size_t)offset <= data_len && len <= data_len - (size_t)offset;
}

int pixa_open_memory(const void *data, size_t len, pixa_asset_t *out_asset) {
  const uint8_t *bytes = (const uint8_t *)data;
  pixa_asset_t asset;

  if (data == NULL || out_asset == NULL) {
    return PIXA_ERR_INVALID_ARG;
  }
  if (len < PIXA_HEADER_SIZE || memcmp(bytes, PIXA_MAGIC, 4u) != 0) {
    return PIXA_ERR_INVALID_FORMAT;
  }
  if (read_u16(bytes, 4u) != PIXA_VERSION) {
    return PIXA_ERR_UNSUPPORTED_VERSION;
  }
  if (read_u16(bytes, 6u) != PIXA_HEADER_SIZE) {
    return PIXA_ERR_INVALID_FORMAT;
  }

  asset.data = bytes;
  asset.len = len;
  asset.canvas.width = read_u16(bytes, 8u);
  asset.canvas.height = read_u16(bytes, 10u);
  asset.color_count = read_u16(bytes, 12u);
  asset.clip_count = read_u16(bytes, 14u);
  asset.frame_count = read_u32(bytes, 16u);
  asset.palette_offset = read_u32(bytes, 20u);
  asset.clip_offset = read_u32(bytes, 24u);
  asset.frame_offset = read_u32(bytes, 28u);
  asset.payload_offset = read_u32(bytes, 32u);
  asset.payload_len = read_u32(bytes, 36u);

  if (asset.canvas.width == 0u || asset.canvas.height == 0u) {
    return PIXA_ERR_INVALID_FORMAT;
  }
  if ((size_t)asset.canvas.width > SIZE_MAX / (size_t)asset.canvas.height ||
      (size_t)asset.canvas.width * (size_t)asset.canvas.height >
          SIZE_MAX / PIXA_PIXEL_BYTES) {
    return PIXA_ERR_INVALID_FORMAT;
  }
  if (asset.clip_count == 0u) {
    return PIXA_ERR_INVALID_FORMAT;
  }
  if (!require_range(len, asset.palette_offset,
                     (size_t)asset.color_count * 2u) ||
      !require_range(len, asset.clip_offset,
                     (size_t)asset.clip_count * PIXA_CLIP_ENTRY_SIZE) ||
      !require_range(len, asset.frame_offset,
                     (size_t)asset.frame_count * PIXA_FRAME_ENTRY_SIZE) ||
      !require_range(len, asset.payload_offset, asset.payload_len)) {
    return PIXA_ERR_INVALID_FORMAT;
  }
  for (uint16_t i = 0u; i < asset.clip_count; ++i) {
    pixa_clip_t clip;
    if (pixa_clip_at(&asset, i, &clip) != PIXA_OK)
      return PIXA_ERR_INVALID_FORMAT;
  }
  for (uint32_t i = 0u; i < asset.frame_count; ++i) {
    pixa_frame_t frame;
    if (pixa_frame_at(&asset, i, &frame) != PIXA_OK)
      return PIXA_ERR_INVALID_FORMAT;
  }

  *out_asset = asset;
  return PIXA_OK;
}

int pixa_clip_at(const pixa_asset_t *asset, uint16_t index,
                 pixa_clip_t *out_clip) {
  size_t base;
  const char *raw_name;
  size_t name_len = 0u;
  uint32_t first_frame;
  uint32_t frame_count;

  if (asset == NULL || out_clip == NULL) {
    return PIXA_ERR_INVALID_ARG;
  }
  if (index >= asset->clip_count) {
    return PIXA_ERR_CLIP_NOT_FOUND;
  }

  base = (size_t)asset->clip_offset + (size_t)index * PIXA_CLIP_ENTRY_SIZE;
  raw_name = (const char *)(asset->data + base);
  while (name_len < PIXA_MAX_CLIP_NAME && raw_name[name_len] != '\0') {
    ++name_len;
  }
  if (name_len == PIXA_MAX_CLIP_NAME) {
    return PIXA_ERR_INVALID_FORMAT;
  }
  first_frame = read_u32(asset->data, base + 36u);
  frame_count = read_u32(asset->data, base + 40u);
  if (first_frame > asset->frame_count ||
      frame_count > asset->frame_count - first_frame) {
    return PIXA_ERR_INVALID_FORMAT;
  }

  out_clip->name = raw_name;
  out_clip->name_len = name_len;
  out_clip->anchor.x = read_i16(asset->data, base + 32u);
  out_clip->anchor.y = read_i16(asset->data, base + 34u);
  out_clip->first_frame = first_frame;
  out_clip->frame_count = frame_count;
  out_clip->total_duration_ms = read_u32(asset->data, base + 44u);
  out_clip->loop = (read_u16(asset->data, base + 48u) & 1u) != 0u;
  return PIXA_OK;
}

int pixa_find_clip(const pixa_asset_t *asset, const char *name,
                   pixa_clip_t *out_clip) {
  size_t name_len;

  if (asset == NULL || name == NULL || out_clip == NULL) {
    return PIXA_ERR_INVALID_ARG;
  }
  name_len = strlen(name);

  for (uint16_t i = 0u; i < asset->clip_count; ++i) {
    pixa_clip_t clip;
    int rc = pixa_clip_at(asset, i, &clip);
    if (rc != PIXA_OK) {
      return rc;
    }
    if (clip.name_len == name_len && memcmp(clip.name, name, name_len) == 0) {
      *out_clip = clip;
      return PIXA_OK;
    }
  }
  return PIXA_ERR_CLIP_NOT_FOUND;
}

int pixa_find_clip_or_default(const pixa_asset_t *asset, const char *name,
                              pixa_clip_t *out_clip) {
  int rc = pixa_find_clip(asset, name, out_clip);
  if (rc != PIXA_ERR_CLIP_NOT_FOUND) {
    return rc;
  }
  return pixa_find_clip(asset, "default", out_clip);
}

int pixa_frame_at(const pixa_asset_t *asset, uint32_t index,
                  pixa_frame_t *out_frame) {
  size_t base;
  uint32_t payload_offset;
  uint32_t payload_len;

  if (asset == NULL || out_frame == NULL) {
    return PIXA_ERR_INVALID_ARG;
  }
  if (index >= asset->frame_count) {
    return PIXA_ERR_FRAME_NOT_FOUND;
  }

  base = (size_t)asset->frame_offset + (size_t)index * PIXA_FRAME_ENTRY_SIZE;
  payload_offset = read_u32(asset->data, base + 4u);
  payload_len = read_u32(asset->data, base + 8u);
  if (payload_offset > asset->payload_len ||
      payload_len > asset->payload_len - payload_offset) {
    return PIXA_ERR_INVALID_FORMAT;
  }

  out_frame->duration_ms = read_u16(asset->data, base + 0u);
  out_frame->frame_type = asset->data[base + 2u];
  out_frame->encoding = asset->data[base + 3u];
  out_frame->payload_offset = payload_offset;
  out_frame->payload_len = payload_len;
  return PIXA_OK;
}

int pixa_frame_index_at_ms(const pixa_asset_t *asset, const pixa_clip_t *clip,
                           uint64_t time_ms, uint32_t *out_index) {
  uint64_t cursor;
  uint64_t total;

  if (asset == NULL || clip == NULL || out_index == NULL) {
    return PIXA_ERR_INVALID_ARG;
  }
  if (clip->frame_count == 0u) {
    *out_index = 0u;
    return PIXA_OK;
  }

  total = clip->total_duration_ms == 0u ? 1u : clip->total_duration_ms;
  cursor = clip->loop ? (time_ms % total)
                      : (time_ms >= total ? total - 1u : time_ms);
  for (uint32_t i = 0u; i < clip->frame_count; ++i) {
    pixa_frame_t frame;
    uint32_t duration;
    int rc = pixa_frame_at(asset, clip->first_frame + i, &frame);
    if (rc != PIXA_OK) {
      return rc;
    }
    duration = frame.duration_ms == 0u ? 1u : frame.duration_ms;
    if (cursor < duration) {
      *out_index = i;
      return PIXA_OK;
    }
    cursor -= duration;
  }

  *out_index = clip->frame_count - 1u;
  return PIXA_OK;
}
