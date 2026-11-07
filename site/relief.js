// The real ground under the picture: one small file instead of a request per map tile.
// relief.bin: 257 x 257 heights, 16-bit, in sixteenths of a world unit, east along a row, south down the
// rows, from -1600 to 1600 units. Made once from the free Terrain Tiles (see the page's credits).

const GRID = 257;
const HALF = 1600;
const STEP = 2 * HALF / (GRID - 1);

export async function loadRelief() {
  const answer = await fetch('relief.bin');
  if (!answer.ok) {
    throw new Error(`relief.bin: the site answered ${answer.status}`);
  }
  const h = new Int16Array(await answer.arrayBuffer());
  return (x, y) => {
    const gx = Math.min(GRID - 1.001, Math.max(0, (x + HALF) / STEP));
    const gy = Math.min(GRID - 1.001, Math.max(0, (y + HALF) / STEP));
    const ix = Math.floor(gx);
    const iy = Math.floor(gy);
    const fx = gx - ix;
    const fy = gy - iy;
    const k = iy * GRID + ix;
    const top = h[k] * (1 - fx) + h[k + 1] * fx;
    const low = h[k + GRID] * (1 - fx) + h[k + GRID + 1] * fx;
    return (top * (1 - fy) + low * fy) / 16;
  };
}
