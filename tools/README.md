# PIXA cooking tools

These repository-level tools convert configured animation source layouts into
PIXA v1 key-frame bundles. They require Node.js 24 and FFmpeg. The WebP cooker
preserves source transparency with palette RLE; the GIF cooker retains its
legacy background-flattened RGB565 output.

Convert a regular WebP atlas:

```sh
node tools/webp-to-pixa.mjs atlas.webp output.pixa manifest.json
```

The WebP manifest declares `canvas`, `atlas`, optional `alphaThreshold`
(default `128`), `colorQuantization` (currently `rgb-6x7x6`), and the clip
slices. Sprite packages that require visible content inside a transparent
canvas border opt in with
`"frameValidation": "visible-transparent-border"`:

```json
{
  "canvas": { "width": 32, "height": 32 },
  "atlas": { "columns": 4, "rows": 2 },
  "alphaThreshold": 128,
  "colorQuantization": "rgb-6x7x6",
  "frameValidation": "visible-transparent-border",
  "clips": [
    {
      "name": "idle",
      "row": 0,
      "column": 0,
      "frameCount": 4,
      "durationMs": 120,
      "loop": true
    }
  ]
}
```

The cooker scales the complete atlas to the declared cell grid with
nearest-neighbor sampling. Pixels below the alpha threshold become transparent
palette index `0`; other pixels are quantized deterministically into the
palette-safe 6-by-7-by-6 opaque color cube. With the sprite validation mode
shown above, it rejects empty frames and any opaque pixel on the outermost
canvas edge. The validation is opt-in so full-canvas animations remain valid.
Legacy WebP manifests containing `background` are rejected explicitly instead
of silently flattening or ignoring source alpha.

The reusable 25-clip layout for Codex desktop pet atlases is
[`manifests/codex-pet-atlas.json`](manifests/codex-pet-atlas.json). The source
WebP artwork is not stored here and is not covered by this repository's
BSD-3-Clause license. The manifest does not grant permission to copy, modify,
or redistribute that artwork or the cooked pet assets.

Convert a configured collection of GIF clips:

```sh
node tools/gifs-to-pixa.mjs source-directory output.pixa manifest.json
```

The GIF manifest declares the output canvas, optional padding/background and
the source file for every clip:

```json
{
  "canvas": { "width": 60, "height": 60 },
  "padding": 1,
  "background": "#dcefe8",
  "minimumFrameDurationMs": 40,
  "clips": [
    { "name": "idle", "source": "default.gif", "loop": true },
    { "name": "wave", "source": "wave.gif", "loop": false }
  ]
}
```

Use `-` as the manifest path to read JSON from standard input. Both commands
create their output directory when needed and print a JSON summary after a
successful conversion.
