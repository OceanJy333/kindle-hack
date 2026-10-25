// input: macOS now-playing events and transport taps; output: current player state and play/skip commands; pos: system media adapter.
import { spawn,execFile } from 'node:child_process';
import type { ChildProcess } from 'node:child_process';
import type { MediaState } from './desk-model.ts';
const apps:Record<string,string>={'com.apple.Music':'Apple Music','com.spotify.client':'Spotify','com.netease.163music':'网易云音乐','com.tencent.QQMusicMac':'QQ 音乐'};
export const commands=['toggle-play-pause','next-track','previous-track'] as const;
export type MediaCommand=typeof commands[number];
export function mediaState(p:any):MediaState {
  if(!p)return {title:'',artist:'',album:'',app:'',playing:false,duration:0,elapsed:0,sampleAt:Date.now(),error:'',connected:true};
  return {title:String(p.title||''),artist:String(p.artist||''),album:String(p.album||''),app:apps[p.bundleIdentifier]||p.applicationName||p.bundleIdentifier||'当前播放器',
    playing:Boolean(p.playing),duration:Number(p.duration)||0,elapsed:Number(p.elapsedTime)||0,sampleAt:Number.isFinite(Date.parse(p.timestamp))?Date.parse(p.timestamp):Date.now(),
    artwork:typeof p.artworkData==='string'?p.artworkData:undefined,mime:p.artworkMimeType,error:'',connected:true};
}
export class Media {
  state:MediaState=mediaState(null);changed:()=>void;child?:ChildProcess;closed=false;
  constructor(changed:()=>void){this.changed=changed;this.start();}
  start(){
    if(this.closed)return;
    const child=spawn('media-control',['stream','--no-diff','--debounce=300'],{stdio:['ignore','pipe','pipe']});this.child=child;let buffer='';
    child.stdout.on('data',b=>{buffer+=b.toString();if(buffer.length>12*1024*1024){buffer='';return;}let end:number;
      while((end=buffer.indexOf('\n'))>=0){const line=buffer.slice(0,end);buffer=buffer.slice(end+1);try{const e=JSON.parse(line);this.state=mediaState(e.payload===undefined?e:e.payload);this.changed();}catch{}}});
    child.stderr.on('data',()=>{});
    child.on('error',e=>{this.state.error=e.message;this.state.connected=false;this.changed();});
    child.on('exit',()=>{if(this.closed)return;this.state.connected=false;this.state.error='播放器连接已断开，正在重连';this.changed();setTimeout(()=>this.start(),10000).unref();});
    this.refresh();
  }
  refresh(){
    execFile('media-control',['get'],{maxBuffer:12*1024*1024,timeout:5000},(error,stdout)=>{if(!error){try{this.state=mediaState(JSON.parse(stdout));this.changed();}catch{}}});
  }
  // Transport commands go to whichever app owns Now Playing; the stream then reports the real result.
  control(command:MediaCommand){
    if(!commands.includes(command))return;
    if(command==='toggle-play-pause'&&this.state.title){const now=Date.now(),s=this.state;
      this.state={...s,elapsed:s.playing?s.elapsed+(now-s.sampleAt)/1000:s.elapsed,sampleAt:now,playing:!s.playing};this.changed();}
    execFile('media-control',[command],{timeout:5000},error=>{if(error){this.state={...this.state,error:'播放器没有响应'};this.changed();}setTimeout(()=>this.refresh(),800).unref();});
  }
  close(){this.closed=true;this.child?.kill('SIGTERM');}
}
