import decode, { init as initJpegDecode } from "@jsquash/jpeg/decode";
import encode, { init as initJpegEncode } from "@jsquash/jpeg/encode";
import resize, { initResize } from "@jsquash/resize";
import jpegDecodeWasm from "@jsquash/jpeg/codec/dec/mozjpeg_dec.wasm";
import jpegEncodeWasm from "@jsquash/jpeg/codec/enc/mozjpeg_enc.wasm";
import resizeWasm from "@jsquash/resize/lib/resize/pkg/squoosh_resize_bg.wasm";
import { cropCenter, IMAGE_SIZE, widthForHeight } from "./image-utils.js";

const RSF_IMAGE_PATH = /^\/images\/(cars|stages)\/(\d+)\.jpg$/;
const CACHE_CONTROL = "public, max-age=31536000, immutable";

if (!globalThis.ImageData) {
  globalThis.ImageData = class ImageData {
    constructor(data, width, height) {
      this.data = data;
      this.width = width;
      this.height = height;
    }
  };
}

const codecsReady = Promise.all([
  initJpegDecode(jpegDecodeWasm),
  initJpegEncode(jpegEncodeWasm),
  initResize(resizeWasm),
]);

async function transformImage(source) {
  const startedAt = performance.now();
  await codecsReady;

  const decoded = await decode(source, { preserveOrientation: true });
  const resizedWidth = widthForHeight(decoded.width, decoded.height);
  const resized = await resize(decoded, {
    width: resizedWidth,
    height: IMAGE_SIZE,
    fitMethod: "stretch",
    method: "mitchell",
  });

  const image = await encode(cropCenter(resized), { quality: 100 });
  return {
    image,
    ms: Math.round(performance.now() - startedAt),
    sourceWidth: decoded.width,
    sourceHeight: decoded.height,
    resizedWidth,
  };
}

export default {
  async fetch(request, env, ctx) {
    const startedAt = performance.now();
    const url = new URL(request.url);
    const match = url.pathname.match(RSF_IMAGE_PATH);

    if (!match) {
      return env.ASSETS.fetch(request);
    }

    if (request.method !== "GET" && request.method !== "HEAD") {
      return new Response("Method Not Allowed", {
        status: 405,
        headers: { Allow: "GET, HEAD" },
      });
    }

    const cache = caches.default;
    const cacheUrl = new URL(url);
    cacheUrl.search = "";
    const cacheKey = new Request(cacheUrl.toString(), { method: "GET" });
    const cached = await cache.match(cacheKey);

    if (cached) {
      console.log("rsf_image", {
        path: url.pathname,
        method: request.method,
        cache: "hit",
        ms: Math.round(performance.now() - startedAt),
      });

      return request.method === "HEAD"
        ? new Response(null, { headers: cached.headers })
        : cached;
    }

    const sourceUrl = `https://www.rallysimfans.hu/rbr/images/${match[1]}/${match[2]}.jpg`;
    const source = await fetch(sourceUrl, {
      cf: {
        cacheEverything: true,
        cacheTtl: 31536000,
      },
    });

    if (!source.ok) {
      console.warn("rsf_image", {
        path: url.pathname,
        sourceUrl,
        sourceStatus: source.status,
        cache: "miss",
        ms: Math.round(performance.now() - startedAt),
      });

      return new Response("Not Found", {
        status: source.status === 404 ? 404 : 502,
      });
    }

    if (request.method === "HEAD") {
      console.log("rsf_image", {
        path: url.pathname,
        method: request.method,
        sourceStatus: source.status,
        cache: "miss",
        ms: Math.round(performance.now() - startedAt),
      });

      return new Response(null, { headers: imageHeaders() });
    }

    const transformed = await transformImage(await source.arrayBuffer());
    const response = new Response(transformed.image, { headers: imageHeaders() });
    ctx.waitUntil(cache.put(cacheKey, response.clone()));

    console.log("rsf_image", {
      path: url.pathname,
      method: request.method,
      sourceStatus: source.status,
      cache: "miss",
      sourceSize: `${transformed.sourceWidth}x${transformed.sourceHeight}`,
      resizedSize: `${transformed.resizedWidth}x${IMAGE_SIZE}`,
      outputSize: `${IMAGE_SIZE}x${IMAGE_SIZE}`,
      transformMs: transformed.ms,
      ms: Math.round(performance.now() - startedAt),
    });

    return response;
  },
};

function imageHeaders() {
  return {
    "Cache-Control": CACHE_CONTROL,
    "Content-Type": "image/jpeg",
    "X-Content-Type-Options": "nosniff",
    "X-Robots-Tag": "noindex, noimageindex, noarchive",
  };
}
