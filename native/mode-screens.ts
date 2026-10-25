// input: real desk state; output: clock, calendar and focus alongside home music; pos: native mode layouts.
import { Canvas } from './canvas.ts';
import { frame, color, W } from './tokens.ts';
import { dateParts, focusRemaining } from './desk-model.ts';
import type { Desk } from './desk-model.ts';
import type { Task } from './model.ts';
const weekday=(now:number)=>new Intl.DateTimeFormat('zh-CN',{weekday:'long',timeZone:'Asia/Taipei'}).format(now);
function face(c:Canvas,p:Record<string,string>){
  const cx=W/2,cy=350,r=172;c.circle(cx,cy,r,'none',color.ink,4);
  for(let i=0;i<60;i++){const a=i*Math.PI/30,major=i%5===0,inner=r-(major?(i%15?26:38):12);
    c.line(cx+Math.sin(a)*inner,cy-Math.cos(a)*inner,cx+Math.sin(a)*(r-8),cy-Math.cos(a)*(r-8),major?(i%15?5:8):2,major?color.ink:color.muted);}
  const minute=Number(p.minute)*Math.PI/30,hour=(Number(p.hour)%12+Number(p.minute)/60)*Math.PI/6;
  c.line(cx,cy,cx+Math.sin(hour)*96,cy-Math.cos(hour)*96,14);c.line(cx,cy,cx+Math.sin(minute)*140,cy-Math.cos(minute)*140,8);
  c.circle(cx,cy,12,color.ink,color.ink,0);
}
export function clockScreen(c:Canvas,d:Desk,tasks:Task[]){
  const p=dateParts(d.now);

  c.button('设置',frame.right-150,150,150,76,'settings');
  if(d.clockStyle==='digital'){
    c.center(`${Number(p.month)} 月 ${Number(p.day)} 日  ${weekday(d.now)}`,218,32,color.sub,500);
    c.center(`${p.hour}:${p.minute}`,474,220,color.ink,500);
  }else{face(c,p);c.text(`${Number(p.month)}/${Number(p.day)} ${weekday(d.now)}`,frame.left,190,24,color.sub,500);}
  c.hit(frame.left,150,frame.width,390,'clock-style');
  // Two glance links connect the clock to the other desk modes.
  const running=tasks.filter(t=>t.status==='running').length,news=d.news.items.length;
  c.center(`${running?`${running} 个 Agent 运行中`:'Agent 空闲'}   ·   AI 日报 ${news} 条`,592,26,color.sub);
  c.hit(0,552,W/2,70,'mode','agent');c.hit(W/2,552,W/2,70,'mode','news');
  c.line(frame.left,646,frame.right,646,2,color.ink);
  c.text('让 Agent 干活，休息一下',frame.left,712,34,color.ink,500);
  const choices=[['读一会儿','继续微信读书','reader',''],['玩一局','Doom / 合金弹头','games',''],['看看 AI 新闻','卡兹克 AIHOT','mode','news']];
  choices.forEach(([label,description,action,value],i)=>{
    const y=752+i*100;
    c.text(label,frame.left,y+40,30,color.ink,500);
    c.text(description,360,y+40,26,color.sub);
    c.right('进入',frame.right,y+40,28,color.ink,500);
    c.hit(frame.left,y,frame.width,88,action,value);
    c.line(frame.left,y+88,frame.right,y+88,1,color.line);
  });
}
function calendar(c:Canvas,p:Record<string,string>,x:number,y:number){
  c.text(`${p.year} 年 ${Number(p.month)} 月`,x,y,28,color.ink,500);
  const first=new Date(Date.UTC(+p.year,+p.month-1,1)).getUTCDay(),days=new Date(Date.UTC(+p.year,+p.month,0)).getUTCDate(),cell=64;
  [...'日一二三四五六'].forEach((s,i)=>c.center(s,y+52,22,color.muted,400,x+i*cell-14,cell));
  for(let n=1;n<=days;n++){
    const i=first+n-1,cx=x+(i%7)*cell-14,cy=y+102+Math.floor(i/7)*52,today=n===Number(p.day);
    if(today)c.box(cx+8,cy-32,cell-16,44,color.ink,'none',0,6);
    c.center(String(n),cy,24,today?color.paper:i%7===0||i%7===6?color.sub:color.ink,today?700:400,cx,cell);
  }
}
function focus(c:Canvas,d:Desk,x:number,y:number){
  const f=d.focus,left=focusRemaining(f,d.now),end=f.endAt?dateParts(f.endAt):undefined;
  c.text(f.finished?'专注完成':'专注',x,y,28,color.ink,500);
  c.text(String(Math.ceil(left/60000)),x,y+138,116,color.ink,500);
  c.text('分钟',x+(Math.ceil(left/60000)>=10?146:84),y+138,26,color.sub);
  c.text(f.finished?'休息一下，再开始一轮':end?`进行中 · ${end.hour}:${end.minute} 结束`:left===25*60000?'25 分钟一轮':'已暂停',x,y+196,24,color.muted);
  c.button(f.endAt?'暂停':f.finished?'再开始':left===25*60000?'开始':'继续',x,y+252,200,76,'focus-toggle',undefined,'primary');
  c.button('重置',x+216,y+252,frame.right-x-216,76,'focus-reset');
}
