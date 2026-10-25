// input: real current-player state; output: compact homepage music controls; pos: shared native home component.
import { Canvas, ellipsize } from './canvas.ts';
import { frame, color } from './tokens.ts';
import { duration, position } from './desk-model.ts';
import type { Desk } from './desk-model.ts';

export function musicWidget(c:Canvas,d:Desk){
  const m=d.media,active=Boolean(m.title),top=frame.bottom-210,size=144;
  const x=frame.left,textX=x+size+24,textWidth=510,elapsed=position(m,d.now);
  c.line(frame.left,top,frame.right,top,1,color.line);
  if(m.artwork&&m.mime)c.image(m.artwork,m.mime,x,top+28,size,size);
  else{
    c.box(x,top+28,size,size,color.soft);
    c.circle(x+size/2,top+28+size/2,48,color.paper,color.ink,2);
    c.circle(x+size/2,top+28+size/2,16,color.ink);
  }
  c.text(ellipsize(active?m.title:'电脑还没有播放音乐',textWidth,30,500),textX,top+65,30,color.ink,500);
  c.text(ellipsize(active?(m.artist||m.album||'未知艺术家'):'播放后自动显示曲目',textWidth,24),textX,top+108,24,color.sub);
  if(active){
    const barWidth=330,by=top+148;
    c.box(textX,by,barWidth,5,color.soft);
    if(m.duration>0)c.box(textX,by,barWidth*Math.min(1,elapsed/m.duration),5,color.ink);
    c.text(ellipsize(m.error||`${duration(elapsed)} / ${duration(m.duration)}`,330,21),textX,top+187,21,color.muted);
    c.right(m.playing?'正在播放':'已暂停',textX+textWidth,top+187,22,color.sub);
    transport(c,m.playing,frame.right-150,top+98);
  }else c.button('刷新状态',frame.right-210,top+64,210,76,'music-refresh');
  if(m.error&&!active)c.text(ellipsize(m.error,textWidth,22),textX,top+187,22,color.muted);
}
function transport(c:Canvas,playing:boolean,cx:number,cy:number){
  c.circle(cx,cy,42,color.ink,color.ink,0);
  if(playing){c.box(cx-15,cy-19,10,38,color.paper);c.box(cx+5,cy-19,10,38,color.paper);}
  else c.path(`M ${cx-11} ${cy-23} L ${cx+23} ${cy} L ${cx-11} ${cy+23} Z`,color.paper,2,color.paper);
  c.hit(cx-48,cy-48,96,96,'music','toggle-play-pause');
  for(const dir of [-1,1]){
    const x=cx+dir*84;
    c.path(`M ${x-dir*15} ${cy-17} L ${x+dir*3} ${cy} L ${x-dir*15} ${cy+17} Z M ${x} ${cy-17} L ${x+dir*18} ${cy} L ${x} ${cy+17} Z`,color.ink,2,color.ink);
    c.hit(x-36,cy-44,72,88,'music',dir<0?'previous-track':'next-track');
  }
}
