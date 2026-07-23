#include "pixa_pack.h"

#include "pixa_dir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PIXA_PATH_MAX 384u
#define PIXA_HEADER_SIZE 40u
#define PIXA_CLIP_ENTRY_SIZE 56u
#define PIXA_FRAME_ENTRY_SIZE 16u
#define PIXA_MAX_COLORS 256u
#define PIXA_MAX_INDEX_BYTES (4u * 1024u * 1024u)
#define PIXA_FRAME_KEY 0u

typedef struct pixa_vec {
  uint8_t *data;
  size_t len;
  size_t cap;
  pixa_alloc_t alloc;
} pixa_vec_t;

typedef struct pixa_pack_clip_state {
  pixa_dir_clip_t dir_clip;
  char id[PIXA_MAX_CLIP_NAME + 1u];
  uint8_t *durations_le;
  uint32_t first_frame;
  uint8_t *frames;
  size_t frames_len;
} pixa_pack_clip_state_t;

static void *default_alloc(void *user, size_t len) {
  (void)user;
  return malloc(len);
}

static void default_free(void *user, void *ptr) {
  (void)user;
  free(ptr);
}

static pixa_alloc_t normalized_alloc(const pixa_alloc_t *alloc) {
  pixa_alloc_t out;
  if (alloc != NULL && alloc->alloc != NULL && alloc->free != NULL) {
    return *alloc;
  }
  out.user = NULL;
  out.alloc = default_alloc;
  out.free = default_free;
  return out;
}

static int map_fs(int rc) { return rc == PIXA_OSAL_OK ? PIXA_OK : PIXA_ERR_FS; }

static int pixa_fs_path_is_safe_component(const char *name) {
  size_t len;

  if (name == NULL || name[0] == '\0') {
    return 0;
  }
  len = strlen(name);
  if ((len == 1u && name[0] == '.') ||
      (len == 2u && name[0] == '.' && name[1] == '.')) {
    return 0;
  }
  for (size_t i = 0u; i < len; ++i) {
    char ch = name[i];
    if (ch == '/' || ch == '\\' || ch == ':') {
      return 0;
    }
  }
  return 1;
}

static int pixa_fs_path_join(char *out, size_t out_len, const char *parent,
                             const char *child) {
  size_t parent_len;
  size_t child_len;
  int needs_slash;

  if (out == NULL || out_len == 0u || parent == NULL || child == NULL ||
      child[0] == '\0') {
    return PIXA_OSAL_ERR_INVALID_ARG;
  }
  if (!pixa_fs_path_is_safe_component(child)) {
    return PIXA_OSAL_ERR_INVALID_ARG;
  }
  parent_len = strlen(parent);
  child_len = strlen(child);
  needs_slash = parent_len > 0u && parent[parent_len - 1u] != '/';
  if (parent_len + (needs_slash ? 1u : 0u) + child_len + 1u > out_len) {
    return PIXA_OSAL_ERR_NO_SPACE;
  }
  memcpy(out, parent, parent_len);
  if (needs_slash) {
    out[parent_len++] = '/';
  }
  memcpy(out + parent_len, child, child_len);
  out[parent_len + child_len] = '\0';
  return PIXA_OSAL_OK;
}

static int pixa_fs_open(const pixa_osal_api_t *fs, const char *path,
                        pixa_osal_open_mode_t mode,
                        pixa_osal_file_t **out_file) {
  if (fs == NULL || path == NULL || out_file == NULL) {
    return PIXA_OSAL_ERR_INVALID_ARG;
  }
  *out_file = NULL;
  return pixa_osal_open(fs, path, mode, out_file);
}

static int pixa_fs_read(const pixa_osal_api_t *fs, pixa_osal_file_t *file,
                        void *data, size_t len, size_t *out_read) {
  if (fs == NULL || file == NULL || data == NULL || out_read == NULL) {
    return PIXA_OSAL_ERR_INVALID_ARG;
  }
  return pixa_osal_read(fs, file, data, len, out_read);
}

static int pixa_fs_write_all(const pixa_osal_api_t *fs, pixa_osal_file_t *file,
                             const void *data, size_t len) {
  const uint8_t *cursor = (const uint8_t *)data;
  size_t remaining = len;

  if (fs == NULL || file == NULL || (data == NULL && len != 0u)) {
    return PIXA_OSAL_ERR_INVALID_ARG;
  }
  while (remaining > 0u) {
    size_t written = 0u;
    int rc = pixa_osal_write(fs, file, cursor, remaining, &written);
    if (rc != PIXA_OSAL_OK) {
      return rc;
    }
    if (written == 0u || written > remaining) {
      return PIXA_OSAL_ERR_IO;
    }
    cursor += written;
    remaining -= written;
  }
  return PIXA_OSAL_OK;
}

static int pixa_fs_close(const pixa_osal_api_t *fs, pixa_osal_file_t *file) {
  if (fs == NULL || file == NULL) {
    return PIXA_OSAL_ERR_INVALID_ARG;
  }
  return pixa_osal_close(fs, file);
}

static int pixa_fs_write_file(const pixa_osal_api_t *fs, const char *path,
                              const void *data, size_t len) {
  pixa_osal_file_t *file = NULL;
  int rc = pixa_fs_open(fs, path, PIXA_OSAL_OPEN_WRITE_TRUNCATE, &file);
  int close_rc;

  if (rc != PIXA_OSAL_OK) {
    return rc;
  }
  rc = pixa_fs_write_all(fs, file, data, len);
  if (rc == PIXA_OSAL_OK) {
    rc = pixa_osal_sync(fs, file);
  }
  close_rc = pixa_fs_close(fs, file);
  return rc == PIXA_OSAL_OK ? close_rc : rc;
}

static int vec_reserve(pixa_vec_t *vec, size_t extra) {
  uint8_t *next;
  size_t next_cap;

  if (extra > SIZE_MAX - vec->len) {
    return PIXA_ERR_NO_MEMORY;
  }
  if (vec->len + extra <= vec->cap) {
    return PIXA_OK;
  }

  next_cap = vec->cap == 0u ? 256u : vec->cap;
  while (next_cap < vec->len + extra) {
    if (next_cap > SIZE_MAX / 2u) {
      next_cap = vec->len + extra;
      break;
    }
    next_cap *= 2u;
  }

  next = (uint8_t *)vec->alloc.alloc(vec->alloc.user, next_cap);
  if (next == NULL) {
    return PIXA_ERR_NO_MEMORY;
  }
  if (vec->data != NULL && vec->len > 0u) {
    memcpy(next, vec->data, vec->len);
  }
  if (vec->data != NULL) {
    vec->alloc.free(vec->alloc.user, vec->data);
  }
  vec->data = next;
  vec->cap = next_cap;
  return PIXA_OK;
}

static int vec_append(pixa_vec_t *vec, const void *data, size_t len) {
  int rc;
  if (data == NULL && len != 0u) {
    return PIXA_ERR_INVALID_ARG;
  }
  rc = vec_reserve(vec, len);
  if (rc != PIXA_OK) {
    return rc;
  }
  if (len > 0u) {
    memcpy(vec->data + vec->len, data, len);
    vec->len += len;
  }
  return PIXA_OK;
}

static int vec_append_zero(pixa_vec_t *vec, size_t len) {
  int rc = vec_reserve(vec, len);
  if (rc != PIXA_OK) {
    return rc;
  }
  memset(vec->data + vec->len, 0, len);
  vec->len += len;
  return PIXA_OK;
}

static void vec_free(pixa_vec_t *vec) {
  if (vec != NULL && vec->data != NULL) {
    vec->alloc.free(vec->alloc.user, vec->data);
  }
  if (vec != NULL) {
    vec->data = NULL;
    vec->len = 0u;
    vec->cap = 0u;
  }
}

static void write_u16(uint8_t *data, size_t offset, uint16_t value) {
  data[offset + 0u] = (uint8_t)(value & 0xffu);
  data[offset + 1u] = (uint8_t)(value >> 8);
}

static void write_i16(uint8_t *data, size_t offset, int16_t value) {
  write_u16(data, offset, (uint16_t)value);
}

static void write_u32(uint8_t *data, size_t offset, uint32_t value) {
  data[offset + 0u] = (uint8_t)(value & 0xffu);
  data[offset + 1u] = (uint8_t)((value >> 8) & 0xffu);
  data[offset + 2u] = (uint8_t)((value >> 16) & 0xffu);
  data[offset + 3u] = (uint8_t)((value >> 24) & 0xffu);
}

static int read_file_alloc(const pixa_osal_api_t *fs, const char *path,
                           pixa_alloc_t alloc, size_t max_len,
                           uint8_t **out_data, size_t *out_len) {
  pixa_osal_file_t *file = NULL;
  pixa_vec_t vec = {0};
  uint8_t chunk[128];
  int rc;

  if (fs == NULL || path == NULL || out_data == NULL || out_len == NULL) {
    return PIXA_ERR_INVALID_ARG;
  }
  *out_data = NULL;
  *out_len = 0u;
  vec.alloc = alloc;

  rc = pixa_fs_open(fs, path, PIXA_OSAL_OPEN_READ, &file);
  if (rc != PIXA_OSAL_OK) {
    return PIXA_ERR_FS;
  }

  for (;;) {
    size_t n = 0u;
    rc = pixa_fs_read(fs, file, chunk, sizeof(chunk), &n);
    if (rc != PIXA_OSAL_OK) {
      pixa_fs_close(fs, file);
      vec_free(&vec);
      return PIXA_ERR_FS;
    }
    if (n == 0u) {
      break;
    }
    if (vec.len + n > max_len) {
      pixa_fs_close(fs, file);
      vec_free(&vec);
      return PIXA_ERR_INVALID_FORMAT;
    }
    rc = vec_append(&vec, chunk, n);
    if (rc != PIXA_OK) {
      pixa_fs_close(fs, file);
      vec_free(&vec);
      return rc;
    }
  }

  rc = pixa_fs_close(fs, file);
  if (rc != PIXA_OSAL_OK) {
    vec_free(&vec);
    return PIXA_ERR_FS;
  }

  *out_data = vec.data;
  *out_len = vec.len;
  return PIXA_OK;
}

static uint16_t argb4444_to_rgb565(uint16_t value) {
  uint8_t a4 = (uint8_t)((value >> 12) & 0x0fu);
  uint8_t r4 = (uint8_t)((value >> 8) & 0x0fu);
  uint8_t g4 = (uint8_t)((value >> 4) & 0x0fu);
  uint8_t b4 = (uint8_t)(value & 0x0fu);
  uint8_t r8;
  uint8_t g8;
  uint8_t b8;

  if (a4 == 0u) {
    return 0u;
  }
  r8 = (uint8_t)((r4 << 4) | r4);
  g8 = (uint8_t)((g4 << 4) | g4);
  b8 = (uint8_t)((b4 << 4) | b4);
  return (uint16_t)(((uint16_t)(r8 >> 3) << 11) | ((uint16_t)(g8 >> 2) << 5) |
                    (uint16_t)(b8 >> 3));
}

static uint16_t read_le16(const uint8_t *data, size_t offset) {
  return (uint16_t)data[offset] | ((uint16_t)data[offset + 1u] << 8);
}

static int palette_find_or_add(uint16_t *palette, uint16_t *palette_count,
                               uint16_t color, uint8_t *out_index) {
  if (color == 0u) {
    *out_index = 0u;
    return PIXA_OK;
  }
  for (uint16_t i = 1u; i < *palette_count; ++i) {
    if (palette[i] == color) {
      *out_index = (uint8_t)i;
      return PIXA_OK;
    }
  }
  if (*palette_count >= PIXA_MAX_COLORS) {
    return PIXA_ERR_INVALID_FORMAT;
  }
  palette[*palette_count] = color;
  *out_index = (uint8_t)*palette_count;
  *palette_count = (uint16_t)(*palette_count + 1u);
  return PIXA_OK;
}

static int index_frame(const uint8_t *argb4444, size_t frame_bytes,
                       uint8_t *indices, uint16_t *palette,
                       uint16_t *palette_count) {
  size_t pixels = frame_bytes / PIXA_ARGB4444_PIXEL_BYTES;
  for (size_t i = 0u; i < pixels; ++i) {
    uint16_t argb = read_le16(argb4444, i * 2u);
    uint16_t rgb565 = argb4444_to_rgb565(argb);
    int rc = palette_find_or_add(palette, palette_count, rgb565, indices + i);
    if (rc != PIXA_OK) {
      return rc;
    }
  }
  return PIXA_OK;
}

static int append_rle(pixa_vec_t *payload, const uint8_t *indices,
                      size_t pixel_count, uint32_t *out_offset,
                      uint32_t *out_len) {
  size_t start = payload->len;
  size_t i = 0u;
  int rc;

  while (i < pixel_count) {
    uint8_t value = indices[i];
    uint8_t count = 1u;
    while (i + count < pixel_count && indices[i + count] == value &&
           count < 255u) {
      ++count;
    }
    rc = vec_append(payload, &count, 1u);
    if (rc != PIXA_OK) {
      return rc;
    }
    rc = vec_append(payload, &value, 1u);
    if (rc != PIXA_OK) {
      return rc;
    }
    i += count;
  }

  if (start > UINT32_MAX || payload->len - start > UINT32_MAX) {
    return PIXA_ERR_INVALID_FORMAT;
  }
  *out_offset = (uint32_t)start;
  *out_len = (uint32_t)(payload->len - start);
  return PIXA_OK;
}

static int safe_clip_id(const char *id) {
  size_t len;
  if (id == NULL || id[0] == '\0') {
    return 0;
  }
  len = strlen(id);
  if (len > PIXA_MAX_CLIP_NAME) {
    return 0;
  }
  for (size_t i = 0u; i < len; ++i) {
    char ch = id[i];
    if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
          (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')) {
      return 0;
    }
  }
  return 1;
}

static int read_clip_state(const char *dir_path, const char *clip_id,
                           const pixa_dir_asset_t *asset,
                           const pixa_osal_api_t *fs, pixa_alloc_t alloc,
                           pixa_pack_clip_state_t *out) {
  char path[PIXA_PATH_MAX];
  char clips_path[PIXA_PATH_MAX];
  char frames_name[PIXA_MAX_CLIP_NAME + sizeof(".argb4444")];
  int rc;

  if (!safe_clip_id(clip_id)) {
    return PIXA_ERR_INVALID_CLIP_NAME;
  }
  memset(out, 0, sizeof(*out));

  rc = pixa_dir_find_clip(asset, clip_id, &out->dir_clip);
  if (rc != PIXA_OK) {
    return rc;
  }
  if (out->dir_clip.frame_bytes != pixa_canvas_argb4444_bytes(asset->canvas)) {
    return PIXA_ERR_INVALID_FORMAT;
  }
  memcpy(out->id, out->dir_clip.name, out->dir_clip.name_len);
  out->id[out->dir_clip.name_len] = '\0';
  out->durations_le = (uint8_t *)alloc.alloc(
      alloc.user, (size_t)out->dir_clip.frame_count * 2u);
  if (out->durations_le == NULL)
    return PIXA_ERR_NO_MEMORY;
  memcpy(out->durations_le, out->dir_clip.durations_le,
         (size_t)out->dir_clip.frame_count * 2u);
  out->dir_clip.name = out->id;
  out->dir_clip.durations_le = out->durations_le;

  int written =
      snprintf(frames_name, sizeof(frames_name), "%s.argb4444", clip_id);
  if (written <= 0 || (size_t)written >= sizeof(frames_name)) {
    alloc.free(alloc.user, out->durations_le);
    out->durations_le = NULL;
    return PIXA_ERR_INVALID_CLIP_NAME;
  }
  if ((rc = map_fs(pixa_fs_path_join(clips_path, sizeof(clips_path), dir_path,
                                     "clips"))) != PIXA_OK ||
      (rc = map_fs(pixa_fs_path_join(path, sizeof(path), clips_path,
                                     frames_name))) != PIXA_OK) {
    alloc.free(alloc.user, out->durations_le);
    out->durations_le = NULL;
    return rc;
  }
  if (out->dir_clip.frame_bytes == 0u ||
      out->dir_clip.frame_count > SIZE_MAX / out->dir_clip.frame_bytes) {
    alloc.free(alloc.user, out->durations_le);
    out->durations_le = NULL;
    return PIXA_ERR_INVALID_FORMAT;
  }
  out->frames_len =
      (size_t)out->dir_clip.frame_count * out->dir_clip.frame_bytes;
  rc = read_file_alloc(fs, path, alloc, out->frames_len, &out->frames,
                       &out->frames_len);
  if (rc != PIXA_OK || out->frames_len != (size_t)out->dir_clip.frame_count *
                                              out->dir_clip.frame_bytes) {
    if (out->frames != NULL) {
      alloc.free(alloc.user, out->frames);
      out->frames = NULL;
    }
    if (out->durations_le != NULL) {
      alloc.free(alloc.user, out->durations_le);
      out->durations_le = NULL;
    }
    return rc == PIXA_OK ? PIXA_ERR_INVALID_FORMAT : rc;
  }
  return PIXA_OK;
}

static void free_clip_states(pixa_pack_clip_state_t *clips, size_t clip_count,
                             pixa_alloc_t alloc) {
  if (clips == NULL) {
    return;
  }
  for (size_t i = 0u; i < clip_count; ++i) {
    if (clips[i].frames != NULL) {
      alloc.free(alloc.user, clips[i].frames);
    }
    if (clips[i].durations_le != NULL) {
      alloc.free(alloc.user, clips[i].durations_le);
    }
  }
  alloc.free(alloc.user, clips);
}

int pixa_pack_dir_to_file(const char *dir_path, const char *out_path,
                          const pixa_osal_api_t *fs,
                          const pixa_alloc_t *alloc_arg,
                          const pixa_pack_options_t *options,
                          pixa_pack_stats_t *out_stats) {
  pixa_alloc_t alloc = normalized_alloc(alloc_arg);
  uint8_t *index_data = NULL;
  size_t index_len = 0u;
  pixa_dir_asset_t meta;
  pixa_pack_clip_state_t *clips = NULL;
  pixa_vec_t payload = {0};
  pixa_vec_t out = {0};
  uint16_t palette[PIXA_MAX_COLORS] = {0};
  uint16_t palette_count = 1u;
  uint32_t frame_count = 0u;
  uint32_t payload_offset;
  uint32_t frame_offset;
  uint32_t clip_offset;
  uint32_t palette_offset = PIXA_HEADER_SIZE;
  uint8_t *indices = NULL;
  char index_path[PIXA_PATH_MAX];
  int rc;

  if (dir_path == NULL || out_path == NULL || fs == NULL || options == NULL ||
      options->clip_ids == NULL || options->clip_count == 0u ||
      options->clip_count > UINT16_MAX) {
    return PIXA_ERR_INVALID_ARG;
  }
  for (size_t i = 0u; i < options->clip_count; ++i) {
    if (!safe_clip_id(options->clip_ids[i])) {
      return PIXA_ERR_INVALID_CLIP_NAME;
    }
    for (size_t previous = 0u; previous < i; ++previous) {
      if (strcmp(options->clip_ids[previous], options->clip_ids[i]) == 0) {
        return PIXA_ERR_INVALID_FORMAT;
      }
    }
  }

  payload.alloc = alloc;
  out.alloc = alloc;

  if ((rc = map_fs(pixa_fs_path_join(index_path, sizeof(index_path), dir_path,
                                     "index.bin"))) != PIXA_OK) {
    return rc;
  }
  rc = read_file_alloc(fs, index_path, alloc, PIXA_MAX_INDEX_BYTES, &index_data,
                       &index_len);
  if (rc != PIXA_OK) {
    return rc;
  }
  rc = pixa_dir_open_memory(index_data, index_len, &meta);
  if (rc != PIXA_OK) {
    alloc.free(alloc.user, index_data);
    return rc;
  }
  if (meta.frame_bytes != pixa_canvas_argb4444_bytes(meta.canvas)) {
    alloc.free(alloc.user, index_data);
    return PIXA_ERR_INVALID_FORMAT;
  }

  clips = (pixa_pack_clip_state_t *)alloc.alloc(
      alloc.user, sizeof(*clips) * options->clip_count);
  if (clips == NULL) {
    alloc.free(alloc.user, index_data);
    return PIXA_ERR_NO_MEMORY;
  }
  memset(clips, 0, sizeof(*clips) * options->clip_count);

  indices =
      (uint8_t *)alloc.alloc(alloc.user, pixa_canvas_pixel_count(meta.canvas));
  if (indices == NULL) {
    alloc.free(alloc.user, index_data);
    free_clip_states(clips, options->clip_count, alloc);
    return PIXA_ERR_NO_MEMORY;
  }

  for (size_t i = 0u; i < options->clip_count; ++i) {
    clips[i].first_frame = frame_count;
    rc = read_clip_state(dir_path, options->clip_ids[i], &meta, fs, alloc,
                         clips + i);
    if (rc != PIXA_OK) {
      alloc.free(alloc.user, index_data);
      alloc.free(alloc.user, indices);
      free_clip_states(clips, options->clip_count, alloc);
      vec_free(&payload);
      return rc;
    }
    if (clips[i].dir_clip.frame_count > UINT32_MAX - frame_count) {
      alloc.free(alloc.user, index_data);
      alloc.free(alloc.user, indices);
      free_clip_states(clips, options->clip_count, alloc);
      vec_free(&payload);
      return PIXA_ERR_INVALID_FORMAT;
    }
    frame_count += clips[i].dir_clip.frame_count;

    for (uint32_t frame = 0u; frame < clips[i].dir_clip.frame_count; ++frame) {
      const uint8_t *frame_data =
          clips[i].frames + (size_t)frame * clips[i].dir_clip.frame_bytes;
      rc = index_frame(frame_data, clips[i].dir_clip.frame_bytes, indices,
                       palette, &palette_count);
      if (rc == PIXA_OK) {
        uint32_t rle_offset = 0u;
        uint32_t rle_len = 0u;
        rc = append_rle(&payload, indices, pixa_canvas_pixel_count(meta.canvas),
                        &rle_offset, &rle_len);
      }
      if (rc != PIXA_OK) {
        alloc.free(alloc.user, index_data);
        alloc.free(alloc.user, indices);
        free_clip_states(clips, options->clip_count, alloc);
        vec_free(&payload);
        return rc;
      }
    }
  }
  alloc.free(alloc.user, index_data);
  index_data = NULL;
  alloc.free(alloc.user, indices);
  indices = NULL;

  clip_offset = palette_offset + (uint32_t)palette_count * 2u;
  frame_offset =
      clip_offset + (uint32_t)options->clip_count * PIXA_CLIP_ENTRY_SIZE;
  payload_offset = frame_offset + frame_count * PIXA_FRAME_ENTRY_SIZE;

  if (payload.len > UINT32_MAX) {
    free_clip_states(clips, options->clip_count, alloc);
    vec_free(&payload);
    return PIXA_ERR_INVALID_FORMAT;
  }

  if ((rc = vec_append_zero(&out, payload_offset)) != PIXA_OK) {
    free_clip_states(clips, options->clip_count, alloc);
    vec_free(&payload);
    vec_free(&out);
    return rc;
  }
  memcpy(out.data, PIXA_MAGIC, 4u);
  write_u16(out.data, 4u, PIXA_VERSION);
  write_u16(out.data, 6u, PIXA_HEADER_SIZE);
  write_u16(out.data, 8u, meta.canvas.width);
  write_u16(out.data, 10u, meta.canvas.height);
  write_u16(out.data, 12u, palette_count);
  write_u16(out.data, 14u, (uint16_t)options->clip_count);
  write_u32(out.data, 16u, frame_count);
  write_u32(out.data, 20u, palette_offset);
  write_u32(out.data, 24u, clip_offset);
  write_u32(out.data, 28u, frame_offset);
  write_u32(out.data, 32u, payload_offset);
  write_u32(out.data, 36u, (uint32_t)payload.len);

  for (uint16_t i = 0u; i < palette_count; ++i) {
    write_u16(out.data, palette_offset + (size_t)i * 2u, palette[i]);
  }

  {
    uint32_t global_frame = 0u;
    size_t payload_cursor = 0u;
    for (size_t clip_index = 0u; clip_index < options->clip_count;
         ++clip_index) {
      pixa_pack_clip_state_t *clip = clips + clip_index;
      size_t clip_base = clip_offset + clip_index * PIXA_CLIP_ENTRY_SIZE;
      size_t id_len = clip->dir_clip.name_len;
      uint32_t total_duration = 0u;
      memcpy(out.data + clip_base, clip->dir_clip.name, id_len);
      write_i16(out.data, clip_base + 32u, clip->dir_clip.anchor.x);
      write_i16(out.data, clip_base + 34u, clip->dir_clip.anchor.y);
      write_u32(out.data, clip_base + 36u, clip->first_frame);
      write_u32(out.data, clip_base + 40u, clip->dir_clip.frame_count);
      for (uint32_t frame = 0u; frame < clip->dir_clip.frame_count; ++frame) {
        uint16_t duration =
            read_le16(clip->dir_clip.durations_le, (size_t)frame * 2u);
        total_duration += duration;
      }
      if (clip->dir_clip.total_duration_ms != 0u) {
        total_duration = clip->dir_clip.total_duration_ms;
      }
      write_u32(out.data, clip_base + 44u, total_duration);
      write_u16(out.data, clip_base + 48u, clip->dir_clip.loop ? 1u : 0u);

      for (uint32_t frame = 0u; frame < clip->dir_clip.frame_count; ++frame) {
        size_t frame_base =
            frame_offset + (size_t)global_frame * PIXA_FRAME_ENTRY_SIZE;
        uint32_t rle_len = 0u;
        size_t scan = payload_cursor;
        size_t pixels = 0u;
        while (scan + 1u < payload.len &&
               pixels < pixa_canvas_pixel_count(meta.canvas)) {
          pixels += payload.data[scan];
          scan += 2u;
        }
        if (pixels != pixa_canvas_pixel_count(meta.canvas) ||
            scan > payload.len) {
          free_clip_states(clips, options->clip_count, alloc);
          vec_free(&payload);
          vec_free(&out);
          return PIXA_ERR_INVALID_FORMAT;
        }
        rle_len = (uint32_t)(scan - payload_cursor);
        write_u16(out.data, frame_base + 0u,
                  read_le16(clip->dir_clip.durations_le, (size_t)frame * 2u));
        out.data[frame_base + 2u] = PIXA_FRAME_KEY;
        out.data[frame_base + 3u] = 1u;
        write_u32(out.data, frame_base + 4u, (uint32_t)payload_cursor);
        write_u32(out.data, frame_base + 8u, rle_len);
        payload_cursor = scan;
        ++global_frame;
      }
    }
  }

  rc = vec_append(&out, payload.data, payload.len);
  vec_free(&payload);
  free_clip_states(clips, options->clip_count, alloc);
  if (rc != PIXA_OK) {
    vec_free(&out);
    return rc;
  }

  rc = pixa_fs_write_file(fs, out_path, out.data, out.len);
  if (rc != PIXA_OSAL_OK) {
    vec_free(&out);
    return PIXA_ERR_FS;
  }

  if (out_stats != NULL) {
    out_stats->clip_count = options->clip_count;
    out_stats->frame_count = frame_count;
    out_stats->payload_len = out.len - payload_offset;
    out_stats->output_len = out.len;
  }
  vec_free(&out);
  return PIXA_OK;
}
