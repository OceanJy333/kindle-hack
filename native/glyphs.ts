// input: Noto Sans CJK SC (same family as the Voyage reader); output: cached vector glyphs per weight; pos: text shaping for the desk.
import { openSync } from 'fontkit';
import { resolve } from 'node:path';
export type Weight = 400|500|700;
const dir=process.env.DESK_FONT_DIR||resolve(import.meta.dirname,'../build/fonts');
const files:Record<Weight,string>={400:'NotoSansCJKsc-Regular.otf',500:'NotoSansCJKsc-Medium.otf',700:'NotoSansCJKsc-Bold.otf'};
const fonts=new Map<Weight,any>();
const cache=new Map<string,{id:string;path:string;advance:number}>();
function font(weight:Weight){if(!fonts.has(weight))fonts.set(weight,openSync(resolve(dir,files[weight])));return fonts.get(weight);}
export function glyph(c: string,weight:Weight=400) {
  const key=weight+c;
  if(!cache.has(key)) {
    const g=font(weight).glyphForCodePoint(c.codePointAt(0));
    cache.set(key,{id:`g${weight}_${c.codePointAt(0)}`,path:g.path.toSVG(),advance:g.advanceWidth});
  }
  return cache.get(key)!;
}
export function measure(s:string,size:number,weight:Weight=400){let w=0;for(const c of s)w+=glyph(c,weight).advance;return w*size/1000;}
export function vectorText(s: string,x: number,y: number,size: number,fill: string,used: Set<string>,weight:Weight=400) {
  let position=0;const paths: string[]=[];
  for(const c of s) {
    const g=glyph(c,weight);used.add(weight+c);
    paths.push(`<use href="#${g.id}" x="${position}"/>`);position+=g.advance;
  }
  return `<g transform="translate(${x} ${y}) scale(${size/1000} ${-size/1000})" fill="${fill}">${paths.join('')}</g>`;
}
export function definitions(used: Set<string>) {
  return `<defs>${[...used].map(key=>{const g=glyph(key.slice(3),Number(key.slice(0,3)) as Weight);return `<path id="${g.id}" d="${g.path}"/>`;}).join('')}</defs>`;
}
