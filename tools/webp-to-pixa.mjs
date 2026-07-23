#!/usr/bin/env node

import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { spawnSync } from "node:child_process";
import {
  flattenPixel,
  parseBackground,
  rgb565,
  validateClipName,
  writePixa,
} from "./lib/pixa-v1.mjs";

function usage() {
  console.error(
    "usage: node tools/webp-to-pixa.mjs <atlas.webp> <output.pixa> <manifest.json|->",
  );
}

const [inputArgument, outputArgument, manifestArgument] = process.argv.slice(2);
if (
  inputArgument == null ||
  outputArgument == null ||
  manifestArgument == null ||
  process.argv.length !== 5
) {
  usage();
  process.exit(2);
}

const input = resolve(inputArgument);
const output = resolve(outputArgument);
const manifest = readManifest(manifestArgument);
const { width, height } = parseCanvas(manifest.canvas);
const { columns, rows } = parseAtlas(manifest.atlas);
const background = parseBackground(manifest.background);
const clips = parseClips(manifest.clips, columns, rows);
const atlas = decodeAtlas(input, width, height, columns, rows);
const frames = [];

for (const clip of clips) {
  clip.firstFrame = frames.length;
  const clipFrames = [];
  for (let frame = 0; frame < clip.frameCount; frame += 1) {
    const bytes = encodeAtlasFrame({
      atlas,
      atlasWidth: width * columns,
      width,
      height,
      column: clip.column + frame,
      row: clip.row,
      background,
    });
    clipFrames.push(bytes);
    frames.push({ bytes, durationMs: clip.durationMs });
  }
  if (
    clipFrames.length > 1 &&
    clipFrames.slice(1).every((frame) => frame.equals(clipFrames[0]))
  ) {
    throw new Error(`${input}: animation clip ${clip.name} is static`);
  }
  clip.totalDurationMs = clip.frameCount * clip.durationMs;
}

const bytes = writePixa(output, { width, height, clips, frames });
console.log(JSON.stringify({
  input,
  output,
  bytes: bytes.length,
  canvas: { width, height },
  clips: clips.length,
  frames: frames.length,
}));

function readManifest(path) {
  const data = path === "-" ? readFileSync(0, "utf8") : readFileSync(resolve(path), "utf8");
  try {
    return JSON.parse(data);
  } catch (error) {
    throw new Error(`invalid manifest JSON: ${error.message}`);
  }
}

function parseCanvas(canvas) {
  const width = positiveInteger(canvas?.width, "canvas.width");
  const height = positiveInteger(canvas?.height, "canvas.height");
  if (width > 0xffff || height > 0xffff) {
    throw new Error("canvas dimensions must fit uint16");
  }
  return { width, height };
}

function parseAtlas(atlas) {
  return {
    columns: positiveInteger(atlas?.columns, "atlas.columns"),
    rows: positiveInteger(atlas?.rows, "atlas.rows"),
  };
}

function parseClips(value, columns, rows) {
  if (!Array.isArray(value) || value.length === 0) {
    throw new Error("manifest.clips must be a non-empty array");
  }
  return value.map((clip, index) => {
    const prefix = `clips[${index}]`;
    const name = validateClipName(clip?.name, `${prefix}.name`);
    const row = nonNegativeInteger(clip?.row, `${prefix}.row`);
    const column = nonNegativeInteger(clip?.column ?? 0, `${prefix}.column`);
    const frameCount = positiveInteger(clip?.frameCount, `${prefix}.frameCount`);
    const durationMs = positiveInteger(clip?.durationMs, `${prefix}.durationMs`);
    if (row >= rows || column + frameCount > columns) {
      throw new Error(`${prefix} exceeds the declared atlas grid`);
    }
    if (durationMs > 0xffff || durationMs * frameCount > 0xffffffff) {
      throw new Error(`${prefix} duration exceeds the PIXA v1 range`);
    }
    return {
      name,
      row,
      column,
      frameCount,
      durationMs,
      loop: Boolean(clip?.loop),
    };
  });
}

function positiveInteger(value, name) {
  if (!Number.isSafeInteger(value) || value < 1) {
    throw new Error(`${name} must be a positive integer`);
  }
  return value;
}

function nonNegativeInteger(value, name) {
  if (!Number.isSafeInteger(value) || value < 0) {
    throw new Error(`${name} must be a non-negative integer`);
  }
  return value;
}

function decodeAtlas(source, width, height, columns, rows) {
  const result = spawnSync("ffmpeg", [
    "-hide_banner",
    "-loglevel", "error",
    "-i", source,
    "-vf", `scale=${width * columns}:${height * rows}:flags=neighbor,format=rgba`,
    "-frames:v", "1",
    "-pix_fmt", "rgba",
    "-f", "rawvideo",
    "pipe:1",
  ], { maxBuffer: 256 * 1024 * 1024 });
  if (result.error != null) throw result.error;
  if (result.status !== 0) {
    throw new Error(`ffmpeg failed for ${source}: ${result.stderr.toString().trim()}`);
  }
  const expected = width * columns * height * rows * 4;
  if (result.stdout.length !== expected) {
    throw new Error(`unexpected RGBA payload size for ${source}: ${result.stdout.length}`);
  }
  return result.stdout;
}

function encodeAtlasFrame({
  atlas,
  atlasWidth,
  width,
  height,
  column,
  row,
  background,
}) {
  const outputFrame = Buffer.alloc(width * height * 2);
  for (let y = 0; y < height; y += 1) {
    for (let x = 0; x < width; x += 1) {
      const sourceX = column * width + x;
      const sourceY = row * height + y;
      const sourceOffset = (sourceY * atlasWidth + sourceX) * 4;
      const { r, g, b } = flattenPixel(atlas, sourceOffset, background);
      outputFrame.writeUInt16LE(rgb565(r, g, b), (y * width + x) * 2);
    }
  }
  return outputFrame;
}
