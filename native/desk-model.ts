// input: clock/news/media state; output: shared mode types and time rules; pos: multifunction desk domain.
export type Mode='clock'|'news'|'agent';
export type NewsItem={id:string;title:string;summary:string;source:string;url:string;originalUrl?:string;publishedAt:number};
export type NewsState={items:NewsItem[];fetchedAt:number;loading:boolean;error:string};
export type MediaState={title:string;artist:string;album:string;app:string;playing:boolean;duration:number;elapsed:number;sampleAt:number;artwork?:string;mime?:string;error:string;connected:boolean};
export type Focus={endAt:number;remainingMs:number;finished:boolean};
export type Desk={now:number;news:NewsState;media:MediaState;focus:Focus;clockStyle:'digital'|'analog'};
export const modes:Mode[]=['clock','news','agent'];
export const modeLabels:Record<Mode,string>={clock:'首页',news:'AI 日报',agent:'Agent'};
export const dateParts=(now:number)=>new Intl.DateTimeFormat('en-CA',{timeZone:'Asia/Taipei',year:'numeric',month:'2-digit',day:'2-digit',hour:'2-digit',minute:'2-digit',hour12:false}).formatToParts(now).reduce((o,p)=>({...o,[p.type]:p.value}),{} as Record<string,string>);
export const dayKey=(now:number)=>{const p=dateParts(now);return `${p.year}-${p.month}-${p.day}`;};
export function position(media:MediaState,now:number){return Math.min(media.duration||Infinity,Math.max(0,media.elapsed+(media.playing?Math.max(0,now-media.sampleAt)/1000:0)));}
export const duration=(seconds:number)=>`${Math.floor(seconds/60)}:${String(Math.floor(seconds%60)).padStart(2,'0')}`;
export function focusRemaining(f:Focus,now:number){return f.endAt?Math.max(0,f.endAt-now):f.remainingMs;}
export const emptyDesk=():Desk=>({now:Date.now(),clockStyle:'digital',focus:{endAt:0,remainingMs:25*60000,finished:false},news:{items:[],fetchedAt:0,loading:false,error:''},media:{title:'',artist:'',album:'',app:'',playing:false,duration:0,elapsed:0,sampleAt:0,error:'',connected:false}});
