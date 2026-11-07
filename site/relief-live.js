// The real ground round any place, from the free Terrain Tiles: red, green and blue of a pixel are its height.
// Answers a function: x (east) and y (south), in world units, to a height in world units. For a board's home.

const TILES = 'https://s3.amazonaws.com/elevation-tiles-prod/terrarium';
const ZOOM = 8;
const SIDE = 5;                                        // tiles across: more than the scene shows
const EARTH_KM = 40075;

async function tile(x, y) {
  const answer = await fetch(`${TILES}/${ZOOM}/${x}/${y}.png`);
  if (!answer.ok) {
    throw new Error(`a relief tile answered ${answer.status}`);
  }
  return createImageBitmap(await answer.blob());
}

/* a box blur, along the rows and then down the columns: the wire is coarser than the data, and
 * every small bump left in would show as a jagged line */
function smooth(a, size, r) {
  const row = new Float32Array(size);
  for (let pass = 0; pass < 2; pass++) {
    const along = pass === 0 ? 1 : size;
    const across = pass === 0 ? size : 1;
    for (let line = 0; line < size; line++) {
      const base = line * across;
      for (let i = 0; i < size; i++) {
        let sum = 0;
        for (let k = -r; k <= r; k++) {
          sum += a[base + Math.min(size - 1, Math.max(0, i + k)) * along];
        }
        row[i] = sum / (2 * r + 1);
      }
      for (let i = 0; i < size; i++) {
        a[base + i * along] = row[i];
      }
    }
  }
}

export async function reliefAround(lat, lon, kmPerUnit, unitsPerMetre) {
  const n = 2 ** ZOOM;
  const rad = lat * Math.PI / 180;
  const tx = (lon + 180) / 360 * n;
  const ty = (1 - Math.log(Math.tan(rad) + 1 / Math.cos(rad)) / Math.PI) / 2 * n;
  const x0 = Math.floor(tx) - 2;
  const y0 = Math.floor(ty) - 2;
  const size = SIDE * 256;
  const canvas = document.createElement('canvas');
  canvas.width = size;
  canvas.height = size;
  const pen = canvas.getContext('2d', { willReadFrequently: true });
  const jobs = [];
  for (let j = 0; j < SIDE; j++) {
    for (let i = 0; i < SIDE; i++) {
      jobs.push(tile(x0 + i, y0 + j).then((b) => pen.drawImage(b, i * 256, j * 256)));
    }
  }
  await Promise.all(jobs);

  const rgb = pen.getImageData(0, 0, size, size).data;
  const metres = new Float32Array(size * size);
  for (let i = 0; i < metres.length; i++) {
    metres[i] = rgb[i * 4] * 256 + rgb[i * 4 + 1] + rgb[i * 4 + 2] / 256 - 32768;
  }
  smooth(metres, size, 3);
  smooth(metres, size, 2);
  const at = (gx, gy) => {
    const x = Math.min(size - 2, Math.max(0, gx));
    const y = Math.min(size - 2, Math.max(0, gy));
    const ix = Math.floor(x);
    const iy = Math.floor(y);
    const fx = x - ix;
    const fy = y - iy;
    const k = iy * size + ix;
    const top = metres[k] * (1 - fx) + metres[k + 1] * fx;
    const low = metres[k + size] * (1 - fx) + metres[k + size + 1] * fx;
    return top * (1 - fy) + low * fy;
  };

  const cx = (tx - x0) * 256;
  const cy = (ty - y0) * 256;
  const pixelsPerKm = 256 * n / (EARTH_KM * Math.cos(rad));
  const perUnit = kmPerUnit * pixelsPerKm;
  const floor = at(cx, cy);                            // the town is the zero
  return (x, y) => Math.max(-6, (at(cx + x * perUnit, cy + y * perUnit) - floor) * unitsPerMetre);
}
