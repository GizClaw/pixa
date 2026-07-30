package pixa

import (
	"encoding/binary"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestParseSharedAsset(t *testing.T) {
	data, err := os.ReadFile(filepath.Join("..", "..", "assets", "codex-pets", "dewey.pixa"))
	if err != nil {
		t.Fatal(err)
	}
	asset, err := Parse(data)
	if err != nil {
		t.Fatal(err)
	}
	if asset.Width == 0 || asset.Height == 0 || asset.ClipCount == 0 || asset.FrameCount == 0 || len(asset.Clips) != int(asset.ClipCount) || len(asset.Frames) != int(asset.FrameCount) {
		t.Fatalf("invalid asset metadata: %+v", asset)
	}
	if asset.Clips[0].Name != "idle" || asset.Clips[0].FrameCount == 0 || asset.Frames[0].PayloadLength == 0 {
		t.Fatalf("invalid parsed tables: clip=%+v frame=%+v", asset.Clips[0], asset.Frames[0])
	}
}

func TestParseCommittedAssets(t *testing.T) {
	assetsRoot := filepath.Join("..", "..", "assets")
	err := filepath.WalkDir(assetsRoot, func(path string, entry os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if entry.IsDir() || filepath.Ext(path) != ".pixa" {
			return nil
		}
		t.Run(filepath.ToSlash(path), func(t *testing.T) {
			data, err := os.ReadFile(path)
			if err != nil {
				t.Fatal(err)
			}
			asset, err := Parse(data)
			if err != nil {
				t.Fatal(err)
			}
			if asset.ClipCount == 0 || asset.FrameCount == 0 {
				t.Fatalf("asset has no animation data: %+v", asset)
			}
			canvasBytes := asset.CanvasRGBABytes()
			if canvasBytes > 16<<20 {
				t.Fatalf("decoded canvas is %d bytes, exceeds test limit", canvasBytes)
			}
			if err := asset.ValidateFramesRGBA(make([]byte, canvasBytes)); err != nil {
				t.Fatalf("asset frame validation failed: %v", err)
			}
		})
		return nil
	})
	if err != nil {
		t.Fatal(err)
	}
}

func TestParseWithLimitsRejectsCountsBeforeTableAllocation(t *testing.T) {
	tests := []struct {
		name       string
		clipCount  uint16
		frameCount uint32
		limits     ParseLimits
		want       string
	}{
		{
			name:      "clips",
			clipCount: 257,
			limits:    ParseLimits{MaxClips: 256},
			want:      "clip count 257 exceeds limit 256",
		},
		{
			name:       "frames",
			frameCount: 4097,
			limits:     ParseLimits{MaxFrames: 4096},
			want:       "frame count 4097 exceeds limit 4096",
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			data := makeHeaderOnlyAsset(tt.clipCount, tt.frameCount)
			_, err := ParseWithLimits(data, tt.limits)
			if err == nil || !strings.Contains(err.Error(), tt.want) {
				t.Fatalf("ParseWithLimits() error = %v, want %q", err, tt.want)
			}
		})
	}
}

func TestParseWithLimitsRejectsReferencedFramesDuringClipParsing(t *testing.T) {
	const (
		clipCount  = 2
		frameCount = 3
	)
	data := make([]byte, HeaderSize+clipCount*clipSize+frameCount*frameSize)
	copy(data[:4], Magic)
	binary.LittleEndian.PutUint16(data[4:6], Version)
	binary.LittleEndian.PutUint16(data[6:8], HeaderSize)
	binary.LittleEndian.PutUint16(data[8:10], 1)
	binary.LittleEndian.PutUint16(data[10:12], 1)
	binary.LittleEndian.PutUint16(data[14:16], clipCount)
	binary.LittleEndian.PutUint32(data[16:20], frameCount)
	binary.LittleEndian.PutUint32(data[20:24], HeaderSize)
	binary.LittleEndian.PutUint32(data[24:28], HeaderSize)
	binary.LittleEndian.PutUint32(data[28:32], HeaderSize+clipCount*clipSize)
	binary.LittleEndian.PutUint32(data[32:36], uint32(len(data)))
	for i := range clipCount {
		base := HeaderSize + i*clipSize
		binary.LittleEndian.PutUint32(data[base+40:base+44], frameCount)
	}

	_, err := ParseWithLimits(data, ParseLimits{MaxReferencedFrames: 5})
	if err == nil || !strings.Contains(err.Error(), "referenced frame count 6 exceeds limit 5") {
		t.Fatalf("ParseWithLimits() error = %v, want referenced-frame limit", err)
	}
}

func makeHeaderOnlyAsset(clipCount uint16, frameCount uint32) []byte {
	data := make([]byte, HeaderSize)
	copy(data[:4], Magic)
	binary.LittleEndian.PutUint16(data[4:6], Version)
	binary.LittleEndian.PutUint16(data[6:8], HeaderSize)
	binary.LittleEndian.PutUint16(data[8:10], 1)
	binary.LittleEndian.PutUint16(data[10:12], 1)
	binary.LittleEndian.PutUint16(data[14:16], clipCount)
	binary.LittleEndian.PutUint32(data[16:20], frameCount)
	return data
}

func TestCodexPetAssetsPreserveTransparentBorders(t *testing.T) {
	paths, err := filepath.Glob(filepath.Join("..", "..", "assets", "codex-pets", "*.pixa"))
	if err != nil {
		t.Fatal(err)
	}
	if len(paths) != 9 {
		t.Fatalf("Codex pet asset count = %d, want 9", len(paths))
	}
	expectedClips := []struct {
		name       string
		frameCount uint32
		duration   uint32
		loop       bool
	}{
		{"idle", 6, 1080, true},
		{"running-right", 8, 800, true},
		{"running-left", 8, 800, true},
		{"waving", 4, 600, false},
		{"jumping", 5, 600, false},
		{"failed", 8, 1200, false},
		{"waiting", 6, 1080, true},
		{"running", 6, 840, true},
		{"review", 6, 1080, true},
		{"look-000", 1, 1000, false},
		{"look-022.5", 1, 1000, false},
		{"look-045", 1, 1000, false},
		{"look-067.5", 1, 1000, false},
		{"look-090", 1, 1000, false},
		{"look-112.5", 1, 1000, false},
		{"look-135", 1, 1000, false},
		{"look-157.5", 1, 1000, false},
		{"look-180", 1, 1000, false},
		{"look-202.5", 1, 1000, false},
		{"look-225", 1, 1000, false},
		{"look-247.5", 1, 1000, false},
		{"look-270", 1, 1000, false},
		{"look-292.5", 1, 1000, false},
		{"look-315", 1, 1000, false},
		{"look-337.5", 1, 1000, false},
	}

	for _, path := range paths {
		t.Run(filepath.Base(path), func(t *testing.T) {
			data, err := os.ReadFile(path)
			if err != nil {
				t.Fatal(err)
			}
			asset, err := Parse(data)
			if err != nil {
				t.Fatal(err)
			}
			if asset.Width != 96 || asset.Height != 104 || asset.ClipCount != 25 || asset.FrameCount != 73 {
				t.Fatalf("metadata = %dx%d clips=%d frames=%d", asset.Width, asset.Height, asset.ClipCount, asset.FrameCount)
			}
			if asset.ColorCount < 2 || asset.ColorCount > 253 {
				t.Fatalf("palette color count = %d, want 2..253", asset.ColorCount)
			}
			if got := binary.LittleEndian.Uint16(data[asset.PaletteOffset:]); got != 0 {
				t.Fatalf("palette index 0 = %#x, want 0", got)
			}
			var firstFrame uint32
			for index, expected := range expectedClips {
				clip := asset.Clips[index]
				if clip.Name != expected.name || clip.FirstFrame != firstFrame ||
					clip.FrameCount != expected.frameCount ||
					clip.TotalDurationMS != expected.duration ||
					clip.AnchorX != 48 || clip.AnchorY != 52 || clip.Loop != expected.loop {
					t.Fatalf("clip %d = %+v, expected %+v at frame %d", index, clip, expected, firstFrame)
				}
				firstFrame += expected.frameCount
			}
			for frameIndex, frame := range asset.Frames {
				if frame.Type != 0 || frame.Encoding != 1 {
					t.Fatalf("frame %d type=%d encoding=%d, want palette-RLE key", frameIndex, frame.Type, frame.Encoding)
				}
				indices, err := decodePaletteRLE(asset, frame)
				if err != nil {
					t.Fatalf("frame %d: %v", frameIndex, err)
				}
				if err := validateVisibleTransparentBorder(indices, int(asset.Width), int(asset.Height)); err != nil {
					t.Fatalf("frame %d: %v", frameIndex, err)
				}
			}
		})
	}
}

func decodePaletteRLE(asset Asset, frame Frame) ([]byte, error) {
	start := int(asset.PayloadOffset + frame.PayloadOffset)
	end := start + int(frame.PayloadLength)
	payload := asset.Bytes[start:end]
	if len(payload)%2 != 0 {
		return nil, fmt.Errorf("truncated RLE pair")
	}
	pixels := make([]byte, int(asset.Width)*int(asset.Height))
	cursor := 0
	for offset := 0; offset < len(payload); offset += 2 {
		runLength := int(payload[offset])
		paletteIndex := payload[offset+1]
		if runLength == 0 {
			return nil, fmt.Errorf("zero-length RLE run")
		}
		if uint16(paletteIndex) >= asset.ColorCount {
			return nil, fmt.Errorf("palette index %d exceeds %d colors", paletteIndex, asset.ColorCount)
		}
		if runLength > len(pixels)-cursor {
			return nil, fmt.Errorf("RLE overflows canvas")
		}
		for range runLength {
			pixels[cursor] = paletteIndex
			cursor++
		}
	}
	if cursor != len(pixels) {
		return nil, fmt.Errorf("RLE fills %d of %d pixels", cursor, len(pixels))
	}
	return pixels, nil
}

func validateVisibleTransparentBorder(pixels []byte, width, height int) error {
	visible := false
	for _, paletteIndex := range pixels {
		visible = visible || paletteIndex != 0
	}
	if !visible {
		return fmt.Errorf("frame is fully transparent")
	}
	for x := range width {
		if pixels[x] != 0 {
			return fmt.Errorf("top edge pixel %d is opaque", x)
		}
		if pixels[(height-1)*width+x] != 0 {
			return fmt.Errorf("bottom edge pixel %d is opaque", x)
		}
	}
	for y := range height {
		if pixels[y*width] != 0 {
			return fmt.Errorf("left edge pixel %d is opaque", y)
		}
		if pixels[y*width+width-1] != 0 {
			return fmt.Errorf("right edge pixel %d is opaque", y)
		}
	}
	return nil
}
