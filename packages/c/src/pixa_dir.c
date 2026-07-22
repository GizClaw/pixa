#include "pixa_dir.h"

#include <string.h>

#define HEADER_CLIP_COUNT 24u
#define HEADER_FRAME_COUNT 28u
#define HEADER_FRAME_BYTES 32u
#define HEADER_CLIP_TABLE 36u
#define HEADER_STRING_TABLE 40u
#define HEADER_DURATION_TABLE 44u

static uint16_t read_u16(const uint8_t *data, size_t offset) {
  return (uint16_t)data[offset] | ((uint16_t)data[offset + 1u] << 8u);
}

static uint32_t read_u32(const uint8_t *data, size_t offset) {
  return (uint32_t)data[offset] | ((uint32_t)data[offset + 1u] << 8u) |
         ((uint32_t)data[offset + 2u] << 16u) |
         ((uint32_t)data[offset + 3u] << 24u);
}

static int16_t read_i16(const uint8_t *data, size_t offset) {
  return (int16_t)read_u16(data, offset);
}

static void write_u16(uint8_t *data, size_t offset, uint16_t value) {
  data[offset] = (uint8_t)value;
  data[offset + 1u] = (uint8_t)(value >> 8u);
}

static void write_u32(uint8_t *data, size_t offset, uint32_t value) {
  data[offset] = (uint8_t)value;
  data[offset + 1u] = (uint8_t)(value >> 8u);
  data[offset + 2u] = (uint8_t)(value >> 16u);
  data[offset + 3u] = (uint8_t)(value >> 24u);
}

static int valid_range(size_t len, uint32_t offset, uint64_t bytes) {
  return offset <= len && bytes <= len - offset;
}

static int valid_name(const char *name, size_t len) {
  if (name == NULL || len == 0u || len > PIXA_MAX_CLIP_NAME)
    return 0;
  for (size_t i = 0u; i < len; ++i) {
    char ch = name[i];
    if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
          (ch >= '0' && ch <= '9') || ch == '_' || ch == '-'))
      return 0;
  }
  return 1;
}

static int decode_clip(const pixa_dir_asset_t *asset, uint32_t index,
                       pixa_dir_clip_t *out_clip) {
  size_t base;
  uint32_t name_offset, durations_offset, duration_count;
  uint16_t name_len, flags;
  if (out_clip != NULL) {
    memset(out_clip, 0, sizeof(*out_clip));
  }
  if (asset == NULL || out_clip == NULL) {
    return PIXA_ERR_INVALID_ARG;
  }
  if (index >= asset->clip_count) {
    return PIXA_ERR_CLIP_NOT_FOUND;
  }
  base = asset->clip_table_offset + (size_t)index * PIXA_DIR_CLIP_ENTRY_SIZE;
  name_offset = read_u32(asset->data, base);
  name_len = read_u16(asset->data, base + 4u);
  flags = read_u16(asset->data, base + 6u);
  durations_offset = read_u32(asset->data, base + 20u);
  duration_count = read_u32(asset->data, base + 24u);
  if ((flags & ~1u) != 0u || read_u32(asset->data, base + 28u) != 0u ||
      duration_count != read_u32(asset->data, base + 12u) ||
      name_offset < asset->string_table_offset ||
      durations_offset < asset->duration_table_offset ||
      !valid_range(asset->len, name_offset, name_len) ||
      !valid_range(asset->len, durations_offset, (uint64_t)duration_count * 2u))
    return PIXA_ERR_INVALID_FORMAT;
  out_clip->name = (const char *)asset->data + name_offset;
  out_clip->name_len = name_len;
  out_clip->anchor.x = read_i16(asset->data, base + 8u);
  out_clip->anchor.y = read_i16(asset->data, base + 10u);
  out_clip->frame_count = duration_count;
  out_clip->total_duration_ms = read_u32(asset->data, base + 16u);
  out_clip->loop = (flags & 1u) != 0u;
  out_clip->frame_bytes = asset->frame_bytes;
  out_clip->durations_le = asset->data + durations_offset;
  if (!valid_name(out_clip->name, out_clip->name_len) ||
      out_clip->frame_count == 0u)
    return PIXA_ERR_INVALID_FORMAT;
  return PIXA_OK;
}

int pixa_dir_open_memory(const void *data_arg, size_t len,
                         pixa_dir_asset_t *out_asset) {
  const uint8_t *data = (const uint8_t *)data_arg;
  pixa_dir_asset_t asset;
  uint32_t declared_size, clip_entry_size, total_frames = 0u;
  if (out_asset != NULL) {
    memset(out_asset, 0, sizeof(*out_asset));
  }
  if (data == NULL || out_asset == NULL)
    return PIXA_ERR_INVALID_ARG;
  if (len < PIXA_DIR_HEADER_SIZE || memcmp(data, PIXA_DIR_MAGIC, 4u) != 0)
    return PIXA_ERR_INVALID_FORMAT;
  if (read_u16(data, 4u) != PIXA_DIR_VERSION_MAJOR)
    return PIXA_ERR_UNSUPPORTED_VERSION;
  if (read_u16(data, 6u) > PIXA_DIR_VERSION_MINOR)
    return PIXA_ERR_UNSUPPORTED_VERSION;
  if (read_u16(data, 8u) != PIXA_DIR_HEADER_SIZE ||
      (clip_entry_size = read_u16(data, 10u)) != PIXA_DIR_CLIP_ENTRY_SIZE ||
      read_u16(data, 16u) != PIXA_DIR_PIXEL_FORMAT_ARGB4444 ||
      read_u16(data, 18u) != 0u)
    return PIXA_ERR_INVALID_FORMAT;
  declared_size = read_u32(data, 20u);
  if (declared_size != len || len > UINT32_MAX)
    return PIXA_ERR_INVALID_FORMAT;
  memset(&asset, 0, sizeof(asset));
  asset.data = data;
  asset.len = len;
  asset.canvas.width = read_u16(data, 12u);
  asset.canvas.height = read_u16(data, 14u);
  asset.clip_count = read_u32(data, HEADER_CLIP_COUNT);
  asset.frame_count = read_u32(data, HEADER_FRAME_COUNT);
  asset.frame_bytes = read_u32(data, HEADER_FRAME_BYTES);
  asset.clip_table_offset = read_u32(data, HEADER_CLIP_TABLE);
  asset.string_table_offset = read_u32(data, HEADER_STRING_TABLE);
  asset.duration_table_offset = read_u32(data, HEADER_DURATION_TABLE);
  uint64_t expected_frame_bytes = (uint64_t)asset.canvas.width *
                                  asset.canvas.height *
                                  PIXA_ARGB4444_PIXEL_BYTES;
  uint64_t expected_string_table = (uint64_t)asset.clip_table_offset +
                                   (uint64_t)asset.clip_count * clip_entry_size;
  if (asset.canvas.width == 0u || asset.canvas.height == 0u ||
      asset.clip_count == 0u || expected_frame_bytes > UINT32_MAX ||
      asset.frame_bytes != expected_frame_bytes ||
      asset.clip_table_offset != PIXA_DIR_HEADER_SIZE ||
      !valid_range(len, asset.clip_table_offset,
                   (uint64_t)asset.clip_count * clip_entry_size) ||
      expected_string_table > UINT32_MAX ||
      asset.string_table_offset != expected_string_table ||
      asset.duration_table_offset < asset.string_table_offset ||
      asset.duration_table_offset > len)
    return PIXA_ERR_INVALID_FORMAT;
  uint64_t expected_name_offset = asset.string_table_offset;
  uint64_t expected_duration_offset = asset.duration_table_offset;
  for (uint32_t i = 0u; i < asset.clip_count; ++i) {
    pixa_dir_clip_t clip;
    size_t entry =
        asset.clip_table_offset + (size_t)i * PIXA_DIR_CLIP_ENTRY_SIZE;
    if (decode_clip(&asset, i, &clip) != PIXA_OK)
      return PIXA_ERR_INVALID_FORMAT;
    if (read_u32(data, entry) != expected_name_offset ||
        read_u32(data, entry + 20u) != expected_duration_offset)
      return PIXA_ERR_INVALID_FORMAT;
    expected_name_offset += clip.name_len;
    expected_duration_offset += (uint64_t)clip.frame_count * 2u;
    if (expected_name_offset > asset.duration_table_offset ||
        expected_duration_offset > len)
      return PIXA_ERR_INVALID_FORMAT;
    if (clip.frame_count > UINT32_MAX - total_frames)
      return PIXA_ERR_INVALID_FORMAT;
    total_frames += clip.frame_count;
    uint64_t duration = 0u;
    for (uint32_t f = 0u; f < clip.frame_count; ++f) {
      uint16_t value = read_u16(clip.durations_le, (size_t)f * 2u);
      if (value == 0u)
        return PIXA_ERR_INVALID_FORMAT;
      duration += value;
    }
    if (duration != clip.total_duration_ms || duration > UINT32_MAX)
      return PIXA_ERR_INVALID_FORMAT;
    for (uint32_t previous = 0u; previous < i; ++previous) {
      pixa_dir_clip_t other;
      if (decode_clip(&asset, previous, &other) != PIXA_OK)
        return PIXA_ERR_INVALID_FORMAT;
      if (other.name_len == clip.name_len &&
          memcmp(other.name, clip.name, clip.name_len) == 0)
        return PIXA_ERR_INVALID_FORMAT;
    }
  }
  if (expected_name_offset != asset.duration_table_offset ||
      expected_duration_offset != len || total_frames != asset.frame_count ||
      asset.duration_table_offset + (uint64_t)asset.frame_count * 2u != len)
    return PIXA_ERR_INVALID_FORMAT;
  *out_asset = asset;
  return PIXA_OK;
}

int pixa_dir_clip_at(const pixa_dir_asset_t *asset, uint32_t index,
                     pixa_dir_clip_t *out_clip) {
  return decode_clip(asset, index, out_clip);
}

int pixa_dir_find_clip(const pixa_dir_asset_t *asset, const char *name,
                       pixa_dir_clip_t *out_clip) {
  if (out_clip != NULL) {
    memset(out_clip, 0, sizeof(*out_clip));
  }
  if (asset == NULL || name == NULL || out_clip == NULL)
    return PIXA_ERR_INVALID_ARG;
  size_t len = strlen(name);
  if (!valid_name(name, len))
    return PIXA_ERR_INVALID_CLIP_NAME;
  for (uint32_t i = 0u; i < asset->clip_count; ++i) {
    pixa_dir_clip_t clip;
    int rc = decode_clip(asset, i, &clip);
    if (rc != PIXA_OK)
      return rc;
    if (clip.name_len == len && memcmp(clip.name, name, len) == 0) {
      *out_clip = clip;
      return PIXA_OK;
    }
  }
  return PIXA_ERR_CLIP_NOT_FOUND;
}

int pixa_dir_clip_duration_at(const pixa_dir_clip_t *clip, uint32_t index,
                              uint16_t *out_duration_ms) {
  if (out_duration_ms != NULL)
    *out_duration_ms = 0u;
  if (clip == NULL || out_duration_ms == NULL)
    return PIXA_ERR_INVALID_ARG;
  if (clip->durations_le == NULL)
    return PIXA_ERR_INVALID_FORMAT;
  if (index >= clip->frame_count)
    return PIXA_ERR_FRAME_NOT_FOUND;
  *out_duration_ms = read_u16(clip->durations_le, (size_t)index * 2u);
  return PIXA_OK;
}

int pixa_dir_frame_index_at_ms(const pixa_dir_clip_t *clip, uint64_t time_ms,
                               uint32_t *out_index) {
  if (out_index != NULL)
    *out_index = 0u;
  if (clip == NULL || out_index == NULL)
    return PIXA_ERR_INVALID_ARG;
  if (clip->durations_le == NULL || clip->frame_count == 0u ||
      clip->total_duration_ms == 0u)
    return PIXA_ERR_INVALID_FORMAT;
  uint64_t cursor = clip->loop ? time_ms % clip->total_duration_ms
                               : (time_ms >= clip->total_duration_ms
                                      ? clip->total_duration_ms - 1u
                                      : time_ms);
  for (uint32_t i = 0u; i < clip->frame_count; ++i) {
    uint16_t duration = read_u16(clip->durations_le, (size_t)i * 2u);
    if (cursor < duration) {
      *out_index = i;
      return PIXA_OK;
    }
    cursor -= duration;
  }
  return PIXA_ERR_INVALID_FORMAT;
}

int pixa_dir_encode(const pixa_canvas_t *canvas, const pixa_dir_clip_t *clips,
                    uint32_t clip_count, void *out_arg, size_t capacity,
                    size_t *out_len) {
  uint8_t *out = (uint8_t *)out_arg;
  uint64_t names_size = 0u, frame_count = 0u;
  if (out_len != NULL)
    *out_len = 0u;
  if (canvas == NULL || clips == NULL || clip_count == 0u || out_len == NULL)
    return PIXA_ERR_INVALID_ARG;
  uint64_t fb =
      (uint64_t)canvas->width * canvas->height * PIXA_ARGB4444_PIXEL_BYTES;
  if (canvas->width == 0u || canvas->height == 0u || fb > UINT32_MAX)
    return PIXA_ERR_INVALID_FORMAT;
  for (uint32_t i = 0u; i < clip_count; ++i) {
    if (!valid_name(clips[i].name, clips[i].name_len) ||
        clips[i].frame_count == 0u || clips[i].durations_le == NULL ||
        clips[i].frame_bytes != fb ||
        (clips[i].loop != 0 && clips[i].loop != 1))
      return PIXA_ERR_INVALID_FORMAT;
    names_size += clips[i].name_len;
    frame_count += clips[i].frame_count;
  }
  if (frame_count > UINT32_MAX)
    return PIXA_ERR_INVALID_FORMAT;
  uint64_t clip_table = PIXA_DIR_HEADER_SIZE;
  uint64_t string_table =
      clip_table + (uint64_t)clip_count * PIXA_DIR_CLIP_ENTRY_SIZE;
  uint64_t duration_table = string_table + names_size;
  uint64_t required = duration_table + frame_count * 2u;
  if (required > UINT32_MAX)
    return PIXA_ERR_OUTPUT_TOO_SMALL;
  *out_len = (size_t)required;
  if (out == NULL || capacity < required)
    return PIXA_ERR_OUTPUT_TOO_SMALL;
  memset(out, 0, (size_t)required);
  memcpy(out, PIXA_DIR_MAGIC, 4u);
  write_u16(out, 4u, PIXA_DIR_VERSION_MAJOR);
  write_u16(out, 6u, PIXA_DIR_VERSION_MINOR);
  write_u16(out, 8u, PIXA_DIR_HEADER_SIZE);
  write_u16(out, 10u, PIXA_DIR_CLIP_ENTRY_SIZE);
  write_u16(out, 12u, canvas->width);
  write_u16(out, 14u, canvas->height);
  write_u16(out, 16u, PIXA_DIR_PIXEL_FORMAT_ARGB4444);
  write_u32(out, 20u, (uint32_t)required);
  write_u32(out, HEADER_CLIP_COUNT, clip_count);
  write_u32(out, HEADER_FRAME_COUNT, (uint32_t)frame_count);
  write_u32(out, HEADER_FRAME_BYTES, (uint32_t)fb);
  write_u32(out, HEADER_CLIP_TABLE, (uint32_t)clip_table);
  write_u32(out, HEADER_STRING_TABLE, (uint32_t)string_table);
  write_u32(out, HEADER_DURATION_TABLE, (uint32_t)duration_table);
  size_t name_cursor = (size_t)string_table,
         duration_cursor = (size_t)duration_table;
  for (uint32_t i = 0u; i < clip_count; ++i) {
    size_t base = (size_t)clip_table + (size_t)i * PIXA_DIR_CLIP_ENTRY_SIZE;
    uint64_t total = 0u;
    for (uint32_t f = 0u; f < clips[i].frame_count; ++f) {
      uint16_t d = read_u16(clips[i].durations_le, (size_t)f * 2u);
      if (d == 0u)
        return PIXA_ERR_INVALID_FORMAT;
      total += d;
    }
    if (total > UINT32_MAX || (clips[i].total_duration_ms != 0u &&
                               clips[i].total_duration_ms != total))
      return PIXA_ERR_INVALID_FORMAT;
    write_u32(out, base, (uint32_t)name_cursor);
    write_u16(out, base + 4u, (uint16_t)clips[i].name_len);
    write_u16(out, base + 6u, clips[i].loop ? 1u : 0u);
    write_u16(out, base + 8u, (uint16_t)clips[i].anchor.x);
    write_u16(out, base + 10u, (uint16_t)clips[i].anchor.y);
    write_u32(out, base + 12u, clips[i].frame_count);
    write_u32(out, base + 16u, (uint32_t)total);
    write_u32(out, base + 20u, (uint32_t)duration_cursor);
    write_u32(out, base + 24u, clips[i].frame_count);
    memcpy(out + name_cursor, clips[i].name, clips[i].name_len);
    name_cursor += clips[i].name_len;
    memcpy(out + duration_cursor, clips[i].durations_le,
           (size_t)clips[i].frame_count * 2u);
    duration_cursor += (size_t)clips[i].frame_count * 2u;
  }
  pixa_dir_asset_t verify;
  return pixa_dir_open_memory(out, (size_t)required, &verify);
}
