import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

import {
  parsePixa,
  pixaClipFrameIndex,
  renderPixaFrameRGBA,
  selectPixaClip,
  validatePixa,
  PixaParseError,
} from "../src/index.ts";

test("parsePixa reads a valid PIXA header, clips, and frames", () => {
  const asset = parsePixa(
    makePixa({ clips: ["idle", "feed"], height: 96, width: 120 }),
  );

  assert.equal(asset.version, 1);
  assert.deepEqual(asset.canvas, {
    height: 96,
    pixelCount: 11520,
    rgb565ByteCount: 23040,
    width: 120,
  });
  assert.equal(asset.colorCount, 1);
  assert.equal(asset.clipCount, 2);
  assert.equal(asset.frameCount, 1);
  assert.equal(asset.payloadLength, 4);
  assert.deepEqual(
    asset.clips.map((clip) => ({
      frameCount: clip.frameCount,
      loop: clip.loop,
      name: clip.name,
      totalDurationMs: clip.totalDurationMs,
    })),
    [
      { frameCount: 1, loop: true, name: "idle", totalDurationMs: 120 },
      { frameCount: 1, loop: true, name: "feed", totalDurationMs: 120 },
    ],
  );
  assert.deepEqual(asset.frames, [
    {
      durationMs: 120,
      encoding: 0,
      payloadLength: 4,
      payloadOffset: 0,
      type: "key",
      typeCode: 0,
    },
  ]);
});

test("parsePixa reads the committed shared asset", () => {
  const asset = parsePixa(
    readFileSync(
      new URL("../../../assets/codex-pets/dewey.pixa", import.meta.url),
    ),
  );
  assert.ok(asset.canvas.width > 0);
  assert.ok(asset.canvas.height > 0);
  assert.ok(asset.clipCount > 0);
  assert.ok(asset.frameCount > 0);
});

test("parsePixa accepts typed array views with byte offsets", () => {
  const pixa = makePixa({ clips: ["idle"], height: 16, width: 16 });
  const padded = new Uint8Array(pixa.byteLength + 8);
  padded.set(new Uint8Array(pixa), 4);

  const asset = parsePixa(padded.subarray(4, 4 + pixa.byteLength));

  assert.equal(asset.canvas.width, 16);
  assert.equal(asset.clips[0]?.name, "idle");
});

test("parsePixa rejects invalid files", () => {
  assert.throws(() => parsePixa(new Uint8Array()), PixaParseError);
  assert.throws(
    () => parsePixa(new TextEncoder().encode("ZPET")),
    PixaParseError,
  );
  assert.throws(
    () => parsePixa(makePixa({ clips: ["idle"], width: 0 })),
    /canvas size/,
  );
  assert.throws(
    () =>
      parsePixa(
        makePixa({
          clips: ["idle"],
          mutate(data) {
            new DataView(data).setUint16(4, 2, true);
          },
        }),
      ),
    /Unsupported PIXA version/,
  );
  assert.throws(
    () =>
      parsePixa(
        makePixa({
          clips: ["idle"],
          mutate(data) {
            new DataView(data).setUint32(36, 9999, true);
          },
        }),
      ),
    /payload range/,
  );

  const oversizedPalette = Array<number>(257).fill(0);
  oversizedPalette[1] = 0xf800;
  assert.equal(
    parsePixa(
      makePixa({
        clips: ["idle"],
        encoding: 1,
        height: 1,
        palette: oversizedPalette.slice(0, 256),
        payload: [1, 1],
        width: 1,
      }),
    ).colorCount,
    256,
  );
  assert.throws(
    () =>
      parsePixa(
        makePixa({
          clips: ["idle"],
          encoding: 1,
          height: 1,
          palette: oversizedPalette,
          payload: [1, 1],
          width: 1,
        }),
      ),
    /at most 256/,
  );
  assert.throws(
    () =>
      parsePixa(
        makePixa({
          clips: ["idle"],
          encoding: 2,
          height: 1,
          palette: [0x001f],
          payload: [0x00, 0xf8, 0x1f, 0x00],
          width: 2,
        }),
      ),
    /index 0/,
  );
  assert.equal(
    parsePixa(makePixa({ clips: ["idle"], palette: [] })).colorCount,
    0,
  );
});

test("validatePixa enforces PetDef and BadgeDef clip contracts", () => {
  assert.equal(
    validatePixa(makePixa({ clips: ["idle", "feed"] }), "petdef").clips.length,
    2,
  );
  assert.equal(
    validatePixa(makePixa({ clips: ["icon"] }), "badgedef").clips[0]?.name,
    "icon",
  );
  assert.throws(
    () => validatePixa(makePixa({ clips: ["feed"] }), "petdef"),
    /idle/,
  );
  assert.throws(
    () => validatePixa(makePixa({ clips: ["idle"] }), "badgedef"),
    /icon/,
  );
});

test("selectPixaClip and pixaClipFrameIndex choose stable animation frames", () => {
  const asset = parsePixa(
    makePixa({ clips: ["idle", "bath"], height: 1, width: 2 }),
  );

  assert.equal(selectPixaClip(asset, "bath")?.name, "bath");
  assert.equal(selectPixaClip(asset, "missing")?.name, "idle");
  const clip = selectPixaClip(asset, "bath");
  assert.ok(clip);
  assert.equal(pixaClipFrameIndex(asset, clip, 0), 0);
  assert.equal(pixaClipFrameIndex(asset, clip, 250), 0);

  const timedClip = { ...clip, frameCount: 2, totalDurationMs: 1000 };
  const timedAsset = {
    ...asset,
    frames: [
      { ...asset.frames[0]!, durationMs: 100 },
      { ...asset.frames[0]!, durationMs: 900 },
    ],
  };
  assert.equal(pixaClipFrameIndex(timedAsset, timedClip, 99), 0);
  assert.equal(pixaClipFrameIndex(timedAsset, timedClip, 100), 1);
  assert.equal(pixaClipFrameIndex(timedAsset, timedClip, 999), 1);
});

test("renderPixaFrameRGBA decodes RGB565 key frames", () => {
  const asset = parsePixa(makePixa({ clips: ["idle"], height: 1, width: 2 }));
  const frame = renderPixaFrameRGBA(asset, 0);

  assert.equal(frame.width, 2);
  assert.equal(frame.height, 1);
  assert.deepEqual(Array.from(frame.data), [255, 0, 0, 255, 0, 255, 0, 255]);
});

test("renderPixaFrameRGBA decodes transparent palette RLE key frames", () => {
  const asset = parsePixa(
    makePixa({
      clips: ["idle"],
      encoding: 1,
      height: 1,
      palette: [0, 0xf800],
      payload: [1, 0, 1, 1],
      width: 2,
    }),
  );
  const frame = renderPixaFrameRGBA(asset, 0);

  assert.deepEqual(Array.from(frame.data), [0, 0, 0, 0, 255, 0, 0, 255]);
});

test("renderPixaFrameRGBA rejects malformed palette RLE", () => {
  const malformed = [
    { payload: [1], pattern: /payload length/ },
    { payload: [0, 0], pattern: /zero-length/ },
    { payload: [2, 2], pattern: /exceeds the palette/ },
    { payload: [1, 1], pattern: /does not fill/ },
    { payload: [3, 1], pattern: /exceeds the frame canvas/ },
  ];
  for (const { payload, pattern } of malformed) {
    const asset = parsePixa(
      makePixa({
        clips: ["idle"],
        encoding: 1,
        height: 1,
        palette: [0, 0xf800],
        payload,
        width: 2,
      }),
    );
    assert.throws(() => renderPixaFrameRGBA(asset, 0), pattern);
  }
});

type MakePixaOptions = {
  clips: string[];
  encoding?: number;
  height?: number;
  mutate?: (data: ArrayBuffer) => void;
  palette?: number[];
  payload?: number[];
  width?: number;
};

function makePixa({
  clips,
  encoding = 0,
  height = 16,
  mutate,
  palette = [0],
  payload = [0x00, 0xf8, 0xe0, 0x07],
  width = 16,
}: MakePixaOptions): ArrayBuffer {
  const headerSize = 40;
  const clipEntrySize = 56;
  const frameEntrySize = 16;
  const paletteOffset = headerSize;
  const clipOffset = paletteOffset + palette.length * 2;
  const frameOffset = clipOffset + clips.length * clipEntrySize;
  const payloadOffset = frameOffset + frameEntrySize;
  const payloadLength = payload.length;
  const data = new ArrayBuffer(payloadOffset + payloadLength);
  const bytes = new Uint8Array(data);
  const view = new DataView(data);

  bytes.set(new TextEncoder().encode("PIXA"), 0);
  view.setUint16(4, 1, true);
  view.setUint16(6, headerSize, true);
  view.setUint16(8, width, true);
  view.setUint16(10, height, true);
  view.setUint16(12, palette.length, true);
  view.setUint16(14, clips.length, true);
  view.setUint32(16, 1, true);
  view.setUint32(20, paletteOffset, true);
  view.setUint32(24, clipOffset, true);
  view.setUint32(28, frameOffset, true);
  view.setUint32(32, payloadOffset, true);
  view.setUint32(36, payloadLength, true);

  for (let i = 0; i < palette.length; i += 1) {
    view.setUint16(paletteOffset + i * 2, palette[i] ?? 0, true);
  }

  for (let i = 0; i < clips.length; i += 1) {
    const base = clipOffset + i * clipEntrySize;
    bytes.set(new TextEncoder().encode(clips[i] ?? ""), base);
    view.setUint32(base + 40, 1, true);
    view.setUint32(base + 44, 120, true);
    view.setUint16(base + 48, 1, true);
  }

  view.setUint16(frameOffset, 120, true);
  view.setUint8(frameOffset + 2, 0);
  view.setUint8(frameOffset + 3, encoding);
  view.setUint32(frameOffset + 4, 0, true);
  view.setUint32(frameOffset + 8, payloadLength, true);
  bytes.set(payload, payloadOffset);
  mutate?.(data);
  return data;
}
