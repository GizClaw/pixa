# PIXA v1 format

PIXA uses little-endian integers. A runtime must reject a file whose declared
range falls outside the supplied byte buffer, whose magic is not `PIXA`, or
whose version is not `1`.

## Header

The fixed header is 40 bytes.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 4 | ASCII magic `PIXA` |
| 4 | 2 | version (`1`) |
| 6 | 2 | header size (`40`) |
| 8 | 2 | canvas width |
| 10 | 2 | canvas height |
| 12 | 2 | palette color count |
| 14 | 2 | clip count |
| 16 | 4 | frame count |
| 20 | 4 | palette offset |
| 24 | 4 | clip table offset |
| 28 | 4 | frame table offset |
| 32 | 4 | payload offset |
| 36 | 4 | payload length |

The palette contains `color_count` RGB565 values. Palette index `0` is
reserved for a fully transparent pixel; its stored RGB565 value must be `0`.
Other palette entries are opaque. The clip table contains `clip_count`
56-byte records; the frame table contains `frame_count` 16-byte records.

## Clip record

The first 32 bytes are a NUL-terminated UTF-8 name. The remaining fields are:

| Offset | Size | Field |
| --- | --- | --- |
| 32 | 2 | signed anchor X |
| 34 | 2 | signed anchor Y |
| 36 | 4 | first frame index |
| 40 | 4 | frame count |
| 44 | 4 | total duration in milliseconds |
| 48 | 2 | flags; bit 0 is loop |

The remaining six bytes are reserved. `first_frame + frame_count` must not
exceed the header frame count.

## Frame record

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 2 | duration in milliseconds |
| 2 | 1 | type: `0` key, `1` diff, other unsupported |
| 3 | 1 | key-frame encoding: `0` legacy inferred, `1` palette RLE, `2` RGB565 |
| 4 | 4 | offset relative to payload offset |
| 8 | 4 | payload length |
| 12 | 4 | reserved |

Each frame payload range must be inside the declared payload. A key frame may
be a row-major RGB565 canvas of `width * height * 2` bytes or a palette-RLE
canvas; the C package supports both. New writers set the key-frame encoding
byte explicitly so a palette-RLE payload that happens to be canvas-sized is
not mistaken for RGB565. For legacy key frames with encoding `0`, C runtimes
infer RGB565 only from a canvas-sized payload and otherwise use palette RLE.
A diff frame describes RLE rectangles on top of the preceding decoded frame.
Consumers that only need container metadata must preserve the frame type and
reject unsupported encodings at render time.

A palette-RLE canvas is a sequence of two-byte `(run_length, palette_index)`
pairs. `run_length` is in the range 1 through 255. Every palette index must be
less than `color_count`, and the sum of the run lengths must equal exactly
`width * height`; truncated, zero-length, underfilled, or overflowing payloads
are invalid. Palette index `0` decodes to transparent regardless of RGB
channels, while every other index decodes to an opaque RGB565 pixel.

PIXA v1 has binary alpha and at most 256 palette entries. The repository
cookers use an alpha threshold of 128 and a deterministic 6-by-7-by-6 opaque
RGB color cube. This bounds a cooked palette to 252 opaque colors plus the
reserved transparent entry without compositing source alpha onto a background.
