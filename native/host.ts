// input: live agent events/native taps; output: changed pixel regions and dispatched tasks; pos: desktop bridge.
import { createServer } from 'node:net';
import type { Socket } from 'node:net';
import { createServer as httpServer } from 'node:http';
import { mkdirSync, readFileSync, writeFileSync, existsSync, appendFileSync } from 'node:fs';
import { randomBytes, timingSafeEqual } from 'node:crypto';
import { resolve } from 'node:path';
import { Sources } from './sources.ts';
import { Dispatcher } from './dispatch.ts';
import { render, screenKey } from './screen.ts';
import { presetPrompt } from './agent-screen.ts';
import type { View, Hit } from './screen.ts';
import type { Task, Provider } from './model.ts';
import { encodeBands } from './frames.ts';
import { Desktop } from './desk.ts';
import { Notifications } from './notifications.ts';
import { notificationServer } from './notification-server.ts';
import { games } from './game-screen.ts';

const root=import.meta.dirname, runtime=resolve(root,'runtime'); mkdirSync(runtime,{recursive:true});
const tokenFile=resolve(runtime,'device-token');
if (!existsSync(tokenFile)) writeFileSync(tokenFile,randomBytes(24).toString('hex'),{mode:0o600});
const token=readFileSync(tokenFile,'utf8').trim();
const current=process.env.BOARD_THREAD_ID || process.env.CODEX_THREAD_ID || '';
const cwd=process.env.BOARD_CWD || resolve(root,'..');
let view: View={mode:'clock',page:'board',filter:'all',provider:'Codex',prompt:'',pageIndex:0,notice:'',cwd};
let hits: Hit[]=[], client: Socket|undefined, previous: Uint8Array|undefined, seq=0, full=true;
let timer: NodeJS.Timeout|undefined, signature='', awaiting=false, queued=false, lastTap=0;
let sources: Sources|undefined, dispatcher: Dispatcher|undefined;
let desktop: Desktop, pages=1;
const notifications=new Notifications(resolve(runtime,'notifications.json'));
const notices=notificationServer(notifications,token,process.env.BOARD_BIND||'127.0.0.1');
const prior=new Map<string,string>(), pending=new Map<number,{at:number;hits:Hit[];gray:Uint8Array;tap:number;full:boolean}>();
const metrics={frames:0,bytes:0,full:0,partial:0,renders:0,taps:0,acks:0,lastRenderMs:0,lastAckMs:0,lastDeviceMs:0,lastTouchMs:0,connections:0};
const record=(type:string,data:object={})=>appendFileSync(resolve(runtime,'events.jsonl'),JSON.stringify({at:new Date().toISOString(),type,...data})+'\n');
function tasks(): Task[] {
  const map=new Map((sources?.list() || []).map(t=>[t.id,t]));
  // The transcript wins once it has newer activity than the screen-dispatched copy.
  for (const t of dispatcher?.list() || []) { const s=map.get(t.id); if(!s || t.updatedAt>=s.updatedAt) map.set(t.id,t); }
  return [...map.values()];
}
function changed() {
  let urgent=false;
  if(notifications.update(tasks()))notices.push();
  for(const t of tasks()) {
    if(prior.has(t.id) && prior.get(t.id)!==t.status && ['done','error','waiting'].includes(t.status)) {
      view.notice=`${t.provider} ${t.status==='done'?'任务已完成':t.status==='error'?'执行出错':'需要处理'} · ${t.title.slice(0,14)}`;
      record('status',{id:t.id,status:t.status,sourceAt:t.updatedAt}); urgent=true; if(view.mode!=='agent')view.agentBadge=true;
    }
    prior.set(t.id,t.status);
  }
  schedule(urgent?0:350);
}
function schedule(ms=0) {
  if(timer && ms>0) return;
  if(timer) clearTimeout(timer);
  timer=setTimeout(()=>{timer=undefined;draw();},ms);
}
function draw() {
  if(!client || awaiting) {queued=true;return;}
  const list=tasks(),desk=desktop.snapshot();
  const nextSignature=screenKey(list,view,current,desk);
  if(nextSignature===signature && !full) return;
  const begin=performance.now(), result=render(list,view,current,desk);
  metrics.renders++;metrics.lastRenderMs=+(performance.now()-begin).toFixed(2);
  const packets=encodeBands(result.gray,previous,full,seq+1);
  signature=nextSignature;
  pages=result.pages;
  if(!packets.length) {hits=result.hits;return;}
  seq+=packets.length;
  writeFileSync(resolve(runtime,'preview.png'),result.png);
  const frameFull=Boolean(packets[0].readUInt32LE(28)&1),bytes=packets.reduce((n,p)=>n+p.length,0);
  metrics.frames++;metrics.bytes+=bytes;metrics[frameFull?'full':'partial']++;
  record('frame',{seq,bytes,regions:packets.map(p=>[8,12,16,20].map(i=>p.readUInt32LE(i))),full:frameFull,renderMs:metrics.lastRenderMs});
  // Only the last rectangle's ACK commits the frame; earlier ACKs have no pending entry.
  pending.set(seq,{at:Date.now(),hits:result.hits,gray:result.gray,tap:lastTap,full:frameFull}); lastTap=0;
  awaiting=true;queued=false;full=false;
  for(const packet of packets)client.write(packet);
  // Recover from a lost ACK by reconnecting with a full frame, never by replaying touch actions.
  const sent=seq;setTimeout(()=>{if(pending.has(sent))client?.destroy();},15000).unref();
}
// One paging rule for buttons and Voyage PagePress keys; article edges continue to the neighbouring story.
function page(dir:number) {
  const mode=view.mode||'agent';
  if(mode==='news'&&view.article){
    const next=(view.articlePage||0)+dir;
    if(next>=0&&next<pages){view.articlePage=next;return;}
    const items=desktop.news.state.items,i=items.findIndex(x=>x.id===view.article)+dir;
    if(i>=0&&i<items.length){view.article=items[i].id;view.articlePage=0;}
  }else if(mode==='news')view.newsPage=Math.max(0,Math.min(pages-1,(view.newsPage||0)+dir));
  else if(mode==='agent'&&view.page==='detail')view.detailPage=Math.max(0,Math.min(pages-1,(view.detailPage||0)+dir));
  else if(mode==='agent'&&view.page==='board')view.pageIndex=Math.max(0,Math.min(pages-1,view.pageIndex+dir));
}
function tap(x:number,y:number) {
  metrics.taps++;lastTap=Date.now();
  const hit=hits.find(h=>x>=h.x&&x<h.x+h.w&&y>=h.y&&y<h.y+h.h);
  record('tap',{x,y,action:hit?.action,value:hit?.value});
  if(!hit) return;
  if(hit.action==='settings'||hit.action==='settings-close'){view.settings=hit.action==='settings';schedule();return;}
  if(['light','light-step','light-auto','display-clean'].includes(hit.action)){
    if(!view.device && hit.action!=='display-clean')return;
    let level=view.device?.light??6,automatic=0;
    if(hit.action==='light')level=Number(hit.value);
    if(hit.action==='light-step')level+=Number(hit.value);
    if(hit.action==='light-auto')automatic=Number(hit.value);
    level=Math.max(0,Math.min(24,level));
    const packet=Buffer.alloc(32);packet.writeUInt32LE(0x31424741);packet.writeUInt32LE(level,8);packet.writeUInt32LE(automatic,12);packet.writeUInt32LE(hit.action==='display-clean'?32:16,28);
    client?.write(packet);return;
  }
  if(hit.action==='games'||hit.action==='games-back'){view.games=hit.action==='games';schedule();return;}
  if(hit.action==='game'){
    const game=games.find(g=>g.id===hit.value);if(!game)return;
    const packet=Buffer.alloc(32);packet.writeUInt32LE(0x31424741);packet.writeUInt32LE(game.code,8);packet.writeUInt32LE(8,28);
    view.games=false;client?.write(packet);record('game-launch',{id:game.id});return;
  }
  if(hit.action==='reader'){const packet=Buffer.alloc(32);packet.writeUInt32LE(0x31424741);packet.writeUInt32LE(4,28);client?.write(packet);return;}
  if(desktop.act(hit,view)){if(view.mode==='agent')view.agentBadge=false;return;}
  const list=tasks();
  switch(hit.action) {
    case 'dismiss':view.notice='';break;
    case 'page':page(Number(hit.value));break;
    case 'agent-filter':view.filter=hit.value as View['filter'];view.pageIndex=0;break;
    case 'detail':view.page='detail';view.selected=hit.value;view.detailPage=0;break;
    case 'back':view.page='board';break;
    case 'compose':view.page='compose';view.replyTo=undefined;view.prompt='';view.cwd=cwd;break;
    case 'reply':view.page='compose';view.replyTo=hit.value;view.prompt='';break;
    case 'compose-cancel':view.page=view.replyTo?'detail':'board';view.prompt='';break;
    case 'workspace':
      const dirs=[...new Set([cwd,...list.map(t=>t.cwd).filter(Boolean)])];
      view.cwd=dirs[(dirs.indexOf(view.cwd||cwd)+1)%dirs.length];break;
    case 'provider':view.provider=hit.value as Provider;break;
    case 'preset':view.prompt=presetPrompt(hit.value!,view.provider);break;
    case 'key':if(view.prompt.length<2000)view.prompt+=hit.value;break;
    case 'delete':view.prompt=Array.from(view.prompt).slice(0,-1).join('');break;
    case 'shift':view.shifted=!view.shifted;break;
    case 'send':{
      if(!view.prompt.trim())return;
      const target=view.replyTo?list.find(t=>t.id===view.replyTo):undefined;
      const task=target?dispatcher!.submit(target.provider,view.prompt,target.cwd||cwd,target):dispatcher!.submit(view.provider,view.prompt,view.cwd||cwd);
      view={...view,page:'detail',selected:task.id,detailPage:0,replyTo:undefined,prompt:'',notice:`已发送给 ${task.provider}`};break;
    }
  }
  schedule();
}
// The screen keeps following a new task when the agent assigns its real session id.
dispatcher=new Dispatcher(resolve(runtime,'dispatch.jsonl'),changed,(from,to)=>{if(view.selected===from)view.selected=to;if(view.replyTo===from)view.replyTo=to;});
sources=new Sources(current,changed);
desktop=new Desktop(runtime,()=>schedule(),message=>{view.notice=message;record('visual-notice',{message});});
for(const signal of ['SIGTERM','SIGINT'] as const)process.on(signal,()=>{desktop.close();notices.close();process.exit(0);});
createServer(socket=>{
  socket.setNoDelay(true);let text='',authenticated=false;
  const authTimeout=setTimeout(()=>{if(!authenticated)socket.destroy();},5000);
  socket.on('data',data=>{
    text+=data.toString();if(text.length>4096){socket.destroy();return;}
    let end:number;
    while((end=text.indexOf('\n'))>=0) {
      const line=text.slice(0,end);text=text.slice(end+1);
      if(!authenticated) {
        // HELLO <token> [tab]: the device names the tab it was asked for, so the first frame is already the right one.
        const [word,secret='',tab]=line.split(' '),candidate=Buffer.from(secret);
        if(word!=='HELLO' || candidate.length!==token.length || !timingSafeEqual(candidate,Buffer.from(token))){socket.destroy();return;}
        if(tab && desktop.act({x:0,y:0,w:0,h:0,action:'mode',value:tab},view) && view.mode==='agent')view.agentBadge=false;
        clearTimeout(authTimeout);authenticated=true;client?.destroy();client=socket;previous=undefined;full=true;awaiting=false;pending.clear();metrics.connections++;record('connected');schedule();continue;
      }
      if(socket!==client)return;
      const parts=line.split(' ');
      if(parts[0]==='DEVICE' && parts.length===5 && parts.slice(1).every(p=>/^-?\d+$/.test(p))){
        const [percent,light,automatic,charging]=parts.slice(1).map(Number);
        if(percent>=-1&&percent<=100&&light>=-1&&light<=24&&automatic>=0&&automatic<=1&&charging>=0&&charging<=2){view.device={percent,light,automatic:Boolean(automatic),charging};schedule();}
      }
      if(parts[0]==='ACK') {
        const ack=pending.get(+parts[1]);if(!ack)continue;
        metrics.acks++;metrics.lastAckMs=Date.now()-ack.at;metrics.lastDeviceMs=+parts[2]/1000;
        if(ack.tap)metrics.lastTouchMs=Date.now()-ack.tap;
        record('ack',{seq:+parts[1],roundTripMs:metrics.lastAckMs,deviceMs:metrics.lastDeviceMs,touchMs:ack.tap?metrics.lastTouchMs:undefined});
        hits=ack.hits;previous=ack.gray;pending.delete(+parts[1]);awaiting=false;
        if(!ack.full && metrics.partial && metrics.partial%30===0)full=true;
        if(queued)schedule();
      }
      if(parts[0]==='TAP' && /^\d+$/.test(parts[1]) && /^\d+$/.test(parts[2]))tap(+parts[1],+parts[2]);
      if(parts[0]==='KEY' && /^-?1$/.test(parts[1])){metrics.taps++;lastTap=Date.now();page(+parts[1]);schedule();}
    }
  });
  socket.on('close',()=>{clearTimeout(authTimeout);if(socket===client){client=undefined;awaiting=false;pending.clear();record('disconnected');}});
  socket.on('error',e=>record('socket-error',{message:e.message}));
}).listen(18765,process.env.BOARD_BIND || '127.0.0.1',()=>console.log(`Native bridge listening on ${process.env.BOARD_BIND || '127.0.0.1'}:18765`));
// Diagnostic JSON only. The display connection is raw TCP, not HTTP or a browser.
httpServer((req,res)=>{
  res.setHeader('Content-Type','application/json');
  if(req.url==='/metrics')res.end(JSON.stringify({...metrics,connected:Boolean(client),memory:process.memoryUsage()},null,2));
  else if(req.url==='/state'){const desk=desktop.snapshot();res.end(JSON.stringify({view,desk:{...desk,media:{...desk.media,artwork:undefined,hasArtwork:Boolean(desk.media.artwork)}},tasks:tasks().map(({source,...t})=>t)},null,2));}
  else{res.statusCode=404;res.end('{}');}
}).listen(18766,'127.0.0.1');
