import assert from "node:assert/strict";
import { cropCenter, widthForHeight } from "./image-utils.js";

if (!globalThis.ImageData) {
  globalThis.ImageData = class ImageData {
    constructor(data, width, height) {
      this.data = data;
      this.width = width;
      this.height = height;
    }
  };
}

assert.equal(widthForHeight(240, 180), 683);
assert.equal(widthForHeight(180, 240), 512);

const image = new ImageData(new Uint8ClampedArray(683 * 512 * 4), 683, 512);
const cropped = cropCenter(image);
assert.equal(cropped.width, 512);
assert.equal(cropped.height, 512);
