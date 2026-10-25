// input: local agent transcripts and metadata; output: authenticated JSON events, never pixels; pos: computer-side data adapter.
import {createServer} from 'node:net';import type {Socket} from 'node:net';
import {readFileSync} from 'node:fs';import {timingSafeEqual} from 'node:crypto';import {resolve} from 'node:path';
import {Sources} from './sources.ts';import {Desktop} from './desk.ts';import {Notifications} from './notifications.ts';
const runtime=resolve(import.meta.dirname,'runtime'),token=readFileSync(resolve(runtime,'device-token'),'utf8').trim();
const queue=new Notifications(resolve(runtime,'notifications.json')),peers=new Set<Socket>();
let sources:Sources|undefined,desktop:Desktop|undefined,last='',timer:NodeJS.Timeout|undefined;
function push(){
 if(!sources||!desktop)return;
 const tasks=sources.list();queue.update(tasks);const d=desktop.snapshot();
 const snapshot={tasks:tasks.map(({source,...t})=>t),notices:queue.queue,news:d.news,media:{...d.media,artwork:undefined},at:Date.now()};
 const key=JSON.stringify({...snapshot,at:0});if(key===last)return;last=key;
 const data=JSON.stringify(snapshot)+'\n';for(const p of peers){if(p.writableLength>1048576)p.destroy();else p.write(data);}
}
function changed(){if(timer)return;timer=setTimeout(()=>{timer=undefined;push();},100);}
sources=new Sources(process.env.CODEX_THREAD_ID||'',changed);
desktop=new Desktop(runtime,changed,()=>{});
const server=createServer(p=>{
 let text='',auth=false;const timeout=setTimeout(()=>{if(!auth)p.destroy();},5000);
 p.on('data',chunk=>{text+=chunk;if(text.length>16384){p.destroy();return;}let end;
 while((end=text.indexOf('\n'))>=0){const line=text.slice(0,end);text=text.slice(end+1);
 if(!auth){const [op,secret='']=line.split(' ');const bytes=Buffer.from(secret);if(op!=='HELLO'||bytes.length!==Buffer.byteLength(token)||!timingSafeEqual(bytes,Buffer.from(token))){p.destroy();return;}auth=true;clearTimeout(timeout);peers.add(p);last='';push();continue;}
 try{const a=JSON.parse(line);if(a.action==='dismiss'&&queue.queue[0]?.key===a.key){queue.dismiss();last='';push();}}catch{}
 }});p.on('close',()=>{clearTimeout(timeout);peers.delete(p)});p.on('error',()=>{});
});server.listen(18768,process.env.BOARD_BIND||'127.0.0.1',()=>console.log('Agent data bridge ready (JSON only, port 18768)'));
for(const s of ['SIGTERM','SIGINT'] as const)process.on(s,()=>{desktop?.close();server.close();process.exit(0)});
