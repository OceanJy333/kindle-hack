// input: layout primitives in native Voyage pixels; output: grayscale screen and touch map; pos: single rendering source.
import { Resvg } from '@resvg/resvg-js';
import { glyph, measure, vectorText, definitions } from './glyphs.ts';
import type { Weight } from './glyphs.ts';
import { W, H, color } from './tokens.ts';
export type Hit = { x:number; y:number; w:number; h:number; action:string; value?:string };
export type Variant = 'primary'|'outline'|'ghost';
export { measure };
// Paragraph-aware greedy wrap; CJK can break anywhere, Latin words stay whole when they fit.
export function wrap(s:string,width:number,size:number,weight:Weight=400) {
  const out:string[]=[];
  for(const paragraph of s.split('\n')){
    let line='',used=0;
    const tokens=paragraph.match(/[A-Za-z0-9_./:%@#+\-]+|\s+|./gu)||[];
    for(const token of tokens){
      const w=measure(token,size,weight);
      if(used+w<=width||!line&&w<=width){line+=token;used+=w;continue;}
      if(w>width){for(const c of token){const cw=glyph(c,weight).advance*size/1000;if(used+cw>width){out.push(line.trimEnd());line='';used=0;}line+=c;used+=cw;}continue;}
      out.push(line.trimEnd());line=token.trimStart();used=measure(line,size,weight);
    }
    out.push(line.trimEnd());
  }
  while(out.length&&!out.at(-1))out.pop();
  return out;
}
export function ellipsize(s:string,width:number,size:number,weight:Weight=400){
  if(measure(s,size,weight)<=width)return s;
  const chars=[...s];while(chars.length&&measure(chars.join('')+'…',size,weight)>width)chars.pop();
  return chars.join('')+'…';
}
export class Canvas {
  svg:string[]=[];hits:Hit[]=[];used=new Set<string>();pages=1;
  box(x:number,y:number,w:number,h:number,fill=color.paper,stroke='none',strokeWidth=2,radius=0) {
    this.svg.push(`<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="${radius}" fill="${fill}" stroke="${stroke}" stroke-width="${strokeWidth}"/>`);
  }
  text(s:string,x:number,y:number,size=30,fill=color.ink,weight:Weight=400){this.svg.push(vectorText(s,x,y,size,fill,this.used,weight));}
  right(s:string,x:number,y:number,size=30,fill=color.ink,weight:Weight=400){this.text(s,x-measure(s,size,weight),y,size,fill,weight);}
  center(s:string,y:number,size=30,fill=color.ink,weight:Weight=400,x0=0,w=W){this.text(s,x0+(w-measure(s,size,weight))/2,y,size,fill,weight);}
  // Draws up to max wrapped lines (last one ellipsized) and returns how many lines were used.
  lines(s:string,x:number,y:number,width:number,size:number,max:number,fill=color.ink,weight:Weight=400,line=Math.round(size*1.6)){
    const ls=wrap(s,width,size,weight),shown=ls.slice(0,max);
    shown.forEach((l,i)=>this.text(i===max-1&&ls.length>max?ellipsize(l+'…',width,size,weight):l,x,y+i*line,size,fill,weight));
    return shown.length;
  }
  hit(x:number,y:number,w:number,h:number,action:string,value?:string){this.hits.push({x,y,w,h,action,value});}
  button(s:string,x:number,y:number,w:number,h:number,action:string,value?:string,variant:Variant='outline',disabled=false,size=28){
    if(variant==='primary')this.box(x,y,w,h,disabled?color.line:color.ink,'none',0,8);
    else if(variant==='outline')this.box(x+1,y+1,w-2,h-2,color.paper,disabled?color.line:color.ink,2,8);
    const fill=variant==='primary'?color.paper:disabled?color.muted:color.ink;
    this.text(s,variant==='ghost'?x:x+(w-measure(s,size,500))/2,y+h/2+size*.36,size,fill,500);
    if(!disabled)this.hit(x,y,w,h,action,value);
  }
  line(x1:number,y1:number,x2:number,y2:number,width=2,stroke=color.ink){this.svg.push(`<path d="M ${x1} ${y1} L ${x2} ${y2}" fill="none" stroke="${stroke}" stroke-width="${width}" stroke-linecap="round"/>`);}
  path(d:string,stroke=color.ink,width=3,fill='none'){this.svg.push(`<path d="${d}" fill="${fill}" stroke="${stroke}" stroke-width="${width}" stroke-linecap="round" stroke-linejoin="round"/>`);}
  circle(x:number,y:number,r:number,fill='none',stroke=color.ink,width=2){this.svg.push(`<circle cx="${x}" cy="${y}" r="${r}" fill="${fill}" stroke="${stroke}" stroke-width="${width}"/>`);}
  image(data:string,mime:string,x:number,y:number,w:number,h:number){if(/^image\/(jpeg|png|webp)$/.test(mime)&&/^[a-zA-Z0-9+/=]+$/.test(data))this.svg.push(`<image href="data:${mime};base64,${data}" x="${x}" y="${y}" width="${w}" height="${h}" preserveAspectRatio="xMidYMid slice"/>`);}
  finish(){
    const doc=`<svg xmlns="http://www.w3.org/2000/svg" width="${W}" height="${H}" viewBox="0 0 ${W} ${H}">${definitions(this.used)}${this.svg.join('')}</svg>`;
    const image=new Resvg(doc,{font:{loadSystemFonts:false}}).render(),rgba=image.pixels,gray=new Uint8Array(W*H);
    // EPDC shows 16 levels; quantize here so identical-looking frames produce identical bytes.
    for(let i=0;i<gray.length;i++)gray[i]=Math.round((.2126*rgba[i*4]+.7152*rgba[i*4+1]+.0722*rgba[i*4+2])/17)*17;
    return {gray,hits:this.hits,pages:this.pages,png:image.asPng(),svg:doc};
  }
}
