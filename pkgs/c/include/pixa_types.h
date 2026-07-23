#ifndef PIXA_TYPES_H
#define PIXA_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PIXA_MAGIC "PIXA"
#define PIXA_VERSION 1u
#define PIXA_PIXEL_BYTES 4u
#define PIXA_ARGB4444_PIXEL_BYTES 2u
#define PIXA_MAX_CLIP_NAME 32u
#define PIXA_DIR_MAGIC "PXDI"
#define PIXA_DIR_VERSION_MAJOR 1u
#define PIXA_DIR_VERSION_MINOR 0u
#define PIXA_DIR_HEADER_SIZE 48u
#define PIXA_DIR_CLIP_ENTRY_SIZE 32u
#define PIXA_DIR_PIXEL_FORMAT_ARGB4444 1u

typedef enum pixa_result {
  PIXA_OK = 0,
  PIXA_ERR_INVALID_ARG = -1,
  PIXA_ERR_INVALID_FORMAT = -2,
  PIXA_ERR_UNSUPPORTED_VERSION = -3,
  PIXA_ERR_UNSUPPORTED_FRAME = -4,
  PIXA_ERR_CLIP_NOT_FOUND = -5,
  PIXA_ERR_FRAME_NOT_FOUND = -6,
  PIXA_ERR_OUTPUT_TOO_SMALL = -7,
  PIXA_ERR_CANVAS_MISMATCH = -8,
  PIXA_ERR_INVALID_CLIP_NAME = -9,
  PIXA_ERR_NO_MEMORY = -10,
  PIXA_ERR_FS = -11,
} pixa_result_t;

typedef struct pixa_canvas {
  uint16_t width;
  uint16_t height;
} pixa_canvas_t;

typedef struct pixa_anchor {
  int16_t x;
  int16_t y;
} pixa_anchor_t;

typedef struct pixa_clip {
  const char *name;
  size_t name_len;
  pixa_anchor_t anchor;
  uint32_t first_frame;
  uint32_t frame_count;
  uint32_t total_duration_ms;
  int loop;
} pixa_clip_t;

typedef struct pixa_frame {
  uint16_t duration_ms;
  uint8_t frame_type;
  uint32_t payload_offset;
  uint32_t payload_len;
} pixa_frame_t;

typedef struct pixa_asset {
  const uint8_t *data;
  size_t len;
  pixa_canvas_t canvas;
  uint16_t color_count;
  uint16_t clip_count;
  uint32_t frame_count;
  uint32_t palette_offset;
  uint32_t clip_offset;
  uint32_t frame_offset;
  uint32_t payload_offset;
  uint32_t payload_len;
} pixa_asset_t;

typedef struct pixa_alloc {
  void *user;
  void *(*alloc)(void *user, size_t len);
  void (*free)(void *user, void *ptr);
} pixa_alloc_t;

typedef struct pixa_extract_stats {
  size_t file_count;
  size_t frame_count;
  size_t payload_len;
} pixa_extract_stats_t;

typedef struct pixa_dir_asset {
  const uint8_t *data;
  size_t len;
  pixa_canvas_t canvas;
  uint32_t clip_count;
  uint32_t frame_count;
  uint32_t frame_bytes;
  uint32_t clip_table_offset;
  uint32_t string_table_offset;
  uint32_t duration_table_offset;
} pixa_dir_asset_t;

typedef struct pixa_dir_clip {
  const char *name;
  size_t name_len;
  pixa_anchor_t anchor;
  int loop;
  uint32_t total_duration_ms;
  uint32_t frame_count;
  uint32_t frame_bytes;
  const uint8_t *durations_le;
} pixa_dir_clip_t;

size_t pixa_canvas_pixel_count(pixa_canvas_t canvas);
size_t pixa_canvas_bgra_bytes(pixa_canvas_t canvas);
size_t pixa_canvas_argb4444_bytes(pixa_canvas_t canvas);
const char *pixa_result_name(int result);

#ifdef __cplusplus
}
#endif

#endif
