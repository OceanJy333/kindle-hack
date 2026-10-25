// input: handoff queue and authenticated device; output: silent full-screen notice frames; pos: independent low-traffic channel.
import {createServer} from 'node:net';
import type {Socket} from 'node:net';
import {timingSafeEqual} from 'node:crypto';
import {Canvas,wrap} from './canvas.ts';
import {W,H,frame,color} from './tokens.ts';
import {encodeBands} from './frames.ts';
import {Notifications} from './notifications.ts';
import type {Notice} from './notifications.ts';
export function noticeScreen(n:Notice,count:number,page=0){
 const c=new Canvas();c.box(0,0,W,H);
 c.text(n.status==='done'?'结果已就绪':n.status==='waiting'?'等你接管':'任务遇到问题',frame.left,150,52,color.ink,700);
 c.text(`${n.provider} · ${count} 项待看`,frame.left,212,28,color.sub);
 c.lines(n.title,frame.left,324,frame.width,38,2,color.ink,500,58);
 c.line(frame.left,436,frame.right,436);
 const lines=wrap(n.body||'回到电脑查看这项任务。',frame.width,30),per=13,pages=Math.max(1,Math.ceil(lines.length/per));
 page=Math.min(page,pages-1);
 lines.slice(page*per,(page+1)*per).forEach((s,i)=>c.text(s,frame.left,502+i*48,30));
 if(pages>1){c.button('上一页',56,1160,220,76,'previous',undefined,'outline',page===0);c.center(`${page+1} / ${pages}`,1210,26);c.button('下一页',796,1160,220,76,'next',undefined,'outline',page===pages-1);}
 c.button('已看，继续消遣',56,1280,456,96,'dismiss',undefined,'primary');
 c.button('稍后再看',536,1280,480,96,'later');
 return {...c.finish(),pages};
}
export function notificationServer(queue:Notifications,token:string,bind:string){
 let peer:Socket|undefined,hits:ReturnType<typeof noticeScreen>['hits']=[],page=0,seq=0,waiting=false,dirty=false,snoozed='',key='';
 function push(){
  if(!peer)return;if(waiting){dirty=true;return;}
  const n=queue.queue[0];
  if(!n||n.key===snoozed){const p=Buffer.alloc(32);p.writeUInt32LE(0x31424741);p.writeUInt32LE(64,28);peer.write(p);key='';return;}
  if(key!==n.key){page=0;key=n.key;}
  const screen=noticeScreen(n,queue.queue.length,page);hits=screen.hits;
  const packets=encodeBands(screen.gray,undefined,true,seq+1);seq+=packets.length;waiting=true;dirty=false;
  for(const p of packets)peer.write(p);
  const sent=seq;setTimeout(()=>{if(waiting&&seq===sent)peer?.destroy();},15000).unref();
 }
 const server=createServer(socket=>{
  let text='',auth=false;const timer=setTimeout(()=>{if(!auth)socket.destroy();},5000);
  socket.on('data',chunk=>{
   text+=chunk.toString();if(text.length>4096){socket.destroy();return;}
   let end;while((end=text.indexOf('\n'))>=0){const line=text.slice(0,end);text=text.slice(end+1);const [op,a,b]=line.split(' ');
    if(!auth){const candidate=Buffer.from(a||'');if(op!=='HELLO'||candidate.length!==Buffer.byteLength(token)||!timingSafeEqual(candidate,Buffer.from(token))){socket.destroy();return;}auth=true;clearTimeout(timer);peer?.destroy();peer=socket;waiting=false;snoozed='';push();continue;}
    if(socket!==peer)return;
    if(op==='ACK'&&Number(a)===seq){waiting=false;if(dirty)push();}
    if(op==='TAP'&&!waiting){const hit=hits.find(h=>+a>=h.x&&+a<h.x+h.w&&+b>=h.y&&+b<h.y+h.h);if(!hit)continue;
     if(hit.action==='dismiss'){queue.dismiss();page=0;}
     if(hit.action==='later')snoozed=queue.queue[0]?.key||'';
     if(hit.action==='previous')page=Math.max(0,page-1);
     if(hit.action==='next')page++;
     push();
    }
   }
  });
  socket.on('close',()=>{clearTimeout(timer);if(peer===socket){peer=undefined;waiting=false;}});socket.on('error',()=>{});
 });
 server.listen(18767,bind);
 return {push,close:()=>{peer?.destroy();server.close();}};
}
