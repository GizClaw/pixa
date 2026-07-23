import { mkdirSync, writeFileSync } from "node:fs";
import { dirname } from "node:path";

export const DEFAULT_BACKGROUND = { r: 0xdc, g: 0xef, b: 0xe8 };

export function parseBackground(value = "#dcefe8") {
  if (typeof value !== "string" || !/^#[0-9a-f]{6}$/i.test(value)) {
    throw new Error("background must be a six-digit hexadecimal color");
  }
  return {
    r: Number.parseInt(value.slice(1, 3), 16),
    g: Number.parseInt(value.slice(3, 5), 16),
    b: Number.parseInt(value.slice(5, 7), 16),
  };
}

export function validateClipName(value, name = "clip.name") {
  if (
    typeof value !== "string" ||
    value.length === 0 ||
    Buffer.byteLength(value, "utf8") > 31
  ) {
    throw new Error(`${name} must contain 1 to 31 UTF-8 bytes`);
  }
  return value;
}

export function encodePixa({ width, height, clips, frames }) {
  const headerSize = 40;
  const paletteSize = 2;
  const clipEntrySize = 56;
  const frameEntrySize = 16;
  const paletteOffset = headerSize;
  const clipOffset = paletteOffset + paletteSize;
  const frameOffset = clipOffset + clips.length * clipEntrySize;
  const payloadOffset = frameOffset + frames.length * frameEntrySize;
  const payloadLength = frames.reduce((size, frame) => size + frame.bytes.length, 0);
  const bytes = Buffer.alloc(payloadOffset + payloadLength);

  bytes.write("PIXA", 0, "ascii");
  bytes.writeUInt16LE(1, 4);
  bytes.writeUInt16LE(headerSize, 6);
  bytes.writeUInt16LE(width, 8);
  bytes.writeUInt16LE(height, 10);
  bytes.writeUInt16LE(1, 12);
  bytes.writeUInt16LE(clips.length, 14);
  bytes.writeUInt32LE(frames.length, 16);
  bytes.writeUInt32LE(paletteOffset, 20);
  bytes.writeUInt32LE(clipOffset, 24);
  bytes.writeUInt32LE(frameOffset, 28);
  bytes.writeUInt32LE(payloadOffset, 32);
  bytes.writeUInt32LE(payloadLength, 36);

  for (let index = 0; index < clips.length; index += 1) {
    const clip = clips[index];
    const base = clipOffset + index * clipEntrySize;
    bytes.write(clip.name, base, 32, "utf8");
    bytes.writeInt16LE(clip.anchorX ?? width / 2, base + 32);
    bytes.writeInt16LE(clip.anchorY ?? height / 2, base + 34);
    bytes.writeUInt32LE(clip.firstFrame, base + 36);
    bytes.writeUInt32LE(clip.frameCount, base + 40);
    bytes.writeUInt32LE(clip.totalDurationMs, base + 44);
    bytes.writeUInt16LE(clip.loop ? 1 : 0, base + 48);
  }

  let relativePayloadOffset = 0;
  for (let index = 0; index < frames.length; index += 1) {
    const frame = frames[index];
    const base = frameOffset + index * frameEntrySize;
    bytes.writeUInt16LE(frame.durationMs, base);
    bytes.writeUInt8(0, base + 2);
    bytes.writeUInt8(0, base + 3);
    bytes.writeUInt32LE(relativePayloadOffset, base + 4);
    bytes.writeUInt32LE(frame.bytes.length, base + 8);
    frame.bytes.copy(bytes, payloadOffset + relativePayloadOffset);
    relativePayloadOffset += frame.bytes.length;
  }

  return bytes;
}

export function writePixa(output, asset) {
  const bytes = encodePixa(asset);
  mkdirSync(dirname(output), { recursive: true });
  writeFileSync(output, bytes);
  return bytes;
}

export function rgb565(r, g, b) {
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

export function flattenPixel(rgba, offset, background = DEFAULT_BACKGROUND) {
  const alpha = rgba[offset + 3] / 255;
  return {
    r: Math.round(rgba[offset] * alpha + background.r * (1 - alpha)),
    g: Math.round(rgba[offset + 1] * alpha + background.g * (1 - alpha)),
    b: Math.round(rgba[offset + 2] * alpha + background.b * (1 - alpha)),
  };
}
