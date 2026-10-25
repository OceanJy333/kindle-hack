// input: old/new grayscale frames; output: RLE dirty rectangle packets; pos: low bandwidth transport.
import { deflateSync } from 'node:zlib';
import { W, H } from './tokens.ts';
export { W, H };
export function encode(next: Uint8Array, previous?: Uint8Array, full = false, seq = 1): Buffer | undefined {
  if (next.length !== W*H) throw new Error('Wrong frame size');
  let left = W, right = -1, top = H, bottom = -1;
  if (!previous || full) { left = top = 0; right = W-1; bottom = H-1; }
  else for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
    const i = y*W+x;
    if (next[i] !== previous[i]) { left = Math.min(left,x); right = Math.max(right,x); top = Math.min(top,y); bottom = Math.max(bottom,y); }
  }
  if (right < left) return;
  return pack(next, left, top, right, bottom, seq, full || !previous);
}
// Separate changes (e.g. header minute and a progress bar) become separate small EPDC updates
// instead of one tall rectangle that re-drives every pixel in between.
export function encodeBands(next: Uint8Array, previous?: Uint8Array, full = false, seq = 1): Buffer[] {
  if (!previous || full) return [encode(next, previous, true, seq)!];
  const bands: {top:number;bottom:number;left:number;right:number}[] = [];
  for (let y = 0; y < H; y++) {
    let l = W, r = -1;
    for (let x = 0, i = y*W; x < W; x++, i++) if (next[i] !== previous[i]) { if (x < l) l = x; r = x; }
    if (r < 0) continue;
    const last = bands.at(-1);
    if (last && y-last.bottom <= 48) { last.bottom = y; last.left = Math.min(last.left,l); last.right = Math.max(last.right,r); }
    else bands.push({top:y,bottom:y,left:l,right:r});
  }
  if (!bands.length) return [];
  if (bands.length > 3) return [encode(next, previous, false, seq)!];
  return bands.map((b,i) => pack(next, b.left, b.top, b.right, b.bottom, seq+i, false));
}
function pack(next: Uint8Array, left: number, top: number, right: number, bottom: number, seq: number, full: boolean): Buffer {
  // EPDC prefers even boundaries; align without exceeding the visible framebuffer.
  left &= ~1; top &= ~1; right = Math.min(W-1, right|1); bottom = Math.min(H-1, bottom|1);
  const width = right-left+1, height = bottom-top+1, result = Buffer.alloc(32+width*height*2);
  let offset = 32, value = -1, count = 0;
  const flush = () => { if (count) { result[offset++] = count; result[offset++] = value; } };
  for (let y = top; y <= bottom; y++) for (let x = left; x <= right; x++) {
    const pixel = next[y*W+x];
    if (pixel !== value || count === 255) { flush(); value = pixel; count = 0; }
    count++;
  }
  flush();
  const rle=result.subarray(32,offset), compressed=deflateSync(rle,{level:3});
  const payload=compressed.length<rle.length?compressed:rle, flags=(full?1:0)|(payload===compressed?2:0);
  [0x31424741,seq,left,top,width,height,payload.length,flags].forEach((n,i) => result.writeUInt32LE(Number(n),i*4));
  return Buffer.concat([result.subarray(0,32),payload]);
}
