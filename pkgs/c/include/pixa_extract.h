#ifndef PIXA_EXTRACT_H
#define PIXA_EXTRACT_H

#include "pixa_osal.h"
#include "pixa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Reports frames written during synchronous PIXA extraction.
 *
 * user is borrowed for the extraction call. completed_frames is monotonic and
 * the final callback reports total_frames / total_frames after the final
 * output file has been synchronized.
 */
typedef void (*pixa_extract_progress_fn)(void *user, uint32_t completed_frames,
                                         uint32_t total_frames);

int pixa_extract_memory_to_dir(const void *data, size_t len,
                               const char *dir_path, const pixa_osal_api_t *fs,
                               const pixa_alloc_t *alloc,
                               pixa_extract_stats_t *out_stats);
int pixa_extract_memory(const void *data, size_t len, const char *source_path,
                        const pixa_osal_api_t *fs, const pixa_alloc_t *alloc,
                        pixa_extract_stats_t *out_stats);
/** Extracts an in-memory PIXA and reports durable frame progress. */
int pixa_extract_memory_with_progress(const void *data, size_t len,
                                      const char *source_path,
                                      const pixa_osal_api_t *fs,
                                      const pixa_alloc_t *alloc,
                                      pixa_extract_stats_t *out_stats,
                                      pixa_extract_progress_fn progress,
                                      void *progress_user);

#ifdef __cplusplus
}
#endif

#endif
