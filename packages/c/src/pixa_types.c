#include "pixa_types.h"

size_t pixa_canvas_pixel_count(pixa_canvas_t canvas) {
  return (size_t)canvas.width * (size_t)canvas.height;
}

size_t pixa_canvas_bgra_bytes(pixa_canvas_t canvas) {
  return pixa_canvas_pixel_count(canvas) * PIXA_PIXEL_BYTES;
}

size_t pixa_canvas_argb4444_bytes(pixa_canvas_t canvas) {
  return pixa_canvas_pixel_count(canvas) * PIXA_ARGB4444_PIXEL_BYTES;
}

const char *pixa_result_name(int result) {
  switch (result) {
  case PIXA_OK:
    return "PIXA_OK";
  case PIXA_ERR_INVALID_ARG:
    return "PIXA_ERR_INVALID_ARG";
  case PIXA_ERR_INVALID_FORMAT:
    return "PIXA_ERR_INVALID_FORMAT";
  case PIXA_ERR_UNSUPPORTED_VERSION:
    return "PIXA_ERR_UNSUPPORTED_VERSION";
  case PIXA_ERR_UNSUPPORTED_FRAME:
    return "PIXA_ERR_UNSUPPORTED_FRAME";
  case PIXA_ERR_CLIP_NOT_FOUND:
    return "PIXA_ERR_CLIP_NOT_FOUND";
  case PIXA_ERR_FRAME_NOT_FOUND:
    return "PIXA_ERR_FRAME_NOT_FOUND";
  case PIXA_ERR_OUTPUT_TOO_SMALL:
    return "PIXA_ERR_OUTPUT_TOO_SMALL";
  case PIXA_ERR_CANVAS_MISMATCH:
    return "PIXA_ERR_CANVAS_MISMATCH";
  case PIXA_ERR_INVALID_CLIP_NAME:
    return "PIXA_ERR_INVALID_CLIP_NAME";
  case PIXA_ERR_NO_MEMORY:
    return "PIXA_ERR_NO_MEMORY";
  case PIXA_ERR_FS:
    return "PIXA_ERR_FS";
  default:
    return "PIXA_ERR_UNKNOWN";
  }
}
