#!/usr/bin/env node

const fs = require("node:fs/promises");
const path = require("node:path");
const sharp = require("sharp");

const ROOT_DIR = process.cwd();
const SCRIPT_DIR = __dirname;
const DATA_DIR = path.join(SCRIPT_DIR, "data");
const ASSETS_DIR = path.join(ROOT_DIR, "assets", "images");
const SUPPORTED_EXTENSIONS = new Set([
  ".jpg",
  ".jpeg",
  ".png",
  ".webp",
  ".bmp",
  ".tif",
  ".tiff",
]);

const TARGETS = [
  {
    name: "cars",
    inputDir: path.join(DATA_DIR, "cars"),
    outputDir: path.join(ASSETS_DIR, "cars"),
  },
  {
    name: "stages",
    inputDir: path.join(DATA_DIR, "stages"),
    outputDir: path.join(ASSETS_DIR, "stages"),
  },
];

async function pathExists(targetPath) {
  try {
    await fs.access(targetPath);
    return true;
  } catch {
    return false;
  }
}

async function collectImages(dir, relativeDir = "") {
  const absoluteDir = path.join(dir, relativeDir);
  const entries = await fs.readdir(absoluteDir, { withFileTypes: true });
  const results = [];

  for (const entry of entries) {
    const entryRelativePath = path.join(relativeDir, entry.name);

    if (entry.isDirectory()) {
      results.push(...(await collectImages(dir, entryRelativePath)));
      continue;
    }

    if (!entry.isFile()) {
      continue;
    }

    const extension = path.extname(entry.name).toLowerCase();
    if (SUPPORTED_EXTENSIONS.has(extension)) {
      results.push(entryRelativePath);
    }
  }

  return results;
}

async function ensureDirectory(targetPath) {
  await fs.mkdir(targetPath, { recursive: true });
}

async function shouldProcessImage(inputPath, outputPath) {
  if (!(await pathExists(outputPath))) {
    return true;
  }

  const [inputStats, outputStats] = await Promise.all([
    fs.stat(inputPath),
    fs.stat(outputPath),
  ]);
  return inputStats.mtimeMs > outputStats.mtimeMs;
}

async function processImage(inputPath, outputPath) {
  await ensureDirectory(path.dirname(outputPath));

  await sharp(inputPath)
    .rotate()
    .resize(512, 512, {
      fit: "cover",
      position: "centre",
    })
    .jpeg({
      quality: 90,
      mozjpeg: true,
    })
    .toFile(outputPath);
}

async function processTarget(target) {
  if (!(await pathExists(target.inputDir))) {
    console.log(`[skip] ${target.name}: directory missing -> ${target.inputDir}`);
    return { processed: 0, skipped: true };
  }

  const images = await collectImages(target.inputDir);
  await ensureDirectory(target.outputDir);

  if (images.length === 0) {
    console.log(`[skip] ${target.name}: no images in ${target.inputDir}`);
    return { processed: 0, skipped: true };
  }

  let processed = 0;
  let reused = 0;

  for (const relativeImagePath of images) {
    const inputPath = path.join(target.inputDir, relativeImagePath);
    const outputRelativePath = `${relativeImagePath.slice(
      0,
      -path.extname(relativeImagePath).length,
    )}.jpg`;
    const outputPath = path.join(target.outputDir, outputRelativePath);

    if (!(await shouldProcessImage(inputPath, outputPath))) {
      reused += 1;
      console.log(`[skip] ${target.name}: unchanged ${relativeImagePath}`);
      continue;
    }

    await processImage(inputPath, outputPath);
    processed += 1;
    console.log(`[ok] ${target.name}: ${relativeImagePath} -> ${outputRelativePath}`);
  }

  return { processed, reused, skipped: false };
}

async function main() {
  let total = 0;
  let reused = 0;

  for (const target of TARGETS) {
    const result = await processTarget(target);
    total += result.processed;
    reused += result.reused ?? 0;
  }

  console.log(
    `Done. ${total} image(s) converted, ${reused} unchanged image(s) reused from ${ASSETS_DIR}`,
  );
}

main().catch((error) => {
  console.error("Processing failed:", error);
  process.exitCode = 1;
});
