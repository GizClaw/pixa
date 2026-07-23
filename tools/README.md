# PIXA cooking tools

These repository-level tools convert configured animation source layouts into
PIXA v1 RGB565 key-frame bundles. They require Node.js 24 and FFmpeg.

Convert a regular WebP atlas:

```sh
node tools/webp-to-pixa.mjs atlas.webp output.pixa manifest.json
```

The WebP manifest declares `canvas`, `atlas`, optional `background`, and the
clip slices:

```json
{
  "canvas": { "width": 32, "height": 32 },
  "atlas": { "columns": 4, "rows": 2 },
  "background": "#dcefe8",
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
