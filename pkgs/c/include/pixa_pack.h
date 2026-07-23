#ifndef PIXA_PACK_H
#define PIXA_PACK_H

#include "pixa_osal.h"
#include "pixa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pixa_pack_options {
  const char *const *clip_ids;
  size_t clip_count;
} pixa_pack_options_t;

typedef struct pixa_pack_stats {
  size_t clip_count;
  size_t frame_count;
  size_t payload_len;
  size_t output_len;
} pixa_pack_stats_t;

int pixa_pack_dir_to_file(const char *dir_path, const char *out_path,
                          const pixa_osal_api_t *fs, const pixa_alloc_t *alloc,
                          const pixa_pack_options_t *options,
                          pixa_pack_stats_t *out_stats);

#ifdef __cplusplus
}
#endif

#endif
