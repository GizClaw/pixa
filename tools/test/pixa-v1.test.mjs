import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import test from "node:test";

import {
  encodePaletteRlePixa,
  quantizeRgbaFrame,
  rgb565,
  validateTransparentSpriteFrame,
} from "../lib/pixa-v1.mjs";
import {
  parsePixa,
  renderPixaFrameRGBA,
} from "../../pkgs/typescript/src/index.ts";

test("palette RLE cooking preserves binary transparency deterministically", () => {
  const rgba = Buffer.alloc(3 * 3 * 4);
  rgba.set([255, 0, 0, 255], (1 * 3 + 1) * 4);
  rgba.set([0, 255, 0, 127], 0);
  const pixels = quantizeRgbaFrame(rgba, 3, 3);
  validateTransparentSpriteFrame(pixels, 3, 3);
  const input = {
    width: 3,
    height: 3,
    clips: [
      {
        name: "idle",
        firstFrame: 0,
        frameCount: 1,
        totalDurationMs: 120,
        loop: true,
      },
    ],
    frames: [{ pixels, durationMs: 120 }],
  };

  const first = encodePaletteRlePixa(input);
  const second = encodePaletteRlePixa(input);
  assert.deepEqual(first, second);
  const asset = parsePixa(first);
  assert.equal(asset.colorCount, 2);
  assert.equal(asset.frames[0]?.encoding, 1);
  const rendered = renderPixaFrameRGBA(asset, 0);
  assert.equal(rendered.data[3], 0);
  assert.equal(rendered.data[(1 * 3 + 1) * 4 + 3], 255);
});

test("rgb-6x7x6 quantization fits the PIXA v1 palette", () => {
  const colors = [];
  for (let red = 0; red < 6; red += 1) {
    for (let green = 0; green < 7; green += 1) {
      for (let blue = 0; blue < 6; blue += 1) {
        colors.push(
          rgb565(
            Math.round((red * 255) / 5),
            Math.round((green * 255) / 6),
            Math.round((blue * 255) / 5),
          ),
        );
      }
    }
  }
  const bytes = encodePaletteRlePixa({
    width: colors.length,
    height: 1,
    clips: [
      {
        name: "idle",
        firstFrame: 0,
        frameCount: 1,
        totalDurationMs: 1,
        loop: false,
      },
    ],
    frames: [{ pixels: Int32Array.from(colors), durationMs: 1 }],
  });
  assert.equal(bytes.readUInt16LE(12), 253);
});

test("transparent sprite validation rejects empty and opaque-edge frames", () => {
  const empty = new Int32Array(9);
  empty.fill(-1);
  assert.throws(
    () => validateTransparentSpriteFrame(empty, 3, 3),
    /fully transparent/,
  );

  for (const pixel of [0, 2, 6, 8]) {
    const pixels = new Int32Array(9);
    pixels.fill(-1);
    pixels[4] = 0xf800;
    pixels[pixel] = 0xf800;
    assert.throws(
      () => validateTransparentSpriteFrame(pixels, 3, 3),
      /edge pixel/,
    );
  }
});

test("WebP cooking rejects legacy background flattening manifests", () => {
  const result = spawnSync(
    process.execPath,
    [
      new URL("../webp-to-pixa.mjs", import.meta.url).pathname,
      "missing.webp",
      "missing.pixa",
      "-",
    ],
    {
      encoding: "utf8",
      input: JSON.stringify({ background: "#ffffff" }),
    },
  );
  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /manifest\.background is unsupported/);
});

test("WebP cooking rejects unknown frame validation modes", () => {
  const result = spawnSync(
    process.execPath,
    [
      new URL("../webp-to-pixa.mjs", import.meta.url).pathname,
      "missing.webp",
      "missing.pixa",
      "-",
    ],
    {
      encoding: "utf8",
      input: JSON.stringify({
        atlas: { columns: 1, rows: 1 },
        canvas: { height: 1, width: 1 },
        clips: [
          {
            durationMs: 1,
            frameCount: 1,
            name: "idle",
            row: 0,
          },
        ],
        frameValidation: "unknown",
      }),
    },
  );
  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /frameValidation must be/);
});
