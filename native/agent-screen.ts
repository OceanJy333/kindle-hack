// input: Codex / Claude conversations and agent view; output: list, reading view and reply composer; pos: Agent 工作台 layout.
import { Canvas, wrap, ellipsize, measure } from './canvas.ts';
import { pager, pagerTop } from './chrome.ts';
import { frame, color, W } from './tokens.ts';
import { dateParts, dayKey } from './desk-model.ts';
import { labels } from './model.ts';
import type { Task, Provider, Status } from './model.ts';
export type AgentFilter='all'|Provider;
export type AgentView={page:'board'|'detail'|'compose';filter?:AgentFilter;selected?:string;replyTo?:string;provider:Provider;prompt:string;pageIndex:number;detailPage?:number;shifted?:boolean;cwd?:string};
const pinned=new Set((process.env.BOARD_PIN_IDS||'').split(','));
export const providerName=(p:Provider)=>p==='Claude'?'Claude Code':'Codex';
export const project=(cwd:string)=>cwd.split('/').filter(Boolean).at(-1)||'电脑';
export function agentTasks(tasks:Task[],filter:AgentFilter='all',current=''){
  return tasks.filter(t=>filter==='all'||t.provider===filter).sort((a,b)=>
    Number(b.id===current)-Number(a.id===current)||Number(b.status==='running')-Number(a.status==='running')||
    Number(pinned.has(b.id))-Number(pinned.has(a.id))||b.updatedAt-a.updatedAt);
}
export const ROWS=5,ROW_H=190,LIST_TOP=244;
const when=(at:number,now:number)=>{const p=dateParts(at);return dayKey(at)===dayKey(now)?`${p.hour}:${p.minute}`:`${Number(p.month)}/${Number(p.day)}`;};
function mark(c:Canvas,s:Status,x:number,y:number){
  if(s==='running')c.circle(x,y,11,color.ink,color.ink,0);
  else if(s==='done')c.path(`M ${x-11} ${y} L ${x-3} ${y+9} L ${x+12} ${y-9}`,color.ink,4);
  else if(s==='waiting'){c.box(x-12,y-12,24,24,color.ink,'none',0,4);c.text('!',x-4,y+9,22,color.paper,700);}
  else if(s==='error'){c.path(`M ${x-9} ${y-9} L ${x+9} ${y+9} M ${x+9} ${y-9} L ${x-9} ${y+9}`,color.ink,4);}
  else c.circle(x,y,10,'none',color.muted,3);
}
export function agentBoard(c:Canvas,tasks:Task[],v:AgentView,current:string,now:number){
  const filter=v.filter||'all';
  (['all','Codex','Claude'] as AgentFilter[]).forEach((f,i)=>{
    const n=f==='all'?tasks.length:tasks.filter(t=>t.provider===f).length,label=`${f==='all'?'全部':f} ${n}`,x=frame.left+i*176,on=f===filter;
    c.text(label,x,192,28,on?color.ink:color.sub,on?700:400);if(on)c.box(x,206,measure(label,28,700),5,color.ink);
    c.hit(x-8,140,168,84,'agent-filter',f);
  });
  c.button('＋ 新任务',frame.right-210,146,210,68,'compose',undefined,'primary');
  const list=agentTasks(tasks,filter,current),pages=Math.max(1,Math.ceil(list.length/ROWS));c.pages=pages;
  const shown=list.slice(v.pageIndex*ROWS,v.pageIndex*ROWS+ROWS);
  if(!shown.length)c.center('暂无对话',560,30,color.muted);
  shown.forEach((t,i)=>{
    const y=LIST_TOP+i*ROW_H,x=frame.left+44,width=frame.width-44;
    c.line(frame.left,y,frame.right,y,1,color.line);
    mark(c,t.status,frame.left+12,y+36);
    const time=t.id===current?'本对话':when(t.updatedAt,now);
    c.right(time,frame.right,y+46,24,color.muted);
    c.text(ellipsize(t.title,width-130,32,500),x,y+48,32,color.ink,500);
    c.text(`${providerName(t.provider)} · ${project(t.cwd)} · ${labels[t.status]}`,x,y+86,23,color.muted);
    c.lines(t.progress||'还没有回复',x,y+128,width,26,2,t.progress?color.text:color.muted,400,38);
    c.hit(frame.left,y,frame.width,ROW_H,'detail',t.id);
  });
  pager(c,v.pageIndex,pages);
}
function pagerBar(c:Canvas,page:number,pages:number,action?:{label:string;action:string;value?:string}){
  const y=frame.bottom-104;
  c.button('‹ 上一页',frame.left,y,200,76,'page','-1','outline',page<=0);
  c.button('下一页 ›',frame.left+224,y,200,76,'page','1','outline',page>=pages-1);
  c.text(`${page+1} / ${pages}`,frame.left+452,y+48,26,color.sub);
  if(action)c.button(action.label,frame.right-260,y,260,76,action.action,action.value,'primary');
}
// Page 1 carries the title block; later pages use the whole frame for the reply.
function detailTop(t:Task){return 270+Math.min(2,wrap(t.title,frame.width,36,700).length)*52+150;}
export function replyPages(t:Task){
  const lines=wrap(t.progress||'还没有回复。',frame.width,30),first=Math.floor((pagerTop-detailTop(t))/50),rest=Math.floor((pagerTop-250)/50);
  const pages=[lines.slice(0,first)];for(let i=first;i<lines.length;i+=rest)pages.push(lines.slice(i,i+rest));
  return pages;
}
export function agentDetail(c:Canvas,tasks:Task[],v:AgentView,now:number){
  c.button('‹ 全部对话',frame.left,148,210,64,'back',undefined,'ghost');
  const t=tasks.find(t=>t.id===v.selected);if(!t){c.center('对话已不在列表中',560,30,color.muted);return;}
  c.right(`${providerName(t.provider)} · ${labels[t.status]}`,frame.right,190,24,color.muted);
  const pages=replyPages(t),p=Math.min(v.detailPage||0,pages.length-1);c.pages=pages.length;
  let y=250;
  if(p===0){
    const n=c.lines(t.title,frame.left,282,frame.width,36,2,color.ink,700,52),meta=282+(n-1)*52+50;
    c.text(`${project(t.cwd)} · ${dayKey(t.updatedAt)} ${when(t.updatedAt,t.updatedAt)}`,frame.left,meta,24,color.muted);
    c.line(frame.left,meta+28,frame.right,meta+28,2,color.ink);
    c.text(t.status==='running'?'最新进展 · 自动更新':'最新回复',frame.left,meta+78,24,color.sub,700);
    y=detailTop(t);
  }
  pages[p].forEach((l,i)=>c.text(l,frame.left,y+i*50,30,color.text));
  pagerBar(c,p,pages.length,{label:`回复 ${t.provider}`,action:'reply',value:t.id});
}
const quick={reply:[['继续','继续'],['按你的建议做','可以，按你的建议做。'],['先停，总结现状','先停下，用三句话总结现状和待决问题。'],['说得具体点','把刚才的结论说得更具体，列出依据。']],
  create:[['验证连接','verify'],['总结项目','summary'],['检查改动','changes'],['今日待办','todo']]};
export function presetPrompt(key:string,provider:Provider){
  return ({verify:`只回复“Kindle 已接入 ${provider}”，不要调用工具。`,summary:'阅读当前项目，用三句话总结它的用途和最近进展。',
    changes:'查看当前工作区改动，指出最需要关注的问题。',todo:'根据最近的改动和对话，列出今天最该推进的三件事。'} as Record<string,string>)[key]||key;
}
export function agentCompose(c:Canvas,tasks:Task[],v:AgentView){
  const target=v.replyTo?tasks.find(t=>t.id===v.replyTo):undefined;
  c.button('‹ 取消',frame.left,148,150,64,'compose-cancel',undefined,'ghost');
  if(target){c.text(ellipsize(`回复 ${providerName(target.provider)} · ${target.title}`,frame.width-190,28,500),frame.left+180,192,28,color.ink,500);}
  else{
    (['Codex','Claude'] as Provider[]).forEach((p,i)=>c.button(providerName(p),frame.left+180+i*214,148,200,64,'provider',p,v.provider===p?'primary':'outline'));
    const label=ellipsize(`工作区 ${project(v.cwd||'')} ›`,frame.right-(frame.left+620),26);
    c.right(label,frame.right,190,26,color.sub);c.hit(frame.left+620,140,frame.right-frame.left-620,84,'workspace');
  }
  c.box(frame.left,236,frame.width,164,color.paper,color.ink,2,8);
  c.lines(v.prompt||(target?'选一句快捷回复，或用键盘输入':'选一项常用任务，或用键盘输入'),frame.left+24,286,frame.width-48,30,3,v.prompt?color.ink:color.muted,400,46);
  const chips=target?quick.reply:quick.create,cw=(frame.width-16)/2;
  chips.forEach(([label,value],i)=>c.button(label,frame.left+(i%2)*(cw+16),424+Math.floor(i/2)*76,cw,64,'preset',value,'outline',false,26));
  const rows=['1234567890','qwertyuiop','asdfghjkl-','zxcvbnm.,?'],kw=(frame.width-9*10)/10;
  rows.forEach((row,r)=>[...row].forEach((key,j)=>{const k=v.shifted?key.toUpperCase():key;c.button(k,frame.left+j*(kw+10),596+r*100,kw,88,'key',k,'outline',false,30);}));
  c.button(v.shifted?'小写':'大写',frame.left,996,200,84,'shift');c.button('空格',frame.left+216,996,frame.width-432,84,'key',' ');c.button('删除',frame.right-200,996,200,84,'delete');
  c.button(target?`发送回复`:`发送给 ${providerName(v.provider)}`,frame.left,1104,frame.width,92,'send',undefined,'primary',!v.prompt.trim());
  c.center(target?'回复会接着这个对话继续，结果自动回到列表':'直接在电脑上运行，结果自动回到列表',1244,23,color.muted,400,0,W);
}
