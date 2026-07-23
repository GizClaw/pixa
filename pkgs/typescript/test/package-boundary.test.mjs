import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";

const packageDirectory = resolve(
  dirname(fileURLToPath(import.meta.url)),
  "../../..",
);

test("root package contains and exports only the TypeScript runtime", () => {
  const temporaryDirectory = mkdtempSync(join(tmpdir(), "pixa-package-"));
  const packOutput = execFileSync(
    "npm",
    ["pack", "--json", "--pack-destination", temporaryDirectory],
    {
      cwd: packageDirectory,
      encoding: "utf8",
    },
  );
  const jsonStart = packOutput.lastIndexOf("\n[") + 1;
  const [packResult] = JSON.parse(packOutput.slice(jsonStart));
  assert.ok(packResult);

  const packagedFiles = packResult.files.map(({ path }) => path).sort();
  assert.deepEqual(packagedFiles, [
    "LICENSE",
    "README.md",
    "package.json",
    "pkgs/typescript/dist/index.d.ts",
    "pkgs/typescript/dist/index.d.ts.map",
    "pkgs/typescript/dist/index.js",
    "pkgs/typescript/dist/index.js.map",
  ]);

  writeFileSync(
    join(temporaryDirectory, "package.json"),
    JSON.stringify({ private: true, type: "module" }),
  );
  execFileSync(
    "npm",
    [
      "install",
      "--ignore-scripts",
      "--no-audit",
      "--no-fund",
      join(temporaryDirectory, packResult.filename),
    ],
    {
      cwd: temporaryDirectory,
      encoding: "utf8",
    },
  );

  const installedPackage = JSON.parse(
    readFileSync(
      join(
        temporaryDirectory,
        "node_modules",
        "@gizclaw",
        "pixa",
        "package.json",
      ),
      "utf8",
    ),
  );
  assert.equal(installedPackage.private, true);

  const smokeProgram = `
    import {
      parsePixa,
      pixaClipFrameIndex,
      renderPixaFrameRGBA,
      selectPixaClip,
      validatePixa
    } from "@gizclaw/pixa";
    for (const value of [
      parsePixa,
      pixaClipFrameIndex,
      renderPixaFrameRGBA,
      selectPixaClip,
      validatePixa
    ]) {
      if (typeof value !== "function") process.exit(1);
    }
  `;
  execFileSync("node", ["--input-type=module", "--eval", smokeProgram], {
    cwd: temporaryDirectory,
    encoding: "utf8",
  });
});
