// Package pixa parses the stable PIXA v1 container header.
package pixa

import (
	"encoding/binary"
	"fmt"
)

const (
	Magic      = "PIXA"
	HeaderSize = 40
	Version    = 1
)

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
}

// Parse validates the PIXA v1 header and its declared table/payload ranges.
func Parse(data []byte) (Asset, error) {
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
	if !rangeOK(len(data), asset.PaletteOffset, uint64(asset.ColorCount)*2) ||
		!rangeOK(len(data), asset.ClipOffset, uint64(asset.ClipCount)*56) ||
		!rangeOK(len(data), asset.FrameOffset, uint64(asset.FrameCount)*16) ||
		!rangeOK(len(data), asset.PayloadOffset, uint64(asset.PayloadLength)) {
		return Asset{}, fmt.Errorf("pixa: table or payload range exceeds file")
	}
	return asset, nil
}

func rangeOK(length int, offset uint32, size uint64) bool {
	return uint64(offset) <= uint64(length) && size <= uint64(length)-uint64(offset)
}
