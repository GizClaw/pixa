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
    "usage: node tools/gifs-to-pixa.mjs <input-directory> <output.pixa> <manifest.json|->",
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

const inputDirectory = resolve(inputArgument);
const output = resolve(outputArgument);
const manifest = readManifest(manifestArgument);
const { width, height } = parseCanvas(manifest.canvas);
const padding = nonNegativeInteger(manifest.padding ?? 0, "padding");
if (padding * 2 >= width || padding * 2 >= height) {
  throw new Error("padding must leave a non-empty frame canvas");
}
const spriteWidth = width - padding * 2;
const spriteHeight = height - padding * 2;
const background = parseBackground(manifest.background);
const minimumFrameDurationMs = positiveInteger(
  manifest.minimumFrameDurationMs ?? 1,
  "minimumFrameDurationMs",
);
const clips = parseClips(manifest.clips);
const frames = [];

for (const clip of clips) {
  const source = resolve(inputDirectory, clip.source);
  const durationResult = run("ffprobe", [
    "-v", "error",
    "-select_streams", "v:0",
    "-show_entries", "stream=duration",
    "-of", "default=noprint_wrappers=1:nokey=1",
    source,
  ], { encoding: "utf8" });
  const result = run("ffmpeg", [
    "-hide_banner",
    "-loglevel", "error",
    "-ignore_loop", "1",
    "-i", source,
    "-vf",
    `scale=${spriteWidth}:${spriteHeight}:force_original_aspect_ratio=decrease:flags=neighbor,pad=${spriteWidth}:${spriteHeight}:(ow-iw)/2:(oh-ih)/2:color=black@0,format=rgba`,
    "-fps_mode", "passthrough",
    "-pix_fmt", "rgba",
    "-f", "rawvideo",
    "pipe:1",
  ], { maxBuffer: 256 * 1024 * 1024 });
  const rgbaFrameBytes = spriteWidth * spriteHeight * 4;
  if (result.stdout.length === 0 || result.stdout.length % rgbaFrameBytes !== 0) {
    throw new Error(`unexpected RGBA payload size for ${source}: ${result.stdout.length}`);
  }

  clip.firstFrame = frames.length;
  clip.frameCount = result.stdout.length / rgbaFrameBytes;
  const durationSeconds = Number(durationResult.stdout.trim());
  if (!Number.isFinite(durationSeconds) || durationSeconds <= 0) {
    throw new Error(`ffprobe returned an invalid duration for ${source}`);
  }
  const clipDurationMs = Math.max(1, Math.round(durationSeconds * 1000));
  const frameDurationMs = Math.max(
    minimumFrameDurationMs,
    Math.round(clipDurationMs / clip.frameCount),
  );
  if (frameDurationMs > 0xffff) {
    throw new Error(`${source}: frame duration exceeds the PIXA v1 range`);
  }

  const clipFrames = [];
  for (let index = 0; index < clip.frameCount; index += 1) {
    const sourceFrame = result.stdout.subarray(
      index * rgbaFrameBytes,
      (index + 1) * rgbaFrameBytes,
    );
    const encoded = encodeGifFrame({
      sourceFrame,
      width,
      height,
      spriteWidth,
      spriteHeight,
      padding,
      background,
    });
    clipFrames.push(encoded);
    frames.push({ bytes: encoded, durationMs: frameDurationMs });
  }
  if (
    clipFrames.length > 1 &&
    clipFrames.slice(1).every((frame) => frame.equals(clipFrames[0]))
  ) {
    throw new Error(`${source}: decoded animation frames are identical`);
  }
  clip.totalDurationMs = frameDurationMs * clip.frameCount;
  if (clip.totalDurationMs > 0xffffffff) {
    throw new Error(`${source}: clip duration exceeds the PIXA v1 range`);
  }
}

const bytes = writePixa(output, { width, height, clips, frames });
console.log(JSON.stringify({
  inputDirectory,
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

function parseClips(value) {
  if (!Array.isArray(value) || value.length === 0) {
    throw new Error("manifest.clips must be a non-empty array");
  }
  return value.map((clip, index) => {
    const prefix = `clips[${index}]`;
    if (typeof clip?.source !== "string" || clip.source.length === 0) {
      throw new Error(`${prefix}.source must be a non-empty path`);
    }
    return {
      name: validateClipName(clip?.name, `${prefix}.name`),
      source: clip.source,
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

function run(command, args, options = {}) {
  const result = spawnSync(command, args, options);
  if (result.error != null) throw result.error;
  if (result.status !== 0) {
    const stderr = result.stderr?.toString().trim() ?? "";
    throw new Error(`${command} failed: ${stderr}`);
  }
  return result;
}

function encodeGifFrame({
  sourceFrame,
  width,
  height,
  spriteWidth,
  spriteHeight,
  padding,
  background,
}) {
  const encoded = Buffer.alloc(width * height * 2);
  for (let y = 0; y < height; y += 1) {
    for (let x = 0; x < width; x += 1) {
      let color = background;
      if (
        x >= padding &&
        x < width - padding &&
        y >= padding &&
        y < height - padding
      ) {
        const sourceOffset = ((y - padding) * spriteWidth + (x - padding)) * 4;
        color = flattenPixel(sourceFrame, sourceOffset, background);
      }
      encoded.writeUInt16LE(rgb565(color.r, color.g, color.b), (y * width + x) * 2);
    }
  }
  return encoded;
}
