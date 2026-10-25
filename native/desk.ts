// input: touch actions/time/source events; output: silent desk state; pos: mode interaction controller.
import { readFileSync,writeFileSync,renameSync } from 'node:fs';
import { join } from 'node:path';
import { News } from './news.ts';
import { Media } from './media.ts';
import type { MediaCommand } from './media.ts';
import { emptyDesk,focusRemaining,modes } from './desk-model.ts';
import type { Desk,Focus,Mode } from './desk-model.ts';
import type { View,Hit } from './screen.ts';
export function toggleFocus(focus:Focus,now:number):Focus {
  if(focus.endAt)return {endAt:0,remainingMs:focusRemaining(focus,now),finished:false};
  const remainingMs=focus.remainingMs||25*60000;
  return {endAt:now+remainingMs,remainingMs,finished:false};
}
export class Desktop {
  news:News;media:Media;focus:Focus=emptyDesk().focus;clockStyle:Desk['clockStyle']='digital';
  settings:string;changed:()=>void;notify:(message:string)=>void;timer:NodeJS.Timeout;mode:Mode='clock';
  constructor(root:string,changed:()=>void,notify:(message:string)=>void){
    this.settings=join(root,'desk-settings.json');this.changed=changed;this.notify=notify;
    try{const s=JSON.parse(readFileSync(this.settings,'utf8'));if(['digital','analog'].includes(s.clockStyle))this.clockStyle=s.clockStyle;
      if(s.focus&&Number.isFinite(s.focus.endAt)&&Number.isFinite(s.focus.remainingMs))this.focus=s.focus;}catch{}
    let newsAt=0;
    this.news=new News(root,()=>{const n=this.news.state;if(!n.loading&&n.fetchedAt>newsAt){newsAt=n.fetchedAt;this.notify(`AI 日报已更新 · ${n.items.length} 条动态`);}this.changed();});
    newsAt=this.news.state.fetchedAt;this.media=new Media(changed);
    this.armTimer();
  }
  snapshot():Desk{return {now:Date.now(),clockStyle:this.clockStyle,focus:this.focus,news:this.news.state,media:this.media.state};}
  save(){writeFileSync(this.settings+'.tmp',JSON.stringify({clockStyle:this.clockStyle,focus:this.focus}));renameSync(this.settings+'.tmp',this.settings);}
  armTimer(){
    clearTimeout(this.timer);
    // Every visible clock, focus and music value changes at most once a minute, so one minute boundary serves all modes.
    const now=Date.now(), step=60000;
    const boundary=(Math.floor(now/step)+1)*step;
    const deadline=this.focus.endAt>now?Math.min(boundary,this.focus.endAt):boundary;
    this.timer=setTimeout(()=>{this.tick();this.armTimer();},Math.max(1,deadline-now));this.timer.unref();
  }
  tick(now=Date.now()){
    if(this.focus.endAt&&now>=this.focus.endAt){this.focus={endAt:0,remainingMs:0,finished:true};this.save();this.notify('专注已完成 · 休息一下');}
    this.changed();
  }
  act(hit:Hit,view:View):boolean {
    switch(hit.action){
      case 'mode':if(modes.includes(hit.value as Mode)){view.mode=hit.value as Mode;this.mode=view.mode;view.notice='';}break;
      case 'clock-style':this.clockStyle=this.clockStyle==='digital'?'analog':'digital';this.save();break;
      case 'focus-toggle':this.focus=toggleFocus(this.focus,Date.now());this.save();break;
      case 'focus-reset':this.focus=emptyDesk().focus;this.save();view.notice='';break;
      case 'news-article':view.article=hit.value;view.articlePage=0;break;
      case 'news-back':view.article=undefined;break;
      case 'news-refresh':view.newsPage=0;void this.news.refresh();break;
      case 'music-refresh':this.media.refresh();break;
      case 'music':this.media.control(hit.value as MediaCommand);break;
      default:return false;
    }
    this.armTimer();this.changed();return true;
  }
  close(){clearInterval(this.timer);this.media.close();}
}
