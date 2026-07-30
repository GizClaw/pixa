package pixa

import (
	"encoding/binary"
	"fmt"
)

const (
	frameTypeKey  = 0
	frameTypeDiff = 1

	keyEncodingLegacy     = 0
	keyEncodingPaletteRLE = 1
	keyEncodingRGB565     = 2

	rgbaBytesPerPixel = 4
)

// CanvasRGBABytes returns the number of bytes required for one decoded RGBA
// canvas. Callers should apply their own resource limit before allocating it.
func (a Asset) CanvasRGBABytes() uint64 {
	return uint64(a.Width) * uint64(a.Height) * rgbaBytesPerPixel
}

// ApplyClipFrameRGBA applies one clip-local frame to out.
//
// Key frames replace the complete canvas. Diff frames update the existing
// canvas, so callers that apply a clip sequentially must start with a cleared
// buffer. The caller retains ownership of out. The function does not allocate
// a canvas and rejects malformed or unsupported frame payloads. Bytes after
// CanvasRGBABytes are left unchanged.
func (a Asset) ApplyClipFrameRGBA(clipIndex int, localFrame uint32, out []byte) error {
	canvas, err := a.rgbaCanvas(out)
	if err != nil {
		return err
	}
	frame, payload, err := a.clipFramePayload(clipIndex, localFrame)
	if err != nil {
		return err
	}

	switch frame.Type {
	case frameTypeKey:
		clear(canvas)
		rawBytes := uint64(a.Width) * uint64(a.Height) * 2
		legacyRGB565 := frame.Encoding == keyEncodingLegacy && uint64(len(payload)) == rawBytes
		switch {
		case frame.Encoding == keyEncodingRGB565 || legacyRGB565:
			if err := a.decodeRGB565(payload, canvas); err != nil {
				return fmt.Errorf("pixa: clip %d frame %d: %w", clipIndex, localFrame, err)
			}
		case frame.Encoding == keyEncodingPaletteRLE || frame.Encoding == keyEncodingLegacy:
			if err := a.decodeRLERect(payload, 0, 0, a.Width, a.Height, canvas); err != nil {
				return fmt.Errorf("pixa: clip %d frame %d: %w", clipIndex, localFrame, err)
			}
		default:
			return fmt.Errorf("pixa: clip %d frame %d uses unsupported key encoding %d", clipIndex, localFrame, frame.Encoding)
		}
	case frameTypeDiff:
		if err := a.applyDiff(payload, canvas); err != nil {
			return fmt.Errorf("pixa: clip %d frame %d: %w", clipIndex, localFrame, err)
		}
	default:
		return fmt.Errorf("pixa: clip %d frame %d uses unsupported frame type %d", clipIndex, localFrame, frame.Type)
	}
	return nil
}

// DecodeClipFrameRGBA decodes one clip-local frame from the start of its clip
// into out. It replays preceding diff frames without allocating a canvas; the
// caller retains ownership of out. Bytes after CanvasRGBABytes are unchanged.
func (a Asset) DecodeClipFrameRGBA(clipIndex int, localFrame uint32, out []byte) error {
	canvas, err := a.rgbaCanvas(out)
	if err != nil {
		return err
	}
	if _, _, err := a.clipFramePayload(clipIndex, localFrame); err != nil {
		return err
	}
	clear(canvas)
	for frame := uint32(0); frame <= localFrame; frame++ {
		if err := a.ApplyClipFrameRGBA(clipIndex, frame, canvas); err != nil {
			return err
		}
	}
	return nil
}

// ValidateFramesRGBA verifies that every clip-referenced frame can be decoded
// in clip-local order. out is caller-owned working storage and must be at least
// CanvasRGBABytes bytes. The method does not allocate a canvas.
func (a Asset) ValidateFramesRGBA(out []byte) error {
	return a.VisitClipFramesRGBA(out, nil)
}

// VisitClipFramesRGBA decodes every referenced frame in clip-local order.
// out remains caller-owned and is reused without allocating a canvas. The same
// borrowed RGBA buffer is passed to visit after each frame and must not be
// retained or modified. Bytes after CanvasRGBABytes are left unchanged.
func (a Asset) VisitClipFramesRGBA(out []byte, visit func(clipIndex int, localFrame uint32, rgba []byte) error) error {
	canvas, err := a.rgbaCanvas(out)
	if err != nil {
		return err
	}
	if len(a.Clips) == 0 {
		return fmt.Errorf("pixa: asset contains no clips")
	}
	for clipIndex, clip := range a.Clips {
		if clip.FrameCount == 0 {
			return fmt.Errorf("pixa: clip %d contains no frames", clipIndex)
		}
		clear(canvas)
		for localFrame := uint32(0); localFrame < clip.FrameCount; localFrame++ {
			if err := a.ApplyClipFrameRGBA(clipIndex, localFrame, canvas); err != nil {
				return err
			}
			if visit != nil {
				if err := visit(clipIndex, localFrame, canvas); err != nil {
					return fmt.Errorf("pixa: visitor for clip %d frame %d: %w", clipIndex, localFrame, err)
				}
			}
		}
	}
	return nil
}

func (a Asset) rgbaCanvas(out []byte) ([]byte, error) {
	required := a.CanvasRGBABytes()
	if uint64(len(out)) < required {
		return nil, fmt.Errorf("pixa: RGBA output is %d bytes, need %d", len(out), required)
	}
	return out[:int(required)], nil
}

func (a Asset) clipFramePayload(clipIndex int, localFrame uint32) (Frame, []byte, error) {
	if clipIndex < 0 || clipIndex >= len(a.Clips) {
		return Frame{}, nil, fmt.Errorf("pixa: clip %d does not exist", clipIndex)
	}
	clip := a.Clips[clipIndex]
	if localFrame >= clip.FrameCount {
		return Frame{}, nil, fmt.Errorf("pixa: clip %d frame %d does not exist", clipIndex, localFrame)
	}
	frameIndex := uint64(clip.FirstFrame) + uint64(localFrame)
	if frameIndex >= uint64(len(a.Frames)) {
		return Frame{}, nil, fmt.Errorf("pixa: clip %d frame %d exceeds the frame table", clipIndex, localFrame)
	}
	frame := a.Frames[frameIndex]
	payload, err := a.framePayload(frame)
	if err != nil {
		return Frame{}, nil, fmt.Errorf("pixa: clip %d frame %d: %w", clipIndex, localFrame, err)
	}
	return frame, payload, nil
}

func (a Asset) framePayload(frame Frame) ([]byte, error) {
	start := uint64(a.PayloadOffset) + uint64(frame.PayloadOffset)
	end := start + uint64(frame.PayloadLength)
	if start > uint64(len(a.Bytes)) || end < start || end > uint64(len(a.Bytes)) {
		return nil, fmt.Errorf("frame payload exceeds file length")
	}
	return a.Bytes[start:end], nil
}

func (a Asset) decodeRGB565(payload, out []byte) error {
	pixels := uint64(a.Width) * uint64(a.Height)
	if uint64(len(payload)) != pixels*2 {
		return fmt.Errorf("RGB565 payload is %d bytes, want %d", len(payload), pixels*2)
	}
	for pixel := range int(pixels) {
		value := binary.LittleEndian.Uint16(payload[pixel*2:])
		writeRGB565RGBA(value, out[pixel*rgbaBytesPerPixel:])
	}
	return nil
}

func (a Asset) decodeRLERect(payload []byte, x, y, width, height uint16, out []byte) error {
	if _, err := a.paletteColor(0); err != nil {
		return err
	}
	if len(payload)%2 != 0 {
		return fmt.Errorf("palette RLE payload has an odd length")
	}
	if uint64(x)+uint64(width) > uint64(a.Width) || uint64(y)+uint64(height) > uint64(a.Height) {
		return fmt.Errorf("palette RLE rectangle exceeds the canvas")
	}
	rectPixels := uint64(width) * uint64(height)
	var decoded uint64
	for offset := 0; offset < len(payload); offset += 2 {
		count := uint64(payload[offset])
		index := payload[offset+1]
		if count == 0 {
			return fmt.Errorf("palette RLE contains a zero-length run")
		}
		if count > rectPixels-decoded {
			return fmt.Errorf("palette RLE expands beyond its rectangle")
		}
		color, err := a.paletteColor(index)
		if err != nil {
			return err
		}
		for range count {
			dx := decoded % uint64(width)
			dy := decoded / uint64(width)
			pixel := (uint64(y)+dy)*uint64(a.Width) + uint64(x) + dx
			target := out[pixel*rgbaBytesPerPixel:]
			if index == 0 {
				clear(target[:rgbaBytesPerPixel])
			} else {
				writeRGB565RGBA(color, target)
			}
			decoded++
		}
	}
	if decoded != rectPixels {
		return fmt.Errorf("palette RLE decodes %d pixels, want %d", decoded, rectPixels)
	}
	return nil
}

func (a Asset) applyDiff(payload, out []byte) error {
	if len(payload) < 1 {
		return fmt.Errorf("diff payload is empty")
	}
	rectCount := int(payload[0])
	offset := 1
	for rect := range rectCount {
		if len(payload)-offset < 12 {
			return fmt.Errorf("diff rectangle %d header is truncated", rect)
		}
		x := binary.LittleEndian.Uint16(payload[offset:])
		y := binary.LittleEndian.Uint16(payload[offset+2:])
		width := binary.LittleEndian.Uint16(payload[offset+4:])
		height := binary.LittleEndian.Uint16(payload[offset+6:])
		length := uint64(binary.LittleEndian.Uint32(payload[offset+8:]))
		offset += 12
		if length > uint64(len(payload)-offset) {
			return fmt.Errorf("diff rectangle %d payload is truncated", rect)
		}
		end := offset + int(length)
		if err := a.decodeRLERect(payload[offset:end], x, y, width, height, out); err != nil {
			return fmt.Errorf("diff rectangle %d: %w", rect, err)
		}
		offset = end
	}
	if offset != len(payload) {
		return fmt.Errorf("diff payload has %d trailing bytes", len(payload)-offset)
	}
	return nil
}

func (a Asset) paletteColor(index uint8) (uint16, error) {
	if uint16(index) >= a.ColorCount {
		return 0, fmt.Errorf("palette index %d exceeds color count %d", index, a.ColorCount)
	}
	offset := uint64(a.PaletteOffset) + uint64(index)*2
	if offset+2 > uint64(len(a.Bytes)) {
		return 0, fmt.Errorf("palette index %d exceeds file length", index)
	}
	color := binary.LittleEndian.Uint16(a.Bytes[offset:])
	if index == 0 && color != 0 {
		return 0, fmt.Errorf("transparent palette index 0 stores nonzero RGB565 value %#04x", color)
	}
	return color, nil
}

func writeRGB565RGBA(color uint16, out []byte) {
	red := uint8((color >> 11) & 0x1f)
	green := uint8((color >> 5) & 0x3f)
	blue := uint8(color & 0x1f)
	out[0] = (red << 3) | (red >> 2)
	out[1] = (green << 2) | (green >> 4)
	out[2] = (blue << 3) | (blue >> 2)
	out[3] = 255
}
