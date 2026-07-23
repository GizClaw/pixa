#include "pixa_dir.h"

#include <assert.h>
#include <string.h>

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

int main(void) {
  const uint8_t durations[] = {40u, 0u, 80u, 0u};
  const pixa_canvas_t canvas = {240u, 240u};
  const pixa_dir_clip_t source = {
      .name = "run_left",
      .name_len = 8u,
      .anchor = {-2, 7},
      .loop = 1,
      .total_duration_ms = 120u,
      .frame_count = 2u,
      .frame_bytes = 115200u,
      .durations_le = durations,
  };
  uint8_t index[128];
  size_t index_len = 0u;
  assert(pixa_dir_encode(&canvas, &source, 1u, index, sizeof(index),
                         &index_len) == PIXA_OK);
  assert(index_len == 92u);

  pixa_dir_asset_t asset;
  assert(pixa_dir_open_memory(index, index_len, &asset) == PIXA_OK);
  assert(asset.canvas.width == 240u && asset.frame_count == 2u);
  pixa_dir_clip_t clip;
  assert(pixa_dir_find_clip(&asset, "run_left", &clip) == PIXA_OK);
  assert(clip.name_len == 8u && clip.anchor.x == -2 && clip.anchor.y == 7);
  uint16_t duration = 0u;
  assert(pixa_dir_clip_duration_at(&clip, 1u, &duration) == PIXA_OK);
  assert(duration == 80u);
  uint32_t frame = 99u;
  assert(pixa_dir_frame_index_at_ms(&clip, 39u, &frame) == PIXA_OK &&
         frame == 0u);
  assert(pixa_dir_frame_index_at_ms(&clip, 40u, &frame) == PIXA_OK &&
         frame == 1u);
  assert(pixa_dir_frame_index_at_ms(&clip, 120u, &frame) == PIXA_OK &&
         frame == 0u);

  uint8_t bad[128];
  memcpy(bad, index, index_len);
  bad[0] = 'X';
  assert(pixa_dir_open_memory(bad, index_len, &asset) ==
         PIXA_ERR_INVALID_FORMAT);
  memcpy(bad, index, index_len);
  write_u16(bad, 4u, 2u);
  assert(pixa_dir_open_memory(bad, index_len, &asset) ==
         PIXA_ERR_UNSUPPORTED_VERSION);
  assert(pixa_dir_open_memory(index, PIXA_DIR_HEADER_SIZE - 1u, &asset) ==
         PIXA_ERR_INVALID_FORMAT);
  memcpy(bad, index, index_len);
  write_u16(bad, 10u, 28u);
  assert(pixa_dir_open_memory(bad, index_len, &asset) ==
         PIXA_ERR_INVALID_FORMAT);
  memcpy(bad, index, index_len);
  write_u32(bad, 48u, UINT32_MAX);
  assert(pixa_dir_open_memory(bad, index_len, &asset) ==
         PIXA_ERR_INVALID_FORMAT);
  memcpy(bad, index, index_len);
  bad[80] = '/';
  assert(pixa_dir_open_memory(bad, index_len, &asset) ==
         PIXA_ERR_INVALID_FORMAT);
  memcpy(bad, index, index_len);
  write_u16(bad, 54u, 2u);
  assert(pixa_dir_open_memory(bad, index_len, &asset) ==
         PIXA_ERR_INVALID_FORMAT);
  memcpy(bad, index, index_len);
  bad[index_len - 2u] = 0u;
  assert(pixa_dir_open_memory(bad, index_len, &asset) ==
         PIXA_ERR_INVALID_FORMAT);
  assert(pixa_dir_open_memory(index, index_len - 1u, &asset) ==
         PIXA_ERR_INVALID_FORMAT);
  memcpy(bad, index, index_len);
  write_u16(bad, 12u, UINT16_MAX);
  write_u16(bad, 14u, UINT16_MAX);
  write_u32(bad, 32u, UINT32_MAX);
  assert(pixa_dir_open_memory(bad, index_len, &asset) ==
         PIXA_ERR_INVALID_FORMAT);

  const uint8_t idle_duration[] = {20u, 0u};
  const pixa_dir_clip_t multi_source[] = {
      source,
      {
          .name = "idle",
          .name_len = 4u,
          .anchor = {1, -1},
          .loop = 0,
          .total_duration_ms = 20u,
          .frame_count = 1u,
          .frame_bytes = 115200u,
          .durations_le = idle_duration,
      },
  };
  uint8_t multi_index[256];
  size_t multi_len = 0u;
  assert(pixa_dir_encode(&canvas, multi_source, 2u, multi_index,
                         sizeof(multi_index), &multi_len) == PIXA_OK);
  assert(pixa_dir_open_memory(multi_index, multi_len, &asset) == PIXA_OK);
  assert(asset.clip_count == 2u && asset.frame_count == 3u);
  assert(pixa_dir_find_clip(&asset, "idle", &clip) == PIXA_OK);
  assert(clip.frame_count == 1u && clip.anchor.x == 1 && clip.anchor.y == -1);

  pixa_dir_clip_t duplicate[] = {source, source};
  uint8_t duplicate_index[256];
  size_t duplicate_len = 0u;
  assert(pixa_dir_encode(&canvas, duplicate, 2u, duplicate_index,
                         sizeof(duplicate_index),
                         &duplicate_len) == PIXA_ERR_INVALID_FORMAT);
  return 0;
}
