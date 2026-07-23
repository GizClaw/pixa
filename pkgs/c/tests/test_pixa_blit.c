#include "pixa_blit.h"

#include <assert.h>
#include <stdint.h>

int main(void) {
  const uint8_t source[] = {
      0x00, 0xff, 0x0f, 0xf0, 0xff, 0xf0, 0xff, 0xff,
  };
  uint16_t destination[16] = {0};
  assert(pixa_blit_argb4444_scaled_to_rgb565(destination, 4u, 4u, 4u, source,
                                             2u, 2u, 0, 0, 4u, 4u) == PIXA_OK);
  assert(destination[0] == destination[1]);
  assert(destination[0] == destination[4]);
  assert(destination[2] == destination[3]);
  assert(destination[8] == destination[12]);
  assert(destination[10] == destination[15]);
  assert(destination[0] != destination[2]);
  assert(destination[0] != destination[8]);
  return 0;
}
