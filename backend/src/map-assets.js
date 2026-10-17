import { access, mkdir, readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
import zlib from 'node:zlib';

const gameCandidates = () => {
  const out = [];
  for (const key of ['STEAM_GAME_PATH', 'CS2_GAME_PATH']) if (process.env[key]) out.push(process.env[key]);
  const drives = [...new Set(['C:', 'D:', 'E:', 'F:', 'G:', process.env.SystemDrive || 'C:'])];
  for (const drive of drives) {
    out.push(`${drive}\\Program Files (x86)\\Steam\\steamapps\\common\\Counter-Strike Global Offensive`);
    out.push(`${drive}\\Program Files\\Steam\\steamapps\\common\\Counter-Strike Global Offensive`);
    out.push(`${drive}\\SteamLibrary\\steamapps\\common\\Counter-Strike Global Offensive`);
  }
  return [...new Set(out)];
};

function crc32(buf) {
  let c = 0xffffffff;
  for (const n of buf) { c ^= n; for (let i = 0; i < 8; i++) c = (c >>> 1) ^ (0xedb88320 & -(c & 1)); }
  return (c ^ 0xffffffff) >>> 0;
}
function chunk(type, data) {
  const t = Buffer.from(type); const out = Buffer.alloc(12 + data.length);
  out.writeUInt32BE(data.length, 0); t.copy(out, 4); data.copy(out, 8); out.writeUInt32BE(crc32(Buffer.concat([t, data])), 8 + data.length); return out;
}
function png(width, height, rgba) {
  const rows = Buffer.alloc((width * 4 + 1) * height);
  for (let y = 0; y < height; y++) { rows[y * (width * 4 + 1)] = 0; rgba.copy(rows, y * (width * 4 + 1) + 1, y * width * 4, (y + 1) * width * 4); }
  return Buffer.concat([Buffer.from('\x89PNG\r\n\x1a\n', 'binary'), chunk('IHDR', (() => { const h = Buffer.alloc(13); h.writeUInt32BE(width, 0); h.writeUInt32BE(height, 4); h[8] = 8; h[9] = 6; return h; })()), chunk('IDAT', zlib.deflateSync(rows, { level: 6 })), chunk('IEND', Buffer.alloc(0))]);
}
function rgb565(v) { return [((v >> 11) & 31) * 255 / 31, ((v >> 5) & 63) * 255 / 63, (v & 31) * 255 / 31]; }
function ddsToPng(b) {
  if (b.toString('ascii', 0, 4) !== 'DDS ') throw new Error('not a DDS file');
  const height = b.readUInt32LE(12), width = b.readUInt32LE(16), pitch = b.readUInt32LE(20);
  const fourcc = b.toString('ascii', 84, 88), rgba = Buffer.alloc(width * height * 4), data = 128;
  if (fourcc === 'DXT1') {
    for (let by = 0; by < height; by += 4) for (let bx = 0; bx < width; bx += 4) {
      const o = data + ((by / 4) * Math.ceil(width / 4) + bx / 4) * 8, c0 = b.readUInt16LE(o), c1 = b.readUInt16LE(o + 2), bits = b.readUInt32LE(o + 4); const a = rgb565(c0), d = rgb565(c1), colors = [a, d];
      if (c0 > c1) { colors.push([(2*a[0]+d[0])/3,(2*a[1]+d[1])/3,(2*a[2]+d[2])/3], [(a[0]+2*d[0])/3,(a[1]+2*d[1])/3,(a[2]+2*d[2])/3]); } else colors.push([(a[0]+d[0])/2,(a[1]+d[1])/2,(a[2]+d[2])/2],[0,0,0]);
      for (let y = 0; y < 4; y++) for (let x = 0; x < 4; x++) { const px = bx+x, py = by+y; if (px >= width || py >= height) continue; const ci = (bits >> (2*(y*4+x))) & 3, q = (py*width+px)*4, col = colors[ci]; rgba[q]=col[0]; rgba[q+1]=col[1]; rgba[q+2]=col[2]; rgba[q+3]=(ci===3&&c0<=c1)?0:255; }
    }
  } else {
    const pfFlags = b.readUInt32LE(80), rgbBits = b.readUInt32LE(88), rMask = b.readUInt32LE(92), gMask = b.readUInt32LE(96), bMask = b.readUInt32LE(100), aMask = b.readUInt32LE(104); if (!(pfFlags & 0x40) || rgbBits < 24) throw new Error(`unsupported DDS format ${fourcc}`);
    const bytes = rgbBits / 8; for (let y=0;y<height;y++) for(let x=0;x<width;x++){const s=data+(y*width+x)*bytes,v=b.readUIntLE(s,bytes),q=(y*width+x)*4; const val=(mask)=>{if(!mask)return 0;let sh=0;while(((mask>>sh)&1)===0)sh++;const n=(mask>>>sh);return (((v&mask)>>>sh)*255/n)|0};rgba[q]=val(rMask);rgba[q+1]=val(gMask);rgba[q+2]=val(bMask);rgba[q+3]=aMask?val(aMask):255;}
  }
  return png(width, height, rgba);
}

async function firstExisting(paths) { for (const p of paths) { try { await access(p); return p; } catch {} } return null; }
export async function ensureMapAsset(map, webRoot) {
  if (!/^[a-z0-9_]+$/i.test(map)) return null;
  const out = path.join(webRoot, 'maps', `${map}.png`); try { await access(out); return out; } catch {}
  const roots = gameCandidates();
  const source = await firstExisting(roots.flatMap(root => [path.join(root, 'csgo', 'resource', 'overviews', `${map}_radar.dds`), path.join(root, 'game', 'csgo', 'resource', 'overviews', `${map}_radar.dds`)]));
  if (!source) return null;
  const image = ddsToPng(await readFile(source)); await mkdir(path.dirname(out), { recursive: true }); await writeFile(out, image); console.log(`[MapAssets] Generated ${map}.png from ${source}`); return out;
}
