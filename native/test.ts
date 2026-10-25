// input: protocol/state fixtures; output: correctness assertions; pos: native bridge regression checks.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { encode,encodeBands,W,H } from './frames.ts';
import { codexEvent,claudeEvent } from './model.ts';
import type { Task } from './model.ts';
import { screenKey,render } from './screen.ts';
import { emptyDesk,focusRemaining,position,dayKey } from './desk-model.ts';
import { Desktop,toggleFocus } from './desk.ts';
import { parseFeed,News,NEWS_FEED } from './news.ts';
import { mkdtempSync,readFileSync,rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { mediaState } from './media.ts';
import { inflateSync } from 'node:zlib';
function task(): Task { return {id:'test',provider:'Codex',title:'测试',cwd:'/tmp',status:'unknown',progress:'',updatedAt:0,source:'fixture'}; }
function apply(packet: Buffer, frame: Uint8Array) {
  const [x,y,w,h,n]=[8,12,16,20,24].map(i=>packet.readUInt32LE(i));
  assert.equal(packet.length,32+n);const pixels: number[]=[];
  const data=packet.readUInt32LE(28)&2?inflateSync(packet.subarray(32)):packet.subarray(32);
  for(let i=0;i<data.length;i+=2) {assert.ok(data[i]>0);for(let j=0;j<data[i];j++)pixels.push(data[i+1]);}
  assert.equal(pixels.length,w*h);
  for(let row=0;row<h;row++)frame.set(pixels.slice(row*w,(row+1)*w),(y+row)*W+x);
}
test('unchanged frame emits nothing; edge and full updates reconstruct exact pixels',()=>{
  const before=new Uint8Array(W*H).fill(255), after=before.slice();
  assert.equal(encode(before,before),undefined);
  after[0]=0;after[W*H-1]=17;after[513*W+457]=34;
  const reconstructed=before.slice();apply(encode(after,before)!,reconstructed);assert.deepEqual(reconstructed,after);
  const full=new Uint8Array(W*H);apply(encode(after,undefined,true)!,full);assert.deepEqual(full,after);
});
test('small dirty patch stays small and pixel exact',()=>{
  const before=new Uint8Array(W*H).fill(255),after=before.slice();
  for(let y=210;y<232;y++)for(let x=217;x<287;x++)after[y*W+x]=(x%2)*255;
  const packet=encode(after,before)!;assert.ok(packet.length<5000);apply(packet,before);assert.deepEqual(before,after);
});
test('completion needs matching turn and reasoning never enters displayed progress',()=>{
  const t=task();codexEvent(t,{type:'event_msg',payload:{type:'task_started',turn_id:'active'}});
  codexEvent(t,{type:'response_item',payload:{type:'reasoning',text:'private'}});assert.equal(t.progress,'');
  codexEvent(t,{type:'event_msg',payload:{type:'task_complete',turn_id:'other',last_agent_message:'wrong'}});assert.equal(t.status,'running');
  codexEvent(t,{type:'event_msg',payload:{type:'task_complete',turn_id:'active',last_agent_message:'完成'}});assert.equal(t.status,'done');assert.equal(t.progress,'完成');
});
test('Claude tool result is not a new human task; final turn ends activity',()=>{
  const t=task();claudeEvent(t,{type:'user',message:{content:'读项目'}});assert.equal(t.status,'running');
  claudeEvent(t,{type:'assistant',message:{content:[{type:'thinking',thinking:'private'},{type:'text',text:'结论'}],stop_reason:'end_turn'}});
  claudeEvent(t,{type:'user',message:{content:[{type:'tool_result',content:'output'}]}});assert.equal(t.status,'done');assert.equal(t.progress,'结论');
});
test('hidden progress does not cause compose rendering, completion still changes count',()=>{
  const t=task(),view={page:'compose' as const,provider:'Codex' as const,prompt:'',pageIndex:0,notice:''};
  t.status='running';const before=screenKey([t],view,'');t.progress='new';t.updatedAt=Date.now();assert.equal(screenKey([t],view,''),before);
  t.status='done';assert.notEqual(screenKey([t],view,''),before);
});
test('focus pauses/resumes against the clock and restarts after completion',()=>{
  const initial=emptyDesk().focus,started=toggleFocus(initial,1000);
  assert.equal(focusRemaining(started,61000),24*60000);
  const paused=toggleFocus(started,61000);assert.equal(focusRemaining(paused,999999),24*60000);
  const resumed=toggleFocus(paused,100000);assert.equal(resumed.endAt,1540000);
  assert.equal(focusRemaining(resumed,1600000),0);
  assert.equal(toggleFocus({endAt:0,remainingMs:0,finished:true},1600000).remainingMs,25*60000);
});
test('focus deadline produces one visual notice and remains completed',()=>{
  const notices:string[]=[];let saved=0;
  const desk={focus:{endAt:100,remainingMs:50,finished:false},save:()=>saved++,notify:(s:string)=>notices.push(s),changed:()=>{}};
  Desktop.prototype.tick.call(desk as unknown as Desktop,100);Desktop.prototype.tick.call(desk as unknown as Desktop,200);
  assert.equal(desk.focus.finished,true);assert.equal(desk.focus.remainingMs,0);assert.equal(saved,1);assert.deepEqual(notices,['专注已完成 · 休息一下']);
});
test('media follows metadata, preserves paused position and clamps playback progress',()=>{
  const m=mediaState({title:'测试曲目',duration:180,elapsedTime:178,playing:true,timestamp:'2026-10-13T04:00:00Z'});
  assert.equal(position(m,m.sampleAt+10000),180);m.playing=false;assert.equal(position(m,m.sampleAt+10000),178);
  assert.equal(mediaState(null).title,'');assert.equal(mediaState(null).playing,false);
});
const newsXml='<rss><channel><item><title>中文标题</title><link>https://aihot.news/items/one</link><guid isPermaLink="false">one</guid><pubDate>Mon, 12 Oct 2026 08:00:00 GMT</pubDate><description><![CDATA[<p>原文 A &amp; B &#x4E2D;</p><p>🔗 <a href="https://example.com/original?a=1&amp;b=2">阅读原文</a></p><p>via AIHOT · <a href="https://aihot.news/items/one">原文地址</a></p>]]></description></item></channel></rss>';
test('AIHOT Chinese text, GUID, publication date and both source links are preserved',()=>{
  const now=Date.parse('2026-10-13T04:00:00Z');
  const items=parseFeed(newsXml,now);assert.equal(items.length,1);assert.equal(dayKey(items[0].publishedAt),'2026-10-12');
  assert.equal(items[0].id,'one');assert.equal(items[0].source,'AIHOT');assert.equal(items[0].title,'中文标题');assert.equal(items[0].summary,'原文 A & B 中');
  assert.equal(items[0].url,'https://aihot.news/items/one');assert.equal(items[0].originalUrl,'https://example.com/original?a=1&b=2');
  assert.equal(parseFeed(newsXml,now-2*86400000).length,0);
  assert.equal(parseFeed(newsXml.replace('https://aihot.news/items/one','javascript:alert(1)'),now).length,0);
});
test('RSS refresh reuses ETag and keeps the last successful issue on failure',async t=>{
  const root=mkdtempSync(join(tmpdir(),'kindle-news-')),requests:any[]=[];
  const responses=[new Response(newsXml,{headers:{etag:'"issue-1"'}}),new Response(null,{status:304}),new Response('',{status:503})];
  t.mock.method(globalThis,'fetch',async(url:any,options:any)=>{requests.push({url,headers:options.headers});return responses.shift()!;});
  const news=Object.assign(Object.create(News.prototype),{root,changed:()=>{},state:emptyDesk().news,etag:'',attemptAt:0}) as News;
  try{
    await news.refresh();const before=structuredClone(news.state.items);assert.equal(news.etag,'"issue-1"');
    await news.refresh();assert.equal(requests[1].headers['If-None-Match'],'"issue-1"');assert.deepEqual(news.state.items,before);
    assert.equal(JSON.parse(readFileSync(join(root,'news.json'),'utf8')).feed,NEWS_FEED);
    await news.refresh();assert.deepEqual(news.state.items,before);assert.match(news.state.error,/503/);assert.equal(news.state.loading,false);
    assert.ok(requests.every(r=>r.url===NEWS_FEED));
  }finally{rmSync(root,{recursive:true,force:true});}
});
test('clock ignores hidden progress and sub-minute ticks; media advances once a minute',()=>{
  const t=task(),desk=emptyDesk();desk.now=Date.parse('2026-10-13T04:00:01Z');
  const view={mode:'clock' as const,page:'board' as const,provider:'Codex' as const,prompt:'',pageIndex:0,notice:''};
  const before=screenKey([t],view,'',desk);t.progress='hidden';desk.now+=10000;assert.equal(screenKey([t],view,'',desk),before);
  desk.now+=60000;assert.notEqual(screenKey([t],view,'',desk),before);
  desk.media=mediaState({title:'曲目',playing:true,elapsedTime:0,duration:180});desk.media.sampleAt=desk.now;
  const music=view,key=screenKey([t],music,'',desk);desk.now+=15000;assert.equal(screenKey([t],music,'',desk),key);
  desk.now+=60000;assert.notEqual(screenKey([t],music,'',desk),key);
});
test('homepage exposes music controls without a separate music tab',()=>{
  const desk=emptyDesk();desk.media=mediaState({title:'视觉测试曲目',artist:'测试数据',playing:true,elapsedTime:50,duration:180});
  const result=render([],{mode:'clock',page:'board',provider:'Codex',prompt:'',pageIndex:0,notice:''},'',desk);
  assert.ok(result.png.length>0);assert.ok(result.hits.some(h=>h.action==='clock-style'));
  assert.ok(!result.hits.some(h=>h.action==='mode'&&h.value==='music'));
  assert.deepEqual(result.hits.filter(h=>h.action==='music').map(h=>h.value).sort(),['next-track','previous-track','toggle-play-pause']);
});
test('news pages fill the screen and every row is tappable',()=>{
  const desk=emptyDesk();desk.news.items=Array.from({length:30},(_,i)=>({id:'n'+i,title:i%2?'短标题':'一个足够长、需要折成两行显示的中文新闻标题，用来检查每页能放下多少条内容',summary:'摘要',source:'AIHOT',url:'https://aihot.news/items/'+i,publishedAt:desk.now-i*60000}));
  const r=render([],{mode:'news',page:'board',provider:'Codex',prompt:'',pageIndex:0,notice:''},'',desk);
  const rows=r.hits.filter(h=>h.action==='news-article');assert.ok(rows.length>=6,`only ${rows.length} rows`);assert.ok(r.pages>1);
  for(const h of rows)assert.ok(h.y+h.h<=1324-104);
});
test('agent detail pages the full latest reply and offers a reply into the same session',()=>{
  const t={...task(),status:'done' as const,progress:'第一段。\n'+'很长的回答内容。'.repeat(200)};
  const r=render([t],{mode:'agent',page:'detail',selected:'test',provider:'Codex',prompt:'',pageIndex:0,notice:''},'');
  assert.ok(r.pages>=3);assert.ok(r.hits.some(h=>h.action==='reply'&&h.value==='test'));
});
test('tab bar geometry matches the device-side switch zone',()=>{
  const r=render([],{mode:'clock',page:'board',provider:'Codex',prompt:'',pageIndex:0,notice:''},'');
  const tabs=r.hits.filter(h=>(h.action==='mode'||h.action==='reader')&&h.y>=1324);
  assert.equal(tabs.length,4);for(const [i,h] of tabs.entries()){assert.equal(Math.round(h.x),Math.round(i*W/4));assert.equal(h.y,H-124);}
});
test('separate changes become separate small rectangles that rebuild the exact frame',()=>{
  const before=new Uint8Array(W*H).fill(255),after=before.slice();
  for(let x=1000;x<1016;x++)after[60*W+x]=0;for(let x=120;x<900;x++)after[980*W+x]=0;
  const packets=encodeBands(after,before);assert.equal(packets.length,2);
  const rebuilt=before.slice();for(const p of packets)apply(p,rebuilt);assert.deepEqual(rebuilt,after);
  assert.ok(packets.reduce((n,p)=>n+p.readUInt32LE(16)*p.readUInt32LE(20),0)<20000);
  assert.equal(encodeBands(after,after).length,0);
});

test('home reacts to song and playback changes while other pages ignore media events',()=>{
  const desk=emptyDesk(),view={mode:'clock' as const,page:'board' as const,provider:'Codex' as const,prompt:'',pageIndex:0,notice:''};
  const before=screenKey([],view,'',desk);
  desk.media=mediaState({title:'曲目',artist:'艺术家',playing:false,duration:180,elapsedTime:20});
  assert.notEqual(screenKey([],view,'',desk),before);
  const paused=screenKey([],view,'',desk);desk.media.playing=true;assert.notEqual(screenKey([],view,'',desk),paused);
  const news={...view,mode:'news' as const},key=screenKey([],news,'',desk);desk.media.title='另一首';assert.equal(screenKey([],news,'',desk),key);
});
test('home controls retain transport actions without invoking the real player',()=>{
  const called:string[]=[],desk={media:{control:(s:string)=>called.push(s)},armTimer:()=>{},changed:()=>{}};
  for(const value of ['previous-track','toggle-play-pause','next-track'])
    Desktop.prototype.act.call(desk as unknown as Desktop,{x:0,y:0,w:1,h:1,action:'music',value},{mode:'clock',page:'board',provider:'Codex',prompt:'',pageIndex:0,notice:''});
  assert.deepEqual(called,['previous-track','toggle-play-pause','next-track']);
});

test('device battery changes the home frame and settings offer bounded controls',()=>{
  const desk=emptyDesk(),view={mode:'clock' as const,page:'board' as const,filter:'all' as const,provider:'Codex' as const,prompt:'',pageIndex:0,notice:'',device:{percent:97,light:6,automatic:false,charging:0}};
  assert.notEqual(screenKey([],view,'',desk),screenKey([],{...view,device:{...view.device,percent:96}},'',desk));
  const menu=render([],{...view,settings:true},'',desk);
  assert.deepEqual(menu.hits.filter(h=>h.action==='light').map(h=>h.value),['0','6','12','18','24']);
  assert.ok(menu.hits.some(h=>h.action==='display-clean'));
});
