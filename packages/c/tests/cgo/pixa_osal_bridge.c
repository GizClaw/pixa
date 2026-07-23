#include <stdint.h>
#include <stdlib.h>

#include "_cgo_export.h"
#include "pixa_extract.h"
#include "pixa_osal.h"

struct pixa_osal_file {
  uintptr_t handle;
};

static int bridge_mkdir(void *user, const char *path) {
  return pixaGoMkdir((uintptr_t)user, (char *)path);
}

static int bridge_open(void *user, const char *path, pixa_osal_open_mode_t mode,
                       pixa_osal_file_t **out_file) {
  uintptr_t handle = 0u;
  int rc = pixaGoOpen((uintptr_t)user, (char *)path, mode, &handle);
  pixa_osal_file_t *file;
  if (rc != PIXA_OSAL_OK)
    return rc;
  file = calloc(1u, sizeof(*file));
  if (file == NULL) {
    (void)pixaGoClose((uintptr_t)user, handle);
    return PIXA_OSAL_ERR_NO_MEMORY;
  }
  file->handle = handle;
  *out_file = file;
  return PIXA_OSAL_OK;
}

static int bridge_read(void *user, pixa_osal_file_t *file, void *data,
                       size_t len, size_t *out_read) {
  return pixaGoRead((uintptr_t)user, file->handle, data, len, out_read);
}

static int bridge_seek(void *user, pixa_osal_file_t *file, uint64_t position) {
  return pixaGoSeek((uintptr_t)user, file->handle, position);
}

static int bridge_write(void *user, pixa_osal_file_t *file, const void *data,
                        size_t len, size_t *out_written) {
  return pixaGoWrite((uintptr_t)user, file->handle, (void *)data, len,
                     out_written);
}

static int bridge_sync(void *user, pixa_osal_file_t *file) {
  return pixaGoSync((uintptr_t)user, file->handle);
}

static int bridge_close(void *user, pixa_osal_file_t *file) {
  int rc = pixaGoClose((uintptr_t)user, file->handle);
  free(file);
  return rc;
}

static int bridge_rename(void *user, const char *old_path,
                         const char *new_path) {
  return pixaGoRename((uintptr_t)user, (char *)old_path, (char *)new_path);
}

static int bridge_remove(void *user, const char *path) {
  return pixaGoRemove((uintptr_t)user, (char *)path);
}

pixa_osal_api_t *pixaGoOSALNew(uintptr_t handle) {
  static const pixa_osal_vtable_t vtable = {
      .mkdir = bridge_mkdir,
      .open = bridge_open,
      .read = bridge_read,
      .seek = bridge_seek,
      .write = bridge_write,
      .sync = bridge_sync,
      .close = bridge_close,
      .remove = bridge_remove,
      .rename = bridge_rename,
  };
  pixa_osal_api_t *api = calloc(1u, sizeof(*api));
  if (api == NULL)
    return NULL;
  api->user = (void *)handle;
  api->vtable = &vtable;
  return api;
}

void pixaGoOSALFree(pixa_osal_api_t *api) { free(api); }

int pixaGoOSALMkdir(pixa_osal_api_t *api, const char *path) {
  return pixa_osal_mkdir(api, path);
}

int pixaGoOSALReadSecond(pixa_osal_api_t *api, const char *path,
                         uint8_t *out_byte) {
  pixa_osal_file_t *file = NULL;
  size_t read = 0u;
  int rc;
  if (out_byte == NULL)
    return PIXA_OSAL_ERR_INVALID_ARG;
  rc = pixa_osal_open(api, path, PIXA_OSAL_OPEN_READ, &file);
  if (rc != PIXA_OSAL_OK)
    return rc;
  rc = pixa_osal_seek(api, file, 1u);
  if (rc == PIXA_OSAL_OK)
    rc = pixa_osal_read(api, file, out_byte, 1u, &read);
  {
    int close_rc = pixa_osal_close(api, file);
    if (rc == PIXA_OSAL_OK)
      rc = close_rc;
  }
  if (rc != PIXA_OSAL_OK)
    return rc;
  return read == 1u ? PIXA_OSAL_OK : PIXA_OSAL_ERR_IO;
}

int pixaGoExtract(const void *data, size_t len, const char *path,
                  pixa_osal_api_t *api) {
  return pixa_extract_memory_to_dir(data, len, path, api, NULL, NULL);
}
