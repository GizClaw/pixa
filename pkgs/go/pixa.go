// Package pixa parses and decodes stable PIXA v1 assets.
package pixa

import (
	"encoding/binary"
	"fmt"
)

const (
	Magic            = "PIXA"
	HeaderSize       = 40
	Version          = 1
	clipSize         = 56
	frameSize        = 16
	maxPaletteColors = 256
)

// Clip is one animation entry in the PIXA clip table.
type Clip struct {
	Name                                    string
	AnchorX, AnchorY                        int16
	FirstFrame, FrameCount, TotalDurationMS uint32
	Loop                                    bool
}

// Frame is one payload reference in the PIXA frame table.
type Frame struct {
	DurationMS                   uint16
	Encoding                     uint8
	Type                         uint8
	PayloadOffset, PayloadLength uint32
}

// ParseLimits bounds table allocation and clip-referenced traversal during
// parsing. A zero field leaves that dimension unrestricted.
type ParseLimits struct {
	MaxClips            uint16
	MaxFrames           uint32
	MaxReferencedFrames uint64
}

// Asset is the validated PIXA v1 container metadata. Table and payload ranges
// are guaranteed to be within Bytes.
type Asset struct {
	Bytes         []byte
	Version       uint16
	Width         uint16
	Height        uint16
	ColorCount    uint16
	ClipCount     uint16
	FrameCount    uint32
	PaletteOffset uint32
	ClipOffset    uint32
	FrameOffset   uint32
	PayloadOffset uint32
	PayloadLength uint32
	Clips         []Clip
	Frames        []Frame
}

// Parse validates the PIXA v1 header and its declared table/payload ranges.
func Parse(data []byte) (Asset, error) {
	return ParseWithLimits(data, ParseLimits{})
}

// ParseWithLimits validates the PIXA v1 container while enforcing caller-owned
// table and traversal limits before allocating the corresponding slices.
func ParseWithLimits(data []byte, limits ParseLimits) (Asset, error) {
	if len(data) < HeaderSize || string(data[:4]) != Magic {
		return Asset{}, fmt.Errorf("pixa: invalid magic or truncated header")
	}
	version := binary.LittleEndian.Uint16(data[4:6])
	if version != Version || binary.LittleEndian.Uint16(data[6:8]) != HeaderSize {
		return Asset{}, fmt.Errorf("pixa: unsupported header version %d", version)
	}
	asset := Asset{
		Bytes:         data,
		Version:       version,
		Width:         binary.LittleEndian.Uint16(data[8:10]),
		Height:        binary.LittleEndian.Uint16(data[10:12]),
		ColorCount:    binary.LittleEndian.Uint16(data[12:14]),
		ClipCount:     binary.LittleEndian.Uint16(data[14:16]),
		FrameCount:    binary.LittleEndian.Uint32(data[16:20]),
		PaletteOffset: binary.LittleEndian.Uint32(data[20:24]),
		ClipOffset:    binary.LittleEndian.Uint32(data[24:28]),
		FrameOffset:   binary.LittleEndian.Uint32(data[28:32]),
		PayloadOffset: binary.LittleEndian.Uint32(data[32:36]),
		PayloadLength: binary.LittleEndian.Uint32(data[36:40]),
	}
	if asset.Width == 0 || asset.Height == 0 {
		return Asset{}, fmt.Errorf("pixa: empty canvas")
	}
	if asset.ColorCount > maxPaletteColors {
		return Asset{}, fmt.Errorf("pixa: palette color count %d exceeds %d", asset.ColorCount, maxPaletteColors)
	}
	if limits.MaxClips != 0 && asset.ClipCount > limits.MaxClips {
		return Asset{}, fmt.Errorf("pixa: clip count %d exceeds limit %d", asset.ClipCount, limits.MaxClips)
	}
	if limits.MaxFrames != 0 && asset.FrameCount > limits.MaxFrames {
		return Asset{}, fmt.Errorf("pixa: frame count %d exceeds limit %d", asset.FrameCount, limits.MaxFrames)
	}
	if !rangeOK(len(data), asset.PaletteOffset, uint64(asset.ColorCount)*2) ||
		!rangeOK(len(data), asset.ClipOffset, uint64(asset.ClipCount)*clipSize) ||
		!rangeOK(len(data), asset.FrameOffset, uint64(asset.FrameCount)*frameSize) ||
		!rangeOK(len(data), asset.PayloadOffset, uint64(asset.PayloadLength)) {
		return Asset{}, fmt.Errorf("pixa: table or payload range exceeds file")
	}
	if asset.ColorCount > 0 && binary.LittleEndian.Uint16(data[asset.PaletteOffset:]) != 0 {
		return Asset{}, fmt.Errorf("pixa: transparent palette index 0 stores a nonzero RGB565 value")
	}
	asset.Clips = make([]Clip, asset.ClipCount)
	var referencedFrames uint64
	for i := range asset.Clips {
		base := int(asset.ClipOffset) + i*clipSize
		first, count := binary.LittleEndian.Uint32(data[base+36:]), binary.LittleEndian.Uint32(data[base+40:])
		if first > asset.FrameCount || count > asset.FrameCount-first {
			return Asset{}, fmt.Errorf("pixa: clip %d frame range exceeds table", i)
		}
		referencedFrames += uint64(count)
		if limits.MaxReferencedFrames != 0 && referencedFrames > limits.MaxReferencedFrames {
			return Asset{}, fmt.Errorf(
				"pixa: referenced frame count %d exceeds limit %d",
				referencedFrames,
				limits.MaxReferencedFrames,
			)
		}
		nameEnd := 0
		for nameEnd < 32 && data[base+nameEnd] != 0 {
			nameEnd++
		}
		if nameEnd == 32 {
			return Asset{}, fmt.Errorf("pixa: clip %d name is not NUL-terminated", i)
		}
		asset.Clips[i] = Clip{
			Name:            string(data[base : base+nameEnd]),
			AnchorX:         int16(binary.LittleEndian.Uint16(data[base+32:])),
			AnchorY:         int16(binary.LittleEndian.Uint16(data[base+34:])),
			FirstFrame:      first,
			FrameCount:      count,
			TotalDurationMS: binary.LittleEndian.Uint32(data[base+44:]),
			Loop:            binary.LittleEndian.Uint16(data[base+48:])&1 != 0,
		}
	}
	asset.Frames = make([]Frame, asset.FrameCount)
	for i := range asset.Frames {
		base := int(asset.FrameOffset) + i*frameSize
		off, size := binary.LittleEndian.Uint32(data[base+4:]), binary.LittleEndian.Uint32(data[base+8:])
		if uint64(off)+uint64(size) > uint64(asset.PayloadLength) {
			return Asset{}, fmt.Errorf("pixa: frame %d payload range exceeds payload", i)
		}
		asset.Frames[i] = Frame{
			DurationMS:    binary.LittleEndian.Uint16(data[base:]),
			Encoding:      data[base+3],
			Type:          data[base+2],
			PayloadOffset: off,
			PayloadLength: size,
		}
	}
	return asset, nil
}

func rangeOK(length int, offset uint32, size uint64) bool {
	return uint64(offset) <= uint64(length) && size <= uint64(length)-uint64(offset)
}
