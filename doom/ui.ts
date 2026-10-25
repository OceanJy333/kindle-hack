// input: Voyage design tokens; output: native game overlay and matching C hit regions; pos: single game-control layout.
import { Canvas } from '../native/canvas.ts';
import { frame, color, W, H } from '../native/tokens.ts';
import { writeFileSync, mkdirSync } from 'node:fs';
import { resolve } from 'node:path';
const out=resolve(import.meta.dirname,'../build/doom');mkdirSync(out,{recursive:true});
const c=new Canvas();c.box(0,0,W,H);
c.text('DOOM',frame.left,78,44,color.ink,700);
c.text('1993 · Voyage 原生实验 · 静音',frame.left,128,24,color.sub);
c.button('退出实验',frame.right-216,32,216,76,'exit');
c.text('左手方向 · 右手 A/B · 压感键可开火 / 使用',frame.left,972,24,color.sub);
const cx=254,cy=1150,step=108,size=104;
c.box(cx-size/2,cy-size/2,size,size,color.ink);
const directions=[['KEY_UPARROW',0,-1],['KEY_DOWNARROW',0,1],['KEY_LEFTARROW',-1,0],['KEY_RIGHTARROW',1,0]] as const;
for(const [key,dx,dy] of directions){
 const x=cx+dx*step,y=cy+dy*step;
 c.box(x-size/2,y-size/2,size,size,color.ink,'none',0,8);
 const tip=[x+dx*22,y+dy*22],base=[x-dx*14,y-dy*14],perp=[-dy*21,dx*21];
 c.path(`M ${tip[0]} ${tip[1]} L ${base[0]+perp[0]} ${base[1]+perp[1]} L ${base[0]-perp[0]} ${base[1]-perp[1]} Z`,color.paper,0,color.paper);
 c.hit(x-size/2,y-size/2,size,size,'key',key);
}
c.circle(cx,cy,9,color.paper,color.paper,0);
c.center('移动 / 转向',1358,24,color.sub,400,frame.left,400);
for(const [label,caption,key,x,y] of [['A','开火','KEY_FIRE',932,1092],['B','使用','KEY_USE',780,1226]] as const){
 c.circle(x,y,64,color.ink,color.ink,0);c.center(label,y+18,48,color.paper,700,x-64,128);
 c.hit(x-70,y-70,140,140,'key',key);c.center(caption,y+104,26,color.sub,400,x-80,160);
}
c.button('确认',514,1056,132,76,'key','KEY_ENTER');
c.button('菜单',514,1176,132,76,'key','KEY_ESCAPE');
c.center('START / SELECT',1306,19,color.muted,400,484,192);
c.text('按住方向移动；A 开火，B 开门。退出实验返回桌面。',frame.left,1410,22,color.muted);
const result=c.finish();writeFileSync(resolve(out,'overlay.gray'),result.gray);
const regions=result.hits.map(h=>`{${h.x},${h.y},${h.w},${h.h},${h.action==='exit'?'0':h.value}}`);
writeFileSync(resolve(out,'controls.h'),`// input: ui.ts control layout; output: matching evdev hit targets; pos: generated game input geometry.\n#pragma once\n#include "doomkeys.h"\nstruct game_button {int x,y,w,h;unsigned char key;};\nstatic const struct game_button game_buttons[]={${regions.join(',')}};\n#define GAME_BUTTON_COUNT ${regions.length}\n`);
writeFileSync(resolve(out,'overlay-preview.png'),result.png);
