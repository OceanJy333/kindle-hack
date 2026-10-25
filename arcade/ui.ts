// input: Voyage Canvas; output: arcade overlay and evdev key geometry; pos: shared gamepad rendering source.
import { Canvas } from '../native/canvas.ts';
import { frame, color, W, H } from '../native/tokens.ts';
import { writeFileSync, mkdirSync } from 'node:fs';
import { resolve } from 'node:path';
const out=resolve(import.meta.dirname,'../build/arcade');mkdirSync(out,{recursive:true});
const c=new Canvas();c.box(0,0,W,H);
c.text('合金弹头',frame.left,78,44,color.ink,700);
c.text('1996 · Neo Geo · Voyage 本机模拟 · 静音',frame.left,128,24,color.sub);
c.button('返回桌面',frame.right-216,32,216,76,'exit');
const cx=254,cy=1150,step=108,size=104;
c.box(cx-size/2,cy-size/2,size,size,color.ink);
for(const [key,dx,dy] of [['KEY_UPARROW',0,-1],['KEY_DOWNARROW',0,1],['KEY_LEFTARROW',-1,0],['KEY_RIGHTARROW',1,0]] as const){
 const x=cx+dx*step,y=cy+dy*step,tip=[x+dx*22,y+dy*22],base=[x-dx*14,y-dy*14],p=[-dy*21,dx*21];
 c.box(x-size/2,y-size/2,size,size,color.ink,'none',0,8);
 c.path(`M ${tip[0]} ${tip[1]} L ${base[0]+p[0]} ${base[1]+p[1]} L ${base[0]-p[0]} ${base[1]-p[1]} Z`,color.paper,0,color.paper);
 c.hit(x-size/2,y-size/2,size,size,'key',key);
}
c.circle(cx,cy,9,color.paper,color.paper,0);
for(const [label,caption,key,x,y] of [['A','射击','KEY_FIRE',932,1078],['B','跳跃','KEY_USE',764,1194],['C','手雷','KEY_GRENADE',932,1310]] as const){
 c.circle(x,y,60,color.ink,color.ink,0);c.center(label,y+18,46,color.paper,700,x-60,120);
 c.hit(x-64,y-64,128,128,'key',key);c.center(caption,y+92,22,color.sub,400,x-70,140);
}
c.button('开始',514,1056,132,76,'key','KEY_ENTER');
c.button('投币',514,1176,132,76,'key','KEY_ESCAPE');
c.center('双指操作',1360,24,color.sub,400,frame.left,400);
c.text('方向 + 射击 / 跳跃可同时按住；压感键对应 A / B。',frame.left,975,24,color.sub);
const result=c.finish();writeFileSync(resolve(out,'overlay.gray'),result.gray);
const regions=result.hits.map(h=>`{${h.x},${h.y},${h.w},${h.h},${h.action==='exit'?'0':h.value}}`);
writeFileSync(resolve(out,'controls.h'),`// input: arcade/ui.ts; output: matching controller hit regions; pos: generated geometry.\n#pragma once\n#include "doomgeneric.h"\nstruct game_button {int x,y,w,h;unsigned char key;};\nstatic const struct game_button game_buttons[]={${regions.join(',')}};\n#define GAME_BUTTON_COUNT ${regions.length}\n`);
writeFileSync(resolve(out,'overlay-preview.png'),result.png);
// This static panel remains event-driven while assets are absent; no fake game frames.
const message=new Canvas();message.box(0,0,960,720);
message.text('游戏文件尚未就绪',48,160,44,color.ink,700);
message.text('需要匹配的初代资源和 Neo Geo BIOS',48,230,30,color.sub);
message.text('mslug.zip  +  neogeo.zip',48,306,32,color.ink,500);
message.text('放入 Kindle 的 /mnt/us/games/roms/',48,398,26,color.sub);
message.text('再从首页游戏入口启动',48,472,28,color.sub);
message.text('模拟器已安装；点击右上角返回桌面。',48,630,26,color.muted);
const full=message.finish().gray,region=new Uint8Array(960*720);
for(let y=0;y<720;y++)region.set(full.subarray(y*W,y*W+960),y*960);
writeFileSync(resolve(out,'missing.gray'),region);
