#ifndef PIXA_OSAL_H
#define PIXA_OSAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pixa_osal_file pixa_osal_file_t;

typedef enum pixa_osal_open_mode {
  PIXA_OSAL_OPEN_WRITE_TRUNCATE = 1,
  PIXA_OSAL_OPEN_READ = 2,
} pixa_osal_open_mode_t;

typedef struct pixa_osal_stat {
  uint64_t size;
  int is_dir;
} pixa_osal_stat_t;

enum {
  PIXA_OSAL_OK = 0,
  PIXA_OSAL_ERR_INVALID_ARG = -1,
  PIXA_OSAL_ERR_IO = -2,
  PIXA_OSAL_ERR_NO_MEMORY = -3,
  PIXA_OSAL_ERR_NO_SPACE = -4,
  PIXA_OSAL_ERR_UNSUPPORTED = -5,
};

typedef struct pixa_osal_vtable {
  int (*mkdir)(void *user, const char *path);
  int (*open)(void *user, const char *path, pixa_osal_open_mode_t mode,
              pixa_osal_file_t **out_file);
  int (*read)(void *user, pixa_osal_file_t *file, void *data, size_t len,
              size_t *out_read);
  int (*seek)(void *user, pixa_osal_file_t *file, uint64_t position);
  int (*write)(void *user, pixa_osal_file_t *file, const void *data, size_t len,
               size_t *out_written);
  int (*sync)(void *user, pixa_osal_file_t *file);
  int (*close)(void *user, pixa_osal_file_t *file);
  int (*stat)(void *user, const char *path, pixa_osal_stat_t *out_stat);
  int (*clear)(void *user, const char *path);
  int (*remove)(void *user, const char *path);
  int (*rename)(void *user, const char *old_path, const char *new_path);
} pixa_osal_vtable_t;

typedef struct pixa_osal_api {
  void *user;
  const pixa_osal_vtable_t *vtable;
} pixa_osal_api_t;

static inline int pixa_osal_mkdir(const pixa_osal_api_t *api,
                                  const char *path) {
  if (api == NULL || api->vtable == NULL || api->vtable->mkdir == NULL ||
      path == NULL)
    return PIXA_OSAL_ERR_INVALID_ARG;
  return api->vtable->mkdir(api->user, path);
}
static inline int pixa_osal_open(const pixa_osal_api_t *api, const char *path,
                                 pixa_osal_open_mode_t mode,
                                 pixa_osal_file_t **out_file) {
  if (api == NULL || api->vtable == NULL || api->vtable->open == NULL ||
      path == NULL || out_file == NULL)
    return PIXA_OSAL_ERR_INVALID_ARG;
  return api->vtable->open(api->user, path, mode, out_file);
}
static inline int pixa_osal_read(const pixa_osal_api_t *api,
                                 pixa_osal_file_t *file, void *data, size_t len,
                                 size_t *out_read) {
  if (api == NULL || api->vtable == NULL || api->vtable->read == NULL ||
      file == NULL || (data == NULL && len != 0u) || out_read == NULL)
    return PIXA_OSAL_ERR_INVALID_ARG;
  return api->vtable->read(api->user, file, data, len, out_read);
}
static inline int pixa_osal_write(const pixa_osal_api_t *api,
                                  pixa_osal_file_t *file, const void *data,
                                  size_t len, size_t *out_written) {
  if (api == NULL || api->vtable == NULL || api->vtable->write == NULL ||
      file == NULL || (data == NULL && len != 0u) || out_written == NULL)
    return PIXA_OSAL_ERR_INVALID_ARG;
  return api->vtable->write(api->user, file, data, len, out_written);
}
static inline int pixa_osal_sync(const pixa_osal_api_t *api,
                                 pixa_osal_file_t *file) {
  if (api == NULL || file == NULL)
    return PIXA_OSAL_ERR_INVALID_ARG;
  if (api->vtable == NULL || api->vtable->sync == NULL)
    return PIXA_OSAL_OK;
  return api->vtable->sync(api->user, file);
}
static inline int pixa_osal_close(const pixa_osal_api_t *api,
                                  pixa_osal_file_t *file) {
  if (api == NULL || api->vtable == NULL || api->vtable->close == NULL ||
      file == NULL)
    return PIXA_OSAL_ERR_INVALID_ARG;
  return api->vtable->close(api->user, file);
}
static inline int pixa_osal_stat(const pixa_osal_api_t *api, const char *path,
                                 pixa_osal_stat_t *out_stat) {
  if (api == NULL || api->vtable == NULL || api->vtable->stat == NULL ||
      path == NULL || out_stat == NULL)
    return PIXA_OSAL_ERR_INVALID_ARG;
  return api->vtable->stat(api->user, path, out_stat);
}
static inline int pixa_osal_remove(const pixa_osal_api_t *api,
                                   const char *path) {
  if (api == NULL || api->vtable == NULL || api->vtable->remove == NULL ||
      path == NULL)
    return PIXA_OSAL_ERR_UNSUPPORTED;
  return api->vtable->remove(api->user, path);
}
static inline int pixa_osal_rename(const pixa_osal_api_t *api,
                                   const char *old_path, const char *new_path) {
  if (api == NULL || api->vtable == NULL || api->vtable->rename == NULL ||
      old_path == NULL || new_path == NULL)
    return PIXA_OSAL_ERR_UNSUPPORTED;
  return api->vtable->rename(api->user, old_path, new_path);
}

#ifdef __cplusplus
}
#endif

#endif
