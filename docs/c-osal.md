# C OSAL boundary

`packages/c` is portable C: its public headers and sources do not include a
firmware PAL, an RTOS, LVGL, or a board SDK. Memory-only parsing and decoding
need no platform services.

Packing and extraction receive a `pixa_osal_api_t` from the caller. The ABI in
`packages/c/include/pixa_osal.h` covers the small filesystem surface the core
needs: directories, files, read/write, sync, rename, and removal. Path roots,
mount policy, downloads, and display ownership remain outside PIXA.

`packages/c/tests/cgo` is the portable C test package. It compiles the core C
sources and the existing C test cases through cgo, then runs them as `go test`
alongside a real Go-backed OSAL provider. The thin C bridge forwards the ABI to
Go `os` and `io` filesystem operations, so the same test runs natively on
Linux, macOS, and Windows. Run it with `make c-check`; CI runs `go test` on all
three hosts.
