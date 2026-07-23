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

The palette contains `color_count` RGB565 values. The clip table contains
`clip_count` 56-byte records; the frame table contains `frame_count` 16-byte
records.

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
| 3 | 1 | reserved |
| 4 | 4 | offset relative to payload offset |
| 8 | 4 | payload length |
| 12 | 4 | reserved |

Each frame payload range must be inside the declared payload. A key frame may
be a row-major RGB565 canvas of `width * height * 2` bytes or a palette-RLE
canvas; the C package supports both. A diff frame describes RLE rectangles on
top of the preceding decoded frame. Consumers that only need container
metadata must preserve the frame type and reject unsupported encodings at
render time.
