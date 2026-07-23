package cgo

import (
	"encoding/binary"
	"os"
	"path/filepath"
	"testing"
)

func TestGoFilesystemBacksOSAL(t *testing.T) {
	root := t.TempDir()
	if got := mkdir(root, "frames"); got != 0 {
		t.Fatalf("mkdir = %d", got)
	}
	if _, err := os.Stat(filepath.Join(root, "frames")); err != nil {
		t.Fatal(err)
	}
	if got := mkdir(root, "../outside"); got == 0 {
		t.Fatal("mkdir accepted a path outside the filesystem root")
	}
	if err := os.WriteFile(filepath.Join(root, "source.bin"), []byte{0x11, 0x22}, 0o644); err != nil {
		t.Fatal(err)
	}
	if status, value := readSecond(root, "source.bin"); status != 0 || value != 0x22 {
		t.Fatalf("read second = status %d, value %#x", status, value)
	}
}

func TestGoFilesystemExtractsPIXA(t *testing.T) {
	root := t.TempDir()
	if got := extract(root, "sample", samplePIXA()); got != 0 {
		t.Fatalf("extract = %d", got)
	}
	for _, path := range []string{
		filepath.Join(root, "sample", "index.bin"),
		filepath.Join(root, "sample", "clips", "idle.argb4444"),
	} {
		info, err := os.Stat(path)
		if err != nil {
			t.Fatal(err)
		}
		if info.Size() == 0 {
			t.Fatalf("%s is empty", path)
		}
	}
}

func TestPortableCCore(t *testing.T) {
	for name, status := range coreTestResults() {
		if status != 0 {
			t.Errorf("%s test exited with status %d", name, status)
		}
	}
}

func samplePIXA() []byte {
	const (
		headerOffset  = 40
		paletteOffset = headerOffset
		clipOffset    = paletteOffset + 2
		frameOffset   = clipOffset + 56
		payloadOffset = frameOffset + 16
	)
	data := make([]byte, payloadOffset+4)
	copy(data, "PIXA")
	put16 := func(offset int, value uint16) { binary.LittleEndian.PutUint16(data[offset:], value) }
	put32 := func(offset int, value uint32) { binary.LittleEndian.PutUint32(data[offset:], value) }
	put16(4, 1)
	put16(6, headerOffset)
	put16(8, 2)
	put16(10, 1)
	put16(12, 1)
	put16(14, 1)
	put32(16, 1)
	put32(20, paletteOffset)
	put32(24, clipOffset)
	put32(28, frameOffset)
	put32(32, payloadOffset)
	put32(36, 4)
	copy(data[clipOffset:], "idle")
	put32(clipOffset+40, 1)
	put32(clipOffset+44, 100)
	put16(clipOffset+48, 1)
	put16(frameOffset, 100)
	put32(frameOffset+8, 4)
	copy(data[payloadOffset:], []byte{0x00, 0xf8, 0x1f, 0x00})
	return data
}
