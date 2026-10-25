// input: observed task transitions; output: persistent, deduplicated handoff queue; pos: notification authority.
import {readFileSync,writeFileSync,renameSync} from 'node:fs';
import type {Task} from './model.ts';
export type Notice={key:string;taskId:string;provider:string;title:string;status:string;body:string;at:number};
export class Notifications {
  queue:Notice[]=[]; seen:Record<string,string>={}; previous=new Map<string,string>();
  file?:string;
  constructor(file?:string){this.file=file;if(file)try{const s=JSON.parse(readFileSync(file,'utf8'));this.queue=s.queue||[];this.seen=s.seen||{};}catch{}}
  update(tasks:Task[]){
    let changed=false;
    for(const t of tasks){
      const before=this.previous.get(t.id),cycle=t.turnId||String(t.startedAt||0),key=`${cycle}:${t.status}`;
      this.previous.set(t.id,t.status);
      if(before===undefined||before===t.status||!['done','waiting','error'].includes(t.status)||this.seen[t.id]===key)continue;
      this.seen[t.id]=key;
      this.queue=this.queue.filter(n=>n.taskId!==t.id);
      this.queue.push({key:`${t.id}:${key}`,taskId:t.id,provider:t.provider,title:t.title,status:t.status,body:t.progress,at:t.updatedAt});
      changed=true;
    }
    if(changed)this.save();return changed;
  }
  dismiss(){this.queue.shift();this.save();}
  save(){if(this.file){writeFileSync(this.file+'.tmp',JSON.stringify({queue:this.queue,seen:this.seen}),{mode:0o600});renameSync(this.file+'.tmp',this.file);}}
}
