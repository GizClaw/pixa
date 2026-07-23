#include "pixa_decode.h"
#include "pixa_reader.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#define HEADER_SIZE 40u
#define CLIP_SIZE 56u
#define FRAME_SIZE 16u

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

static size_t build_asset(uint8_t *data, size_t capacity,
                          const uint16_t *palette, uint16_t color_count,
                          const uint8_t *payload, size_t payload_len) {
  const size_t palette_offset = HEADER_SIZE;
  const size_t clip_offset = palette_offset + (size_t)color_count * 2u;
  const size_t frame_offset = clip_offset + CLIP_SIZE;
  const size_t payload_offset = frame_offset + FRAME_SIZE;
  const size_t total = payload_offset + payload_len;
  assert(total <= capacity);
  memset(data, 0, total);
  memcpy(data, PIXA_MAGIC, 4u);
  write_u16(data, 4u, PIXA_VERSION);
  write_u16(data, 6u, HEADER_SIZE);
  write_u16(data, 8u, 2u);
  write_u16(data, 10u, 1u);
  write_u16(data, 12u, color_count);
  write_u16(data, 14u, 1u);
  write_u32(data, 16u, 1u);
  write_u32(data, 20u, (uint32_t)palette_offset);
  write_u32(data, 24u, (uint32_t)clip_offset);
  write_u32(data, 28u, (uint32_t)frame_offset);
  write_u32(data, 32u, (uint32_t)payload_offset);
  write_u32(data, 36u, (uint32_t)payload_len);
  for (uint16_t index = 0u; index < color_count; ++index) {
    write_u16(data, palette_offset + (size_t)index * 2u, palette[index]);
  }
  memcpy(data + clip_offset, "idle", 4u);
  write_u32(data, clip_offset + 36u, 0u);
  write_u32(data, clip_offset + 40u, 1u);
  write_u32(data, clip_offset + 44u, 100u);
  write_u16(data, clip_offset + 48u, 1u);
  write_u16(data, frame_offset, 100u);
  data[frame_offset + 2u] = 0u;
  write_u32(data, frame_offset + 4u, 0u);
  write_u32(data, frame_offset + 8u, (uint32_t)payload_len);
  memcpy(data + payload_offset, payload, payload_len);
  return total;
}

static void test_gizclaw_rgb565_key_frame(void) {
  uint8_t data[128];
  const uint16_t palette[] = {0u};
  const uint8_t payload[] = {0x00u, 0xf8u, 0x1fu, 0x00u};
  const size_t len =
      build_asset(data, sizeof(data), palette, 1u, payload, sizeof(payload));
  pixa_asset_t asset;
  uint8_t bgra[8];
  assert(pixa_open_memory(data, len, &asset) == PIXA_OK);
  assert(pixa_decode_clip_frame_bgra(&asset, "idle", 0u, bgra, sizeof(bgra)) ==
         PIXA_OK);
  const uint8_t expected[] = {0u, 0u, 255u, 255u, 255u, 0u, 0u, 255u};
  assert(memcmp(bgra, expected, sizeof(expected)) == 0);
}

static void test_palette_rle_key_frame(void) {
  uint8_t data[128];
  const uint16_t palette[] = {0u, 0xf800u};
  const uint8_t payload[] = {2u, 1u};
  const size_t len =
      build_asset(data, sizeof(data), palette, 2u, payload, sizeof(payload));
  pixa_asset_t asset;
  uint8_t bgra[8];
  assert(pixa_open_memory(data, len, &asset) == PIXA_OK);
  assert(pixa_decode_clip_frame_bgra(&asset, "idle", 0u, bgra, sizeof(bgra)) ==
         PIXA_OK);
  const uint8_t expected[] = {0u, 0u, 255u, 255u, 0u, 0u, 255u, 255u};
  assert(memcmp(bgra, expected, sizeof(expected)) == 0);
}

static void test_rgb565_key_frame_with_palette(void) {
  uint8_t data[128];
  const uint16_t palette[] = {0u, 0xf800u};
  const uint8_t payload[] = {0x00u, 0xf8u, 0x1fu, 0x00u};
  const size_t len =
      build_asset(data, sizeof(data), palette, 2u, payload, sizeof(payload));
  pixa_asset_t asset;
  uint8_t bgra[8];
  assert(pixa_open_memory(data, len, &asset) == PIXA_OK);
  assert(pixa_decode_clip_frame_bgra(&asset, "idle", 0u, bgra, sizeof(bgra)) ==
         PIXA_OK);
  const uint8_t expected[] = {0u, 0u, 255u, 255u, 255u, 0u, 0u, 255u};
  assert(memcmp(bgra, expected, sizeof(expected)) == 0);
}

int main(void) {
  test_gizclaw_rgb565_key_frame();
  test_palette_rle_key_frame();
  test_rgb565_key_frame_with_palette();
  return 0;
}
