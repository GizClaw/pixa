package cgo

/*
#cgo CFLAGS: -I../../include
#include <stdlib.h>
#include "pixa_osal.h"
pixa_osal_api_t *pixaGoOSALNew(uintptr_t handle);
void pixaGoOSALFree(pixa_osal_api_t *api);
int pixaGoOSALMkdir(pixa_osal_api_t *api, const char *path);
int pixaGoOSALReadSecond(pixa_osal_api_t *api, const char *path,
                         uint8_t *out_byte);
int pixaGoExtract(const void *data, size_t len, const char *path,
                  pixa_osal_api_t *api);
*/
import "C"

import (
	"errors"
	"io"
	"os"
	"path/filepath"
	"runtime/cgo"
	"strings"
	"unsafe"
)

type filesystem struct{ root string }

func (fs filesystem) path(relative string) (string, error) {
	if relative == "" || filepath.IsAbs(relative) {
		return "", errors.New("pixa osal: path must be relative")
	}
	path := filepath.Join(fs.root, filepath.FromSlash(relative))
	rel, err := filepath.Rel(fs.root, path)
	if err != nil || rel == ".." || strings.HasPrefix(rel, ".."+string(os.PathSeparator)) {
		return "", errors.New("pixa osal: path escapes root")
	}
	return path, nil
}

func filesystemFor(handle C.uintptr_t) filesystem {
	return cgo.Handle(handle).Value().(filesystem)
}

func fileFor(handle C.uintptr_t) *os.File {
	return cgo.Handle(handle).Value().(*os.File)
}

func osalError(error) C.int { return C.int(-2) }

//export pixaGoMkdir
func pixaGoMkdir(handle C.uintptr_t, path *C.char) C.int {
	name, err := filesystemFor(handle).path(C.GoString(path))
	if err != nil {
		return osalError(err)
	}
	if err := os.MkdirAll(name, 0o755); err != nil {
		return osalError(err)
	}
	return 0
}

//export pixaGoOpen
func pixaGoOpen(root C.uintptr_t, path *C.char, mode C.pixa_osal_open_mode_t, out *C.uintptr_t) C.int {
	name, err := filesystemFor(root).path(C.GoString(path))
	if err != nil {
		return osalError(err)
	}
	var file *os.File
	switch mode {
	case C.PIXA_OSAL_OPEN_WRITE_TRUNCATE:
		file, err = os.OpenFile(name, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0o644)
	case C.PIXA_OSAL_OPEN_READ:
		file, err = os.Open(name)
	default:
		return C.int(-5)
	}
	if err != nil {
		return osalError(err)
	}
	*out = C.uintptr_t(cgo.NewHandle(file))
	return 0
}

//export pixaGoRead
func pixaGoRead(_ C.uintptr_t, handle C.uintptr_t, data unsafe.Pointer, length C.size_t, out *C.size_t) C.int {
	if uint64(length) > uint64(^uint(0)>>1) {
		return C.int(-4)
	}
	n, err := fileFor(handle).Read(unsafe.Slice((*byte)(data), int(length)))
	*out = C.size_t(n)
	if err != nil && !errors.Is(err, io.EOF) {
		return osalError(err)
	}
	return 0
}

//export pixaGoSeek
func pixaGoSeek(_ C.uintptr_t, handle C.uintptr_t, position C.uint64_t) C.int {
	if uint64(position) > uint64(^uint64(0)>>1) {
		return C.int(-1)
	}
	if _, err := fileFor(handle).Seek(int64(position), io.SeekStart); err != nil {
		return osalError(err)
	}
	return 0
}

//export pixaGoWrite
func pixaGoWrite(_ C.uintptr_t, handle C.uintptr_t, data unsafe.Pointer, length C.size_t, out *C.size_t) C.int {
	if uint64(length) > uint64(^uint(0)>>1) {
		return C.int(-4)
	}
	n, err := fileFor(handle).Write(unsafe.Slice((*byte)(data), int(length)))
	*out = C.size_t(n)
	if err != nil {
		return osalError(err)
	}
	return 0
}

//export pixaGoSync
func pixaGoSync(_ C.uintptr_t, handle C.uintptr_t) C.int {
	if err := fileFor(handle).Sync(); err != nil {
		return osalError(err)
	}
	return 0
}

//export pixaGoClose
func pixaGoClose(_ C.uintptr_t, handle C.uintptr_t) C.int {
	file := fileFor(handle)
	err := file.Close()
	cgo.Handle(handle).Delete()
	if err != nil {
		return osalError(err)
	}
	return 0
}

//export pixaGoRename
func pixaGoRename(handle C.uintptr_t, oldPath, newPath *C.char) C.int {
	fs := filesystemFor(handle)
	oldName, err := fs.path(C.GoString(oldPath))
	if err != nil {
		return osalError(err)
	}
	newName, err := fs.path(C.GoString(newPath))
	if err != nil {
		return osalError(err)
	}
	if err := os.Rename(oldName, newName); err != nil {
		return osalError(err)
	}
	return 0
}

//export pixaGoRemove
func pixaGoRemove(handle C.uintptr_t, path *C.char) C.int {
	name, err := filesystemFor(handle).path(C.GoString(path))
	if err != nil {
		return osalError(err)
	}
	if err := os.Remove(name); err != nil && !errors.Is(err, os.ErrNotExist) {
		return osalError(err)
	}
	return 0
}

func withOSAL(root string, call func(*C.pixa_osal_api_t) C.int) int {
	h := cgo.NewHandle(filesystem{root: root})
	defer h.Delete()
	api := C.pixaGoOSALNew(C.uintptr_t(h))
	defer C.pixaGoOSALFree(api)
	return int(call(api))
}

func mkdir(root, path string) int {
	cpath := C.CString(path)
	defer C.free(unsafe.Pointer(cpath))
	return withOSAL(root, func(api *C.pixa_osal_api_t) C.int { return C.pixaGoOSALMkdir(api, cpath) })
}

func readSecond(root, path string) (int, byte) {
	cpath := C.CString(path)
	defer C.free(unsafe.Pointer(cpath))
	var value C.uint8_t
	status := withOSAL(root, func(api *C.pixa_osal_api_t) C.int {
		return C.pixaGoOSALReadSecond(api, cpath, &value)
	})
	return status, byte(value)
}

func extract(root, path string, data []byte) int {
	cpath := C.CString(path)
	defer C.free(unsafe.Pointer(cpath))
	return withOSAL(root, func(api *C.pixa_osal_api_t) C.int {
		return C.pixaGoExtract(unsafe.Pointer(unsafe.SliceData(data)), C.size_t(len(data)), cpath, api)
	})
}
