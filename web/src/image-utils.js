export const IMAGE_SIZE = 512;

export function widthForHeight(width, height) {
  return Math.max(IMAGE_SIZE, Math.round((width * IMAGE_SIZE) / height));
}

export function cropCenter(image) {
  const sx = Math.max(0, Math.floor((image.width - IMAGE_SIZE) / 2));
  const output = new Uint8ClampedArray(IMAGE_SIZE * IMAGE_SIZE * 4);

  for (let y = 0; y < IMAGE_SIZE; y += 1) {
    const inputStart = (y * image.width + sx) * 4;
    output.set(
      image.data.subarray(inputStart, inputStart + IMAGE_SIZE * 4),
      y * IMAGE_SIZE * 4,
    );
  }

  return new ImageData(output, IMAGE_SIZE, IMAGE_SIZE);
}
