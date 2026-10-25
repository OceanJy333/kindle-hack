// input: tasks, desk state and view; output: native screen pixels, touch regions and a change key; pos: screen composition entry.
import type { Task } from './model.ts';
import { Canvas } from './canvas.ts';
import { W, H } from './tokens.ts';
import { header, tabBar } from './chrome.ts';
import { clockScreen } from './mode-screens.ts';
import { musicWidget } from './music-widget.ts';
import { newsList, newsArticle, newsStatus } from './news-screen.ts';
import { agentBoard, agentDetail, agentCompose, agentTasks, ROWS } from './agent-screen.ts';
import type { AgentView } from './agent-screen.ts';
import { emptyDesk, dateParts, focusRemaining, position } from './desk-model.ts';
import type { Desk, Mode } from './desk-model.ts';
import { settingsScreen,batteryLabel } from './settings-screen.ts';
import type { DeviceState } from './settings-screen.ts';
import { gameScreen } from './game-screen.ts';
export type { Hit } from './canvas.ts';
export type View = AgentView & { notice: string; settings?: boolean; device?: DeviceState; mode?: Mode; games?: boolean; newsPage?: number; article?: string; articlePage?: number; agentBadge?: boolean };
const titles:Record<Mode,string>={clock:'等一会儿',news:'AI 日报',agent:'Agent'};
function agentKey(tasks:Task[],v:View,current:string){
  const t=(x:Task)=>[x.id,x.title,x.status,x.progress,x.updatedAt,x.cwd];
  if(v.page==='compose')return {v:[v.page,v.replyTo,v.provider,v.prompt,v.shifted,v.cwd],target:tasks.filter(x=>x.id===v.replyTo).map(x=>x.title)};
  if(v.page==='detail')return {v:[v.page,v.selected,v.detailPage],task:tasks.filter(x=>x.id===v.selected).map(t)};
  const list=agentTasks(tasks,v.filter,current);
  return {v:[v.page,v.filter,v.pageIndex],n:[tasks.length,...['Codex','Claude'].map(p=>tasks.filter(x=>x.provider===p).length)],shown:list.slice(v.pageIndex*ROWS,v.pageIndex*ROWS+ROWS).map(t)};
}
// Anything visible must be in this key; anything invisible must not, so idle screens emit no frames.
export function screenKey(tasks:Task[],view:View,current:string,desk:Desk=emptyDesk()){
  const mode=view.mode||'agent',base={mode,notice:view.notice,badge:view.agentBadge,minute:Math.floor(desk.now/60000)};
  if(view.settings)return JSON.stringify({base,settings:true,device:view.device});
  if(view.games)return JSON.stringify({base,games:true});
  const m=desk.media,media=[m.title,m.artist,m.album,m.app,m.playing,m.duration,m.artwork,m.mime,m.error,Math.floor(position(m,desk.now)/60)];
  if(mode==='clock')return JSON.stringify({base,device:view.device,media,style:desk.clockStyle,focus:[Math.ceil(focusRemaining(desk.focus,desk.now)/60000),desk.focus.endAt,desk.focus.finished],running:tasks.filter(t=>t.status==='running').length,news:desk.news.items.length});
  if(mode==='news')return JSON.stringify({base,page:view.newsPage,article:view.article,articlePage:view.articlePage,news:desk.news});
  const counts=['running','done'].map(s=>tasks.filter(t=>t.status===s).length);
  return JSON.stringify({base,counts,agent:agentKey(tasks,view,current)});
}
export function render(tasks:Task[],view:View,current:string,desk:Desk=emptyDesk()){
  const c=new Canvas(),mode=view.mode||'agent',time=dateParts(desk.now);
  c.box(0,0,W,H);
  const running=tasks.filter(t=>t.status==='running').length;
  const status=view.settings?'':mode==='news'?newsStatus(desk):mode==='agent'?`${running} 运行中 · ${tasks.filter(t=>t.status==='done').length} 已完成`:'轻点时间切换表盘';
  header(c,view.games?'游戏':titles[mode],view.games||mode!=='clock'?`${time.hour}:${time.minute}`:batteryLabel(view.device),view.notice,view.games?'':status);
  if(view.settings){settingsScreen(c,view.device);return c.finish();}
  if(view.games){gameScreen(c);return c.finish();}
  if(mode==='clock'){clockScreen(c,desk,tasks);musicWidget(c,desk);}
  else if(mode==='news'){if(view.article)newsArticle(c,desk,view.article,view.articlePage||0);else newsList(c,desk,view.newsPage||0);}
  else if(view.page==='detail')agentDetail(c,tasks,view,desk.now);
  else if(view.page==='compose')agentCompose(c,tasks,view);
  else agentBoard(c,tasks,view,current,desk.now);
  tabBar(c,mode,{agent:Boolean(view.agentBadge)&&mode!=='agent'});
  return c.finish();
}
