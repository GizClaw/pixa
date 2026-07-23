#include "pixa_extract.h"

#include "pixa_decode.h"
#include "pixa_dir.h"
#include "pixa_reader.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PIXA_PATH_MAX 384u

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

static int pixa_fs_path_append_suffix(char *out, size_t out_len,
                                      const char *path, const char *suffix) {
  size_t path_len;
  size_t suffix_len;

  if (out == NULL || path == NULL || suffix == NULL) {
    return PIXA_OSAL_ERR_INVALID_ARG;
  }
  path_len = strlen(path);
  suffix_len = strlen(suffix);
  if (path_len + suffix_len + 1u > out_len) {
    return PIXA_OSAL_ERR_NO_SPACE;
  }
  memcpy(out, path, path_len);
  memcpy(out + path_len, suffix, suffix_len);
  out[path_len + suffix_len] = '\0';
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

static int pixa_fs_sync(const pixa_osal_api_t *fs, pixa_osal_file_t *file) {
  if (fs == NULL || file == NULL) {
    return PIXA_OSAL_ERR_INVALID_ARG;
  }
  return pixa_osal_sync(fs, file);
}

static int pixa_fs_close(const pixa_osal_api_t *fs, pixa_osal_file_t *file) {
  if (fs == NULL || file == NULL) {
    return PIXA_OSAL_ERR_INVALID_ARG;
  }
  return pixa_osal_close(fs, file);
}

static int pixa_fs_make_dir_all(const pixa_osal_api_t *fs, const char *path) {
  char current[PIXA_PATH_MAX];
  size_t len;

  if (fs == NULL || path == NULL || path[0] == '\0') {
    return PIXA_OSAL_ERR_INVALID_ARG;
  }
  len = strlen(path);
  if (len >= sizeof(current)) {
    return PIXA_OSAL_ERR_NO_SPACE;
  }
  memcpy(current, path, len + 1u);
  for (size_t i = 1u; i < len; ++i) {
    if (current[i] != '/') {
      continue;
    }
    current[i] = '\0';
    if (current[0] != '\0') {
      int rc = pixa_osal_mkdir(fs, current);
      if (rc != PIXA_OSAL_OK && rc != PIXA_OSAL_ERR_UNSUPPORTED) {
        return rc;
      }
    }
    current[i] = '/';
  }
  return pixa_osal_mkdir(fs, current);
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
    rc = pixa_fs_sync(fs, file);
  }
  close_rc = pixa_fs_close(fs, file);
  return rc == PIXA_OSAL_OK ? close_rc : rc;
}

static int validate_clip_name(const pixa_clip_t *clip) {
  if (clip == NULL || clip->name == NULL || clip->name_len == 0u ||
      clip->name_len > PIXA_MAX_CLIP_NAME) {
    return PIXA_ERR_INVALID_CLIP_NAME;
  }
  for (size_t i = 0u; i < clip->name_len; ++i) {
    char ch = clip->name[i];
    if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
          (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')) {
      return PIXA_ERR_INVALID_CLIP_NAME;
    }
  }
  return PIXA_OK;
}

static int clip_name_z(const pixa_clip_t *clip,
                       char out[PIXA_MAX_CLIP_NAME + 1u]) {
  int rc = validate_clip_name(clip);
  if (rc != PIXA_OK) {
    return rc;
  }
  memcpy(out, clip->name, clip->name_len);
  out[clip->name_len] = '\0';
  return PIXA_OK;
}

static void write_u16_le(uint8_t *out, uint16_t value) {
  out[0] = (uint8_t)value;
  out[1] = (uint8_t)(value >> 8u);
}

static int write_index(const pixa_asset_t *asset, const char *dir_path,
                       const pixa_osal_api_t *fs, const pixa_alloc_t *alloc,
                       pixa_extract_stats_t *stats) {
  pixa_dir_clip_t *clips = NULL;
  uint8_t *durations = NULL;
  uint8_t *index = NULL;
  char path[PIXA_PATH_MAX];
  char temp_path[PIXA_PATH_MAX];
  size_t index_len = 0u;
  int rc = PIXA_OK;

#if SIZE_MAX == UINT32_MAX
  if (asset->frame_count > SIZE_MAX / 2u) {
    return PIXA_ERR_NO_MEMORY;
  }
#endif
  clips = (pixa_dir_clip_t *)alloc->alloc(
      alloc->user, (size_t)asset->clip_count * sizeof(*clips));
  durations =
      (uint8_t *)alloc->alloc(alloc->user, (size_t)asset->frame_count * 2u);
  if (clips == NULL || durations == NULL) {
    rc = PIXA_ERR_NO_MEMORY;
    goto cleanup;
  }
  uint32_t duration_cursor = 0u;
  for (uint16_t i = 0u; i < asset->clip_count; ++i) {
    pixa_clip_t source;
    if ((rc = pixa_clip_at(asset, i, &source)) != PIXA_OK)
      goto cleanup;
    clips[i] = (pixa_dir_clip_t){
        .name = source.name,
        .name_len = source.name_len,
        .anchor = source.anchor,
        .loop = source.loop,
        .total_duration_ms = source.total_duration_ms,
        .frame_count = source.frame_count,
        .frame_bytes = (uint32_t)pixa_canvas_argb4444_bytes(asset->canvas),
        .durations_le = durations + (size_t)duration_cursor * 2u,
    };
    for (uint32_t frame_index = 0u; frame_index < source.frame_count;
         ++frame_index) {
      pixa_frame_t frame;
      if ((rc = pixa_frame_at(asset, source.first_frame + frame_index,
                              &frame)) != PIXA_OK)
        goto cleanup;
      write_u16_le(durations + (size_t)duration_cursor * 2u,
                   frame.duration_ms == 0u ? 1u : frame.duration_ms);
      ++duration_cursor;
    }
  }
  rc = pixa_dir_encode(&asset->canvas, clips, asset->clip_count, NULL, 0u,
                       &index_len);
  if (rc != PIXA_ERR_OUTPUT_TOO_SMALL)
    goto cleanup;
  index = (uint8_t *)alloc->alloc(alloc->user, index_len);
  if (index == NULL) {
    rc = PIXA_ERR_NO_MEMORY;
    goto cleanup;
  }
  rc = pixa_dir_encode(&asset->canvas, clips, asset->clip_count, index,
                       index_len, &index_len);
  if (rc != PIXA_OK)
    goto cleanup;
  if (pixa_fs_path_join(path, sizeof(path), dir_path, "index.bin") !=
          PIXA_OSAL_OK ||
      pixa_fs_path_join(temp_path, sizeof(temp_path), dir_path,
                        "index.bin.tmp") != PIXA_OSAL_OK ||
      pixa_fs_write_file(fs, temp_path, index, index_len) != PIXA_OSAL_OK ||
      pixa_osal_rename(fs, temp_path, path) != PIXA_OSAL_OK) {
    (void)pixa_osal_remove(fs, temp_path);
    rc = PIXA_ERR_FS;
    goto cleanup;
  }
  stats->file_count += 1u;
  stats->payload_len += index_len;

cleanup:
  if (index != NULL)
    alloc->free(alloc->user, index);
  if (durations != NULL)
    alloc->free(alloc->user, durations);
  if (clips != NULL)
    alloc->free(alloc->user, clips);
  return rc;
}

static int write_clip_frames(const pixa_asset_t *asset, const pixa_clip_t *clip,
                             const char *clips_dir, const pixa_osal_api_t *fs,
                             const pixa_alloc_t *alloc,
                             pixa_extract_stats_t *stats,
                             pixa_extract_progress_fn progress,
                             void *progress_user) {
  char frames_path[PIXA_PATH_MAX];
  pixa_osal_file_t *file = NULL;
  size_t bgra_len = pixa_canvas_bgra_bytes(asset->canvas);
  size_t frame_len = pixa_canvas_argb4444_bytes(asset->canvas);
  uint8_t *bgra = NULL;
  uint8_t *frame = NULL;
  int rc;

  char clip_name[PIXA_MAX_CLIP_NAME + sizeof(".argb4444")];
  if ((rc = clip_name_z(clip, clip_name)) != PIXA_OK)
    return rc;
  if (strlen(clip_name) + sizeof(".argb4444") > sizeof(clip_name))
    return PIXA_ERR_INVALID_CLIP_NAME;
  strcat(clip_name, ".argb4444");
  rc =
      pixa_fs_path_join(frames_path, sizeof(frames_path), clips_dir, clip_name);
  if (rc != PIXA_OSAL_OK) {
    return PIXA_ERR_FS;
  }

  bgra = (uint8_t *)alloc->alloc(alloc->user, bgra_len);
  frame = (uint8_t *)alloc->alloc(alloc->user, frame_len);
  if (bgra == NULL || frame == NULL) {
    if (bgra != NULL) {
      alloc->free(alloc->user, bgra);
    }
    if (frame != NULL) {
      alloc->free(alloc->user, frame);
    }
    return PIXA_ERR_NO_MEMORY;
  }
  memset(bgra, 0, bgra_len);

  rc = pixa_fs_open(fs, frames_path, PIXA_OSAL_OPEN_WRITE_TRUNCATE, &file);
  if (rc != PIXA_OSAL_OK) {
    alloc->free(alloc->user, frame);
    alloc->free(alloc->user, bgra);
    return PIXA_ERR_FS;
  }

  for (uint32_t i = 0u; i < clip->frame_count; ++i) {
    rc = pixa_apply_clip_frame_bgra(asset, clip, i, bgra, bgra_len);
    if (rc == PIXA_OK) {
      rc = pixa_bgra_to_argb4444(bgra, bgra_len, frame, frame_len);
    }
    if (rc == PIXA_OK) {
      rc = map_fs(pixa_fs_write_all(fs, file, frame, frame_len));
    }
    if (rc != PIXA_OK) {
      pixa_fs_close(fs, file);
      alloc->free(alloc->user, frame);
      alloc->free(alloc->user, bgra);
      return rc;
    }
    stats->frame_count += 1u;
    stats->payload_len += frame_len;
    if (progress != NULL && stats->frame_count < asset->frame_count) {
      progress(progress_user, (uint32_t)stats->frame_count, asset->frame_count);
    }
  }

  rc = pixa_fs_sync(fs, file);
  if (rc == PIXA_OSAL_OK) {
    rc = pixa_fs_close(fs, file);
  } else {
    pixa_fs_close(fs, file);
  }
  alloc->free(alloc->user, frame);
  alloc->free(alloc->user, bgra);
  if (rc != PIXA_OSAL_OK) {
    return PIXA_ERR_FS;
  }
  stats->file_count += 1u;
  return PIXA_OK;
}

static int
extract_memory_to_dir(const void *data, size_t len, const char *dir_path,
                      const pixa_osal_api_t *fs, const pixa_alloc_t *alloc_arg,
                      pixa_extract_stats_t *out_stats,
                      pixa_extract_progress_fn progress, void *progress_user) {
  pixa_asset_t asset;
  pixa_extract_stats_t stats;
  pixa_alloc_t alloc = normalized_alloc(alloc_arg);
  char clips_dir[PIXA_PATH_MAX];
  int rc;

  if (data == NULL || len == 0u || dir_path == NULL || fs == NULL) {
    return PIXA_ERR_INVALID_ARG;
  }
  rc = pixa_open_memory(data, len, &asset);
  if (rc != PIXA_OK) {
    return rc;
  }

  memset(&stats, 0, sizeof(stats));
  if ((rc = map_fs(pixa_fs_make_dir_all(fs, dir_path))) != PIXA_OK) {
    return rc;
  }
  if ((rc = map_fs(pixa_fs_path_join(clips_dir, sizeof(clips_dir), dir_path,
                                     "clips"))) != PIXA_OK) {
    return rc;
  }
  if ((rc = map_fs(pixa_fs_make_dir_all(fs, clips_dir))) != PIXA_OK) {
    return rc;
  }
  /* A stale index must never make a partial refresh appear complete. */
  {
    char stale_path[PIXA_PATH_MAX];
    if (pixa_fs_path_join(stale_path, sizeof(stale_path), dir_path,
                          "index.bin") != PIXA_OSAL_OK)
      return PIXA_ERR_FS;
    (void)pixa_osal_remove(fs, stale_path);
    if (pixa_fs_path_join(stale_path, sizeof(stale_path), dir_path,
                          "meta.json") == PIXA_OSAL_OK)
      (void)pixa_osal_remove(fs, stale_path);
  }
  for (uint16_t i = 0u; i < asset.clip_count; ++i) {
    pixa_clip_t clip;

    if ((rc = pixa_clip_at(&asset, i, &clip)) != PIXA_OK) {
      return rc;
    }
    if ((rc = write_clip_frames(&asset, &clip, clips_dir, fs, &alloc, &stats,
                                progress, progress_user)) != PIXA_OK) {
      return rc;
    }
  }
  /* index.bin is written last and is the completion marker for the asset. */
  if ((rc = write_index(&asset, dir_path, fs, &alloc, &stats)) != PIXA_OK) {
    return rc;
  }

  if (progress != NULL) {
    progress(progress_user, asset.frame_count, asset.frame_count);
  }
  if (out_stats != NULL) {
    *out_stats = stats;
  }
  return PIXA_OK;
}

int pixa_extract_memory_to_dir(const void *data, size_t len,
                               const char *dir_path, const pixa_osal_api_t *fs,
                               const pixa_alloc_t *alloc_arg,
                               pixa_extract_stats_t *out_stats) {
  return extract_memory_to_dir(data, len, dir_path, fs, alloc_arg, out_stats,
                               NULL, NULL);
}

int pixa_extract_memory(const void *data, size_t len, const char *source_path,
                        const pixa_osal_api_t *fs, const pixa_alloc_t *alloc,
                        pixa_extract_stats_t *out_stats) {
  char dir_path[PIXA_PATH_MAX];
  int rc;

  if (source_path == NULL) {
    return PIXA_ERR_INVALID_ARG;
  }
  rc =
      pixa_fs_path_append_suffix(dir_path, sizeof(dir_path), source_path, ".d");
  if (rc != PIXA_OSAL_OK) {
    return PIXA_ERR_FS;
  }
  return pixa_extract_memory_to_dir(data, len, dir_path, fs, alloc, out_stats);
}

int pixa_extract_memory_with_progress(const void *data, size_t len,
                                      const char *source_path,
                                      const pixa_osal_api_t *fs,
                                      const pixa_alloc_t *alloc,
                                      pixa_extract_stats_t *out_stats,
                                      pixa_extract_progress_fn progress,
                                      void *progress_user) {
  char dir_path[PIXA_PATH_MAX];
  int rc;

  if (source_path == NULL) {
    return PIXA_ERR_INVALID_ARG;
  }
  rc =
      pixa_fs_path_append_suffix(dir_path, sizeof(dir_path), source_path, ".d");
  if (rc != PIXA_OSAL_OK) {
    return PIXA_ERR_FS;
  }
  return extract_memory_to_dir(data, len, dir_path, fs, alloc, out_stats,
                               progress, progress_user);
}
