package cgo

import (
	"encoding/binary"
	"fmt"
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
	data := samplePIXA()
	binary.LittleEndian.PutUint16(data[98:], 0)
	if got := extract(root, "sample", data); got != 0 {
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

func TestGoFilesystemPacksTransparentPaletteRLE(t *testing.T) {
	root := t.TempDir()
	const clipID = "look-022.5"
	if got := extract(root, "source", samplePIXAWithClip(clipID)); got != 0 {
		t.Fatalf("extract = %d", got)
	}
	framesPath := filepath.Join(root, "source", "clips", clipID+".argb4444")
	if err := os.WriteFile(framesPath, []byte{0x00, 0x00, 0x00, 0xff}, 0o644); err != nil {
		t.Fatal(err)
	}
	if got := pack(root, "source", "packed.pixa", clipID); got != 0 {
		t.Fatalf("pack = %d", got)
	}
	packed, err := os.ReadFile(filepath.Join(root, "packed.pixa"))
	if err != nil {
		t.Fatal(err)
	}
	frameOffset := binary.LittleEndian.Uint32(packed[28:32])
	paletteOffset := binary.LittleEndian.Uint32(packed[20:24])
	if packed[frameOffset+3] != 1 {
		t.Fatalf("key frame encoding = %d, want palette RLE", packed[frameOffset+3])
	}
	if got := binary.LittleEndian.Uint16(packed[paletteOffset:]); got != 0 {
		t.Fatalf("palette index 0 = %#x, want transparent zero", got)
	}
	if got := extract(root, "roundtrip", packed); got != 0 {
		t.Fatalf("roundtrip extract = %d", got)
	}
	roundtrip, err := os.ReadFile(filepath.Join(root, "roundtrip", "clips", clipID+".argb4444"))
	if err != nil {
		t.Fatal(err)
	}
	if got := binary.LittleEndian.Uint16(roundtrip[:2]); got != 0 {
		t.Fatalf("transparent pixel = %#x, want 0", got)
	}
	if got := binary.LittleEndian.Uint16(roundtrip[2:4]); got>>12 != 0xf {
		t.Fatalf("opaque pixel = %#x, want alpha 0xf", got)
	}
}

func TestCExtractsCommittedCodexPetFrames(t *testing.T) {
	data, err := os.ReadFile(filepath.Join("..", "..", "..", "..", "assets", "codex-pets", "dewey.pixa"))
	if err != nil {
		t.Fatal(err)
	}
	if len(data) < 4 || string(data[:4]) != "PIXA" {
		t.Fatal("dewey.pixa is not hydrated from Git LFS")
	}
	root := t.TempDir()
	if got := extract(root, "dewey", data); got != 0 {
		t.Fatalf("extract = %d", got)
	}
	paths, err := filepath.Glob(filepath.Join(root, "dewey", "clips", "*.argb4444"))
	if err != nil {
		t.Fatal(err)
	}
	if len(paths) != 25 {
		t.Fatalf("extracted clip count = %d, want 25", len(paths))
	}
	const width, height = 96, 104
	const frameBytes = width * height * 2
	totalFrames := 0
	for _, path := range paths {
		frames, err := os.ReadFile(path)
		if err != nil {
			t.Fatal(err)
		}
		if len(frames) == 0 || len(frames)%frameBytes != 0 {
			t.Fatalf("%s has invalid frame bytes %d", path, len(frames))
		}
		for offset := 0; offset < len(frames); offset += frameBytes {
			if err := validateARGB4444SpriteFrame(frames[offset:offset+frameBytes], width, height); err != nil {
				t.Fatalf("%s frame %d: %v", path, offset/frameBytes, err)
			}
			totalFrames++
		}
	}
	if totalFrames != 73 {
		t.Fatalf("extracted frame count = %d, want 73", totalFrames)
	}
	if _, err := os.Stat(filepath.Join(root, "dewey", "clips", "look-022.5.argb4444")); err != nil {
		t.Fatal(err)
	}
}

func validateARGB4444SpriteFrame(frame []byte, width, height int) error {
	alpha := func(pixel int) byte {
		return frame[pixel*2+1] >> 4
	}
	visible := false
	for pixel := range width * height {
		visible = visible || alpha(pixel) != 0
	}
	if !visible {
		return fmt.Errorf("frame is fully transparent")
	}
	for x := range width {
		if alpha(x) != 0 {
			return fmt.Errorf("top edge pixel %d is opaque", x)
		}
		if alpha((height-1)*width+x) != 0 {
			return fmt.Errorf("bottom edge pixel %d is opaque", x)
		}
	}
	for y := range height {
		if alpha(y*width) != 0 {
			return fmt.Errorf("left edge pixel %d is opaque", y)
		}
		if alpha(y*width+width-1) != 0 {
			return fmt.Errorf("right edge pixel %d is opaque", y)
		}
	}
	return nil
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

func samplePIXAWithClip(name string) []byte {
	data := samplePIXA()
	const clipOffset = 42
	clear(data[clipOffset : clipOffset+32])
	copy(data[clipOffset:], name)
	return data
}
