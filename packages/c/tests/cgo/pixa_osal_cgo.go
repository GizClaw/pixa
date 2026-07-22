package cgo

/*
#cgo CFLAGS: -I../../include
#include "pixa_osal.h"

struct pixa_osal_file { int unused; };

static int cgo_mkdir(void *user, const char *path) {
    int *expected = (int *)user;
    return *expected == 42 && path[0] == 'x' && path[1] == '\0'
        ? PIXA_OSAL_OK
        : PIXA_OSAL_ERR_IO;
}

static int cgo_osal_smoke(void) {
    int expected = 42;
    pixa_osal_vtable_t vtable = {0};
    vtable.mkdir = cgo_mkdir;
    pixa_osal_api_t api = { .user = &expected, .vtable = &vtable };
    return pixa_osal_mkdir(&api, "x");
}
*/
import "C"

func hostOSALSmoke() int {
	return int(C.cgo_osal_smoke())
}
