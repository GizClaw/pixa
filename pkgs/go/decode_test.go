package pixa

import (
	"bytes"
	"encoding/binary"
	"errors"
	"strings"
	"testing"
)

func TestVisitClipFramesRGBAPaletteKeyAndDiff(t *testing.T) {
	asset := parseDecodeFixture(t, []uint16{0, 0xf800}, []decodeFixtureFrame{
		{encoding: keyEncodingPaletteRLE, payload: []byte{1, 1, 8, 0}},
		{frameType: frameTypeDiff, payload: diffPayload(1, 0, 1, 1, []byte{1, 1})},
		{frameType: frameTypeDiff, payload: diffPayload(1, 1, 1, 1, []byte{1, 1})},
	})
	asset.Clips = []Clip{
		{Name: "first", FirstFrame: 0, FrameCount: 2},
		{Name: "second", FirstFrame: 2, FrameCount: 1},
	}
	asset.ClipCount = uint16(len(asset.Clips))
	out := make([]byte, asset.CanvasRGBABytes())
	var visits []frameVisit
	err := asset.VisitClipFramesRGBA(out, func(clipIndex int, localFrame uint32, rgba []byte) error {
		visits = append(visits, frameVisit{
			clip:    clipIndex,
			frame:   localFrame,
			visible: visiblePixels(rgba),
		})
		return nil
	})
	if err != nil {
		t.Fatal(err)
	}
	want := []frameVisit{
		{clip: 0, frame: 0, visible: 1},
		{clip: 0, frame: 1, visible: 2},
		{clip: 1, frame: 0, visible: 1},
	}
	if len(visits) != len(want) {
		t.Fatalf("visits = %v, want %v", visits, want)
	}
	for index := range want {
		if visits[index] != want[index] {
			t.Fatalf("visit %d = %+v, want %+v", index, visits[index], want[index])
		}
	}
}

func TestDecodeClipFrameRGBAReplaysDiffs(t *testing.T) {
	asset := parseDecodeFixture(t, []uint16{0, 0x07e0}, []decodeFixtureFrame{
		{encoding: keyEncodingPaletteRLE, payload: []byte{9, 0}},
		{frameType: frameTypeDiff, payload: diffPayload(1, 1, 1, 1, []byte{1, 1})},
	})
	out := make([]byte, asset.CanvasRGBABytes())
	if err := asset.DecodeClipFrameRGBA(0, 1, out); err != nil {
		t.Fatal(err)
	}
	center := (1*3 + 1) * rgbaBytesPerPixel
	if got := out[center : center+rgbaBytesPerPixel]; got[0] != 0 || got[1] != 255 || got[2] != 0 || got[3] != 255 {
		t.Fatalf("center RGBA = %v, want opaque green", got)
	}
}

func TestApplyClipFrameRGBARawEncodingsAreOpaque(t *testing.T) {
	tests := []struct {
		name     string
		encoding uint8
	}{
		{name: "legacy inferred", encoding: keyEncodingLegacy},
		{name: "explicit RGB565", encoding: keyEncodingRGB565},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			payload := make([]byte, 3*3*2)
			binary.LittleEndian.PutUint16(payload[2*(1*3+1):], 0xf800)
			asset := parseDecodeFixture(t, nil, []decodeFixtureFrame{{
				encoding: test.encoding,
				payload:  payload,
			}})
			out := make([]byte, asset.CanvasRGBABytes())
			if err := asset.ApplyClipFrameRGBA(0, 0, out); err != nil {
				t.Fatal(err)
			}
			for pixel := 0; pixel < len(out); pixel += rgbaBytesPerPixel {
				if out[pixel+3] != 255 {
					t.Fatalf("pixel %d alpha = %d, want 255", pixel/rgbaBytesPerPixel, out[pixel+3])
				}
			}
			center := (1*3 + 1) * rgbaBytesPerPixel
			if got := out[center : center+rgbaBytesPerPixel]; got[0] != 255 || got[1] != 0 || got[2] != 0 || got[3] != 255 {
				t.Fatalf("center RGBA = %v, want opaque red", got)
			}
		})
	}
}

func TestApplyClipFrameRGBALegacyPaletteRLE(t *testing.T) {
	asset := parseDecodeFixture(t, []uint16{0, 0x001f}, []decodeFixtureFrame{{
		encoding: keyEncodingLegacy,
		payload:  []byte{9, 1},
	}})
	out := make([]byte, asset.CanvasRGBABytes())
	if err := asset.ApplyClipFrameRGBA(0, 0, out); err != nil {
		t.Fatal(err)
	}
	if got := out[:rgbaBytesPerPixel]; got[0] != 0 || got[1] != 0 || got[2] != 255 || got[3] != 255 {
		t.Fatalf("first RGBA = %v, want opaque blue", got)
	}
}

func TestApplyClipFrameRGBAPreservesTrailingOutput(t *testing.T) {
	asset := parseDecodeFixture(t, []uint16{0}, []decodeFixtureFrame{{
		encoding: keyEncodingPaletteRLE,
		payload:  []byte{9, 0},
	}})
	required := int(asset.CanvasRGBABytes())
	out := bytes.Repeat([]byte{0xa5}, required+7)
	if err := asset.ApplyClipFrameRGBA(0, 0, out); err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(out[required:], bytes.Repeat([]byte{0xa5}, 7)) {
		t.Fatalf("trailing output changed: %x", out[required:])
	}
}

func TestApplyClipFrameRGBARejectsMalformedPayloads(t *testing.T) {
	truncatedDiff := diffPayload(0, 0, 1, 1, []byte{1, 0})
	binary.LittleEndian.PutUint32(truncatedDiff[9:13], 3)
	tests := []struct {
		name      string
		palette   []uint16
		frame     decodeFixtureFrame
		wantError string
	}{
		{
			name:      "odd RLE",
			palette:   []uint16{0},
			frame:     decodeFixtureFrame{encoding: keyEncodingPaletteRLE, payload: []byte{9}},
			wantError: "odd length",
		},
		{
			name:      "zero run",
			palette:   []uint16{0},
			frame:     decodeFixtureFrame{encoding: keyEncodingPaletteRLE, payload: []byte{0, 0}},
			wantError: "zero-length run",
		},
		{
			name:      "palette index",
			palette:   []uint16{0},
			frame:     decodeFixtureFrame{encoding: keyEncodingPaletteRLE, payload: []byte{9, 1}},
			wantError: "palette index 1",
		},
		{
			name:      "RLE underflow",
			palette:   []uint16{0},
			frame:     decodeFixtureFrame{encoding: keyEncodingPaletteRLE, payload: []byte{8, 0}},
			wantError: "decodes 8 pixels",
		},
		{
			name:      "RLE overflow",
			palette:   []uint16{0},
			frame:     decodeFixtureFrame{encoding: keyEncodingPaletteRLE, payload: []byte{10, 0}},
			wantError: "expands beyond",
		},
		{
			name:      "truncated explicit RGB565",
			frame:     decodeFixtureFrame{encoding: keyEncodingRGB565, payload: []byte{0, 0}},
			wantError: "RGB565 payload is 2 bytes",
		},
		{
			name:      "unsupported encoding",
			palette:   []uint16{0},
			frame:     decodeFixtureFrame{encoding: 37, payload: []byte{9, 0}},
			wantError: "unsupported key encoding 37",
		},
		{
			name:      "unsupported frame",
			palette:   []uint16{0},
			frame:     decodeFixtureFrame{frameType: 37, payload: []byte{9, 0}},
			wantError: "unsupported frame type 37",
		},
		{
			name:      "empty diff",
			palette:   []uint16{0},
			frame:     decodeFixtureFrame{frameType: frameTypeDiff},
			wantError: "diff payload is empty",
		},
		{
			name:      "truncated diff header",
			palette:   []uint16{0},
			frame:     decodeFixtureFrame{frameType: frameTypeDiff, payload: []byte{1, 0}},
			wantError: "header is truncated",
		},
		{
			name:      "truncated diff payload",
			palette:   []uint16{0},
			frame:     decodeFixtureFrame{frameType: frameTypeDiff, payload: truncatedDiff},
			wantError: "payload is truncated",
		},
		{
			name:    "out of range diff",
			palette: []uint16{0},
			frame: decodeFixtureFrame{
				frameType: frameTypeDiff,
				payload:   diffPayload(3, 0, 1, 1, []byte{1, 0}),
			},
			wantError: "rectangle exceeds the canvas",
		},
		{
			name:      "trailing diff bytes",
			palette:   []uint16{0},
			frame:     decodeFixtureFrame{frameType: frameTypeDiff, payload: append(diffPayload(0, 0, 1, 1, []byte{1, 0}), 0xff)},
			wantError: "trailing bytes",
		},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			asset := parseDecodeFixture(t, test.palette, []decodeFixtureFrame{test.frame})
			out := make([]byte, asset.CanvasRGBABytes())
			err := asset.ApplyClipFrameRGBA(0, 0, out)
			if err == nil || !strings.Contains(err.Error(), test.wantError) {
				t.Fatalf("ApplyClipFrameRGBA() error = %v, want containing %q", err, test.wantError)
			}
		})
	}
}

func TestApplyClipFrameRGBARejectsInvalidReferences(t *testing.T) {
	asset := parseDecodeFixture(t, []uint16{0}, []decodeFixtureFrame{{
		encoding: keyEncodingPaletteRLE,
		payload:  []byte{9, 0},
	}})
	tests := []struct {
		name      string
		asset     Asset
		clip      int
		frame     uint32
		wantError string
	}{
		{name: "negative clip", asset: asset, clip: -1, wantError: "clip -1 does not exist"},
		{name: "missing clip", asset: asset, clip: 1, wantError: "clip 1 does not exist"},
		{name: "missing local frame", asset: asset, clip: 0, frame: 1, wantError: "frame 1 does not exist"},
		{
			name: "missing frame table entry",
			asset: func() Asset {
				invalid := asset
				invalid.Frames = nil
				return invalid
			}(),
			clip:      0,
			wantError: "exceeds the frame table",
		},
		{
			name: "payload beyond current bytes",
			asset: func() Asset {
				invalid := asset
				invalid.Bytes = invalid.Bytes[:len(invalid.Bytes)-1]
				return invalid
			}(),
			clip:      0,
			wantError: "payload exceeds file length",
		},
		{
			name: "palette beyond current bytes",
			asset: func() Asset {
				invalid := asset
				invalid.PaletteOffset = uint32(len(invalid.Bytes))
				return invalid
			}(),
			clip:      0,
			wantError: "palette index 0 exceeds file length",
		},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			err := test.asset.ApplyClipFrameRGBA(test.clip, test.frame, make([]byte, test.asset.CanvasRGBABytes()))
			if err == nil || !strings.Contains(err.Error(), test.wantError) {
				t.Fatalf("ApplyClipFrameRGBA() error = %v, want containing %q", err, test.wantError)
			}
		})
	}
}

func TestDecodeClipFrameRGBARejectsInvalidTargetBeforeMutation(t *testing.T) {
	asset := parseDecodeFixture(t, []uint16{0}, []decodeFixtureFrame{{
		encoding: keyEncodingPaletteRLE,
		payload:  []byte{9, 0},
	}})
	out := bytes.Repeat([]byte{0xa5}, int(asset.CanvasRGBABytes()))
	before := append([]byte(nil), out...)
	err := asset.DecodeClipFrameRGBA(0, 1, out)
	if err == nil || !strings.Contains(err.Error(), "frame 1 does not exist") {
		t.Fatalf("DecodeClipFrameRGBA() error = %v, want missing-frame error", err)
	}
	if !bytes.Equal(out, before) {
		t.Fatal("DecodeClipFrameRGBA mutated output for an invalid target")
	}
}

func TestVisitClipFramesRGBAVisitorErrorStopsTraversal(t *testing.T) {
	asset := parseDecodeFixture(t, []uint16{0}, []decodeFixtureFrame{
		{encoding: keyEncodingPaletteRLE, payload: []byte{9, 0}},
		{encoding: keyEncodingPaletteRLE, payload: []byte{9, 0}},
	})
	want := errors.New("stop")
	calls := 0
	err := asset.VisitClipFramesRGBA(make([]byte, asset.CanvasRGBABytes()), func(_ int, _ uint32, _ []byte) error {
		calls++
		return want
	})
	if !errors.Is(err, want) {
		t.Fatalf("VisitClipFramesRGBA() error = %v, want %v", err, want)
	}
	if !strings.Contains(err.Error(), "visitor for clip 0 frame 0") {
		t.Fatalf("VisitClipFramesRGBA() error = %v, want clip/frame context", err)
	}
	if calls != 1 {
		t.Fatalf("visitor calls = %d, want 1", calls)
	}
}

func TestValidateFramesRGBARejectsSmallOutput(t *testing.T) {
	asset := parseDecodeFixture(t, []uint16{0}, []decodeFixtureFrame{{
		encoding: keyEncodingPaletteRLE,
		payload:  []byte{9, 0},
	}})
	err := asset.ValidateFramesRGBA(make([]byte, asset.CanvasRGBABytes()-1))
	if err == nil || !strings.Contains(err.Error(), "RGBA output") {
		t.Fatalf("ValidateFramesRGBA() error = %v, want output-size error", err)
	}
}

func TestValidateFramesRGBARejectsEmptyClip(t *testing.T) {
	asset := parseDecodeFixture(t, []uint16{0}, nil)
	err := asset.ValidateFramesRGBA(make([]byte, asset.CanvasRGBABytes()))
	if err == nil || !strings.Contains(err.Error(), "contains no frames") {
		t.Fatalf("ValidateFramesRGBA() error = %v, want empty-clip error", err)
	}
}

func TestValidateFramesRGBARejectsNoClips(t *testing.T) {
	asset := parseDecodeFixture(t, []uint16{0}, []decodeFixtureFrame{{
		encoding: keyEncodingPaletteRLE,
		payload:  []byte{9, 0},
	}})
	asset.Clips = nil
	err := asset.ValidateFramesRGBA(make([]byte, asset.CanvasRGBABytes()))
	if err == nil || !strings.Contains(err.Error(), "contains no clips") {
		t.Fatalf("ValidateFramesRGBA() error = %v, want no-clips error", err)
	}
}

func TestParseValidatesPaletteHeaders(t *testing.T) {
	oversizedPalette := make([]uint16, maxPaletteColors+1)
	oversizedPalette[1] = 0xf800
	rawRGB565 := make([]byte, 3*3*2)
	maxPaletteAsset, err := Parse(buildDecodeFixture(
		oversizedPalette[:maxPaletteColors],
		[]decodeFixtureFrame{{
			encoding: keyEncodingPaletteRLE,
			payload:  []byte{9, 1},
		}},
	))
	if err != nil {
		t.Fatalf("Parse() 256-color palette error = %v", err)
	}
	if maxPaletteAsset.ColorCount != maxPaletteColors {
		t.Fatalf("Parse() color count = %d, want %d", maxPaletteAsset.ColorCount, maxPaletteColors)
	}
	noPaletteAsset, err := Parse(buildDecodeFixture(
		nil,
		[]decodeFixtureFrame{{
			encoding: keyEncodingRGB565,
			payload:  rawRGB565,
		}},
	))
	if err != nil {
		t.Fatalf("Parse() empty palette error = %v", err)
	}
	if noPaletteAsset.ColorCount != 0 {
		t.Fatalf("Parse() empty palette color count = %d, want 0", noPaletteAsset.ColorCount)
	}
	tests := []struct {
		name      string
		palette   []uint16
		frame     decodeFixtureFrame
		wantError string
	}{
		{
			name:      "more than 256 colors",
			palette:   oversizedPalette,
			frame:     decodeFixtureFrame{encoding: keyEncodingPaletteRLE, payload: []byte{9, 1}},
			wantError: "color count 257 exceeds 256",
		},
		{
			name:      "nonzero transparent entry with RGB565",
			palette:   []uint16{0x001f},
			frame:     decodeFixtureFrame{encoding: keyEncodingRGB565, payload: rawRGB565},
			wantError: "palette index 0",
		},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			_, err := Parse(buildDecodeFixture(test.palette, []decodeFixtureFrame{test.frame}))
			if err == nil || !strings.Contains(err.Error(), test.wantError) {
				t.Fatalf("Parse() error = %v, want containing %q", err, test.wantError)
			}
		})
	}
}

type frameVisit struct {
	clip    int
	frame   uint32
	visible int
}

type decodeFixtureFrame struct {
	frameType uint8
	encoding  uint8
	payload   []byte
}

func visiblePixels(rgba []byte) int {
	count := 0
	for pixel := 0; pixel < len(rgba); pixel += rgbaBytesPerPixel {
		if rgba[pixel+3] != 0 {
			count++
		}
	}
	return count
}

func parseDecodeFixture(t *testing.T, palette []uint16, frames []decodeFixtureFrame) Asset {
	t.Helper()
	data := buildDecodeFixture(palette, frames)
	asset, err := Parse(data)
	if err != nil {
		t.Fatal(err)
	}
	return asset
}

func buildDecodeFixture(palette []uint16, frames []decodeFixtureFrame) []byte {
	const (
		width          = 3
		height         = 3
		headerSize     = 40
		clipEntrySize  = 56
		frameEntrySize = 16
	)
	paletteOffset := headerSize
	clipOffset := paletteOffset + len(palette)*2
	frameOffset := clipOffset + clipEntrySize
	payloadOffset := frameOffset + len(frames)*frameEntrySize
	payloadLength := 0
	for _, frame := range frames {
		payloadLength += len(frame.payload)
	}
	data := make([]byte, payloadOffset+payloadLength)
	copy(data[:4], Magic)
	binary.LittleEndian.PutUint16(data[4:6], Version)
	binary.LittleEndian.PutUint16(data[6:8], HeaderSize)
	binary.LittleEndian.PutUint16(data[8:10], width)
	binary.LittleEndian.PutUint16(data[10:12], height)
	binary.LittleEndian.PutUint16(data[12:14], uint16(len(palette)))
	binary.LittleEndian.PutUint16(data[14:16], 1)
	binary.LittleEndian.PutUint32(data[16:20], uint32(len(frames)))
	binary.LittleEndian.PutUint32(data[20:24], uint32(paletteOffset))
	binary.LittleEndian.PutUint32(data[24:28], uint32(clipOffset))
	binary.LittleEndian.PutUint32(data[28:32], uint32(frameOffset))
	binary.LittleEndian.PutUint32(data[32:36], uint32(payloadOffset))
	binary.LittleEndian.PutUint32(data[36:40], uint32(payloadLength))
	for index, color := range palette {
		binary.LittleEndian.PutUint16(data[paletteOffset+index*2:], color)
	}
	copy(data[clipOffset:clipOffset+32], "idle")
	binary.LittleEndian.PutUint32(data[clipOffset+36:clipOffset+40], 0)
	binary.LittleEndian.PutUint32(data[clipOffset+40:clipOffset+44], uint32(len(frames)))
	payloadCursor := 0
	for index, frame := range frames {
		base := frameOffset + index*frameEntrySize
		data[base+2] = frame.frameType
		data[base+3] = frame.encoding
		binary.LittleEndian.PutUint32(data[base+4:base+8], uint32(payloadCursor))
		binary.LittleEndian.PutUint32(data[base+8:base+12], uint32(len(frame.payload)))
		copy(data[payloadOffset+payloadCursor:], frame.payload)
		payloadCursor += len(frame.payload)
	}
	return data
}

func diffPayload(x, y, width, height uint16, rle []byte) []byte {
	payload := make([]byte, 1+12+len(rle))
	payload[0] = 1
	binary.LittleEndian.PutUint16(payload[1:3], x)
	binary.LittleEndian.PutUint16(payload[3:5], y)
	binary.LittleEndian.PutUint16(payload[5:7], width)
	binary.LittleEndian.PutUint16(payload[7:9], height)
	binary.LittleEndian.PutUint32(payload[9:13], uint32(len(rle)))
	copy(payload[13:], rle)
	return payload
}
