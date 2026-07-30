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

export function encodePixa({ width, height, clips, frames, palette = [0] }) {
  const headerSize = 40;
  if (!Array.isArray(palette) || palette.length === 0 || palette.length > 256) {
    throw new Error("palette must contain 1 to 256 RGB565 colors");
  }
  if (palette[0] !== 0) {
    throw new Error("palette index 0 must store RGB565 value 0");
  }
  const paletteSize = palette.length * 2;
  const clipEntrySize = 56;
  const frameEntrySize = 16;
  const paletteOffset = headerSize;
  const clipOffset = paletteOffset + paletteSize;
  const frameOffset = clipOffset + clips.length * clipEntrySize;
  const payloadOffset = frameOffset + frames.length * frameEntrySize;
  const payloadLength = frames.reduce(
    (size, frame) => size + frame.bytes.length,
    0,
  );
  const bytes = Buffer.alloc(payloadOffset + payloadLength);

  bytes.write("PIXA", 0, "ascii");
  bytes.writeUInt16LE(1, 4);
  bytes.writeUInt16LE(headerSize, 6);
  bytes.writeUInt16LE(width, 8);
  bytes.writeUInt16LE(height, 10);
  bytes.writeUInt16LE(palette.length, 12);
  bytes.writeUInt16LE(clips.length, 14);
  bytes.writeUInt32LE(frames.length, 16);
  bytes.writeUInt32LE(paletteOffset, 20);
  bytes.writeUInt32LE(clipOffset, 24);
  bytes.writeUInt32LE(frameOffset, 28);
  bytes.writeUInt32LE(payloadOffset, 32);
  bytes.writeUInt32LE(payloadLength, 36);

  for (let index = 0; index < palette.length; index += 1) {
    const color = palette[index];
    if (!Number.isInteger(color) || color < 0 || color > 0xffff) {
      throw new Error(`palette[${index}] must be an RGB565 uint16`);
    }
    bytes.writeUInt16LE(color, paletteOffset + index * 2);
  }

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
    bytes.writeUInt8(frame.encoding ?? 0, base + 3);
    bytes.writeUInt32LE(relativePayloadOffset, base + 4);
    bytes.writeUInt32LE(frame.bytes.length, base + 8);
    frame.bytes.copy(bytes, payloadOffset + relativePayloadOffset);
    relativePayloadOffset += frame.bytes.length;
  }

  return bytes;
}

export function encodePaletteRlePixa({ width, height, clips, frames }) {
  const pixelCount = width * height;
  const palette = [0];
  const paletteIndexes = new Map();
  const encodedFrames = frames.map((frame, frameIndex) => {
    if (!(frame.pixels instanceof Int32Array)) {
      throw new Error(`frames[${frameIndex}].pixels must be an Int32Array`);
    }
    if (frame.pixels.length !== pixelCount) {
      throw new Error(
        `frames[${frameIndex}] has ${frame.pixels.length} pixels, expected ${pixelCount}`,
      );
    }
    const indices = new Uint8Array(pixelCount);
    for (let pixel = 0; pixel < pixelCount; pixel += 1) {
      const color = frame.pixels[pixel];
      if (color === -1) {
        indices[pixel] = 0;
        continue;
      }
      if (color < 0 || color > 0xffff) {
        throw new Error(
          `frames[${frameIndex}] contains an invalid RGB565 color`,
        );
      }
      let paletteIndex = paletteIndexes.get(color);
      if (paletteIndex == null) {
        if (palette.length === 256) {
          throw new Error("opaque colors exceed the PIXA v1 palette capacity");
        }
        paletteIndex = palette.length;
        paletteIndexes.set(color, paletteIndex);
        palette.push(color);
      }
      indices[pixel] = paletteIndex;
    }
    return {
      bytes: encodePaletteRle(indices),
      durationMs: frame.durationMs,
      encoding: 1,
    };
  });
  return encodePixa({ width, height, clips, frames: encodedFrames, palette });
}

export function writePaletteRlePixa(output, asset) {
  const bytes = encodePaletteRlePixa(asset);
  mkdirSync(dirname(output), { recursive: true });
  writeFileSync(output, bytes);
  return bytes;
}

export function writePixa(output, asset) {
  const bytes = encodePixa(asset);
  mkdirSync(dirname(output), { recursive: true });
  writeFileSync(output, bytes);
  return bytes;
}

export function quantizeRgbaFrame(rgba, width, height, alphaThreshold = 128) {
  const pixelCount = width * height;
  if (
    !Buffer.isBuffer(rgba) &&
    !(rgba instanceof Uint8Array) &&
    !(rgba instanceof Uint8ClampedArray)
  ) {
    throw new Error("RGBA frame must be a byte array");
  }
  if (rgba.length !== pixelCount * 4) {
    throw new Error(
      `RGBA frame has ${rgba.length} bytes, expected ${pixelCount * 4}`,
    );
  }
  if (
    !Number.isInteger(alphaThreshold) ||
    alphaThreshold < 1 ||
    alphaThreshold > 255
  ) {
    throw new Error("alpha threshold must be an integer from 1 to 255");
  }
  const pixels = new Int32Array(pixelCount);
  for (let pixel = 0; pixel < pixelCount; pixel += 1) {
    const offset = pixel * 4;
    if (rgba[offset + 3] < alphaThreshold) {
      pixels[pixel] = -1;
      continue;
    }
    pixels[pixel] = rgb565(
      quantizeChannel(rgba[offset], 6),
      quantizeChannel(rgba[offset + 1], 7),
      quantizeChannel(rgba[offset + 2], 6),
    );
  }
  return pixels;
}

export function validateTransparentSpriteFrame(
  pixels,
  width,
  height,
  label = "frame",
) {
  if (!(pixels instanceof Int32Array) || pixels.length !== width * height) {
    throw new Error(`${label}: invalid pixel buffer`);
  }
  if (!pixels.some((pixel) => pixel !== -1)) {
    throw new Error(`${label}: frame is fully transparent`);
  }
  for (let x = 0; x < width; x += 1) {
    if (pixels[x] !== -1) {
      throw new Error(`${label}: top edge pixel ${x} is opaque`);
    }
    const bottom = (height - 1) * width + x;
    if (pixels[bottom] !== -1) {
      throw new Error(`${label}: bottom edge pixel ${x} is opaque`);
    }
  }
  for (let y = 0; y < height; y += 1) {
    const left = y * width;
    if (pixels[left] !== -1) {
      throw new Error(`${label}: left edge pixel ${y} is opaque`);
    }
    const right = left + width - 1;
    if (pixels[right] !== -1) {
      throw new Error(`${label}: right edge pixel ${y} is opaque`);
    }
  }
}

export function rgb565(r, g, b) {
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

function quantizeChannel(value, levels) {
  const level = Math.round((value * (levels - 1)) / 255);
  return Math.round((level * 255) / (levels - 1));
}

function encodePaletteRle(indices) {
  const output = Buffer.alloc(indices.length * 2);
  let outputLength = 0;
  for (let pixel = 0; pixel < indices.length;) {
    const paletteIndex = indices[pixel];
    let runLength = 1;
    while (
      runLength < 255 &&
      pixel + runLength < indices.length &&
      indices[pixel + runLength] === paletteIndex
    ) {
      runLength += 1;
    }
    output[outputLength] = runLength;
    output[outputLength + 1] = paletteIndex;
    outputLength += 2;
    pixel += runLength;
  }
  return output.subarray(0, outputLength);
}

export function flattenPixel(rgba, offset, background = DEFAULT_BACKGROUND) {
  const alpha = rgba[offset + 3] / 255;
  return {
    r: Math.round(rgba[offset] * alpha + background.r * (1 - alpha)),
    g: Math.round(rgba[offset + 1] * alpha + background.g * (1 - alpha)),
    b: Math.round(rgba[offset + 2] * alpha + background.b * (1 - alpha)),
  };
}
