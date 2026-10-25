// input: AIHOT selected RSS; output: cached Chinese news with source links; pos: sole news provider.
import { XMLParser } from 'fast-xml-parser';
import { readFileSync,writeFileSync,renameSync,mkdirSync } from 'node:fs';
import { join } from 'node:path';
import { dayKey,dateParts } from './desk-model.ts';
import type { NewsItem,NewsState } from './desk-model.ts';
export const NEWS_FEED='https://aihot.news/feed.xml';
const entities:Record<string,string>={amp:'&',lt:'<',gt:'>',quot:'"',apos:"'",nbsp:' '};
function decode(s:string){return s.replace(/&(#x[\da-f]+|#\d+|amp|lt|gt|quot|apos|nbsp);/gi,(all,key)=>{
  if(key[0]!=='#')return entities[key.toLowerCase()];
  const n=key[1].toLowerCase()==='x'?parseInt(key.slice(2),16):Number(key.slice(1));
  return n>0&&n<=0x10ffff?String.fromCodePoint(n):all;
});}
const plain=(s:unknown)=>decode(String(s||'').replace(/<(script|style)\b[^>]*>[\s\S]*?<\/\1>/gi,'').replace(/<[^>]*>/g,' ')).replace(/\s+/g,' ').trim();
function httpsUrl(s:unknown){try{const u=new URL(decode(String(s||'')));return u.protocol==='https:'?u.href:'';}catch{return '';}}
export function parseFeed(xml:string,now=Date.now()):NewsItem[]{
  const root=new XMLParser({processEntities:true,ignoreAttributes:false}).parse(xml),raw=root.rss?.channel?.item||[];
  const items:NewsItem[]=(Array.isArray(raw)?raw:[raw]).map((i:any)=>{
    const html=String(i.description||''),summary=html.split(/<p\b[^>]*>\s*(?:🔗|via\s+AIHOT\b)/iu)[0];
    const original=html.match(/<a\b[^>]*href=["']([^"']+)["'][^>]*>\s*阅读原文\s*<\/a>/iu)?.[1];
    return {id:String(i.guid?.['#text']||i.guid||i.link),title:plain(i.title),summary:plain(summary).slice(0,1200),source:'AIHOT',url:httpsUrl(i.link),originalUrl:httpsUrl(original)||undefined,publishedAt:Date.parse(i.pubDate)};
  }).filter((i:NewsItem)=>i.title&&i.url.startsWith('https://aihot.news/items/')&&Number.isFinite(i.publishedAt)&&i.publishedAt<=now+60000);
  return [...new Map(items.map(i=>[i.id,i])).values()].sort((a,b)=>b.publishedAt-a.publishedAt).slice(0,50);
}
export class News {
  state:NewsState={items:[],fetchedAt:0,loading:false,error:''};etag='';attemptAt=0;
  root:string;changed:()=>void;
  constructor(root:string,changed:()=>void){this.root=root;this.changed=changed;mkdirSync(root,{recursive:true});
    try{const cached=JSON.parse(readFileSync(join(root,'news.json'),'utf8'));
      // A source change invalidates the old digest and its conditional-request token together.
      if(cached.feed===NEWS_FEED&&Array.isArray(cached.state?.items)){this.state={...cached.state,loading:false};this.etag=cached.etag||'';}
    }catch{}
    setInterval(()=>this.tick(),60000).unref();setTimeout(()=>this.tick(),1000).unref();}
  tick(now=Date.now()){
    if((!this.state.fetchedAt||(dayKey(now)!==dayKey(this.state.fetchedAt)&&Number(dateParts(now).hour)>=8))&&now-this.attemptAt>3600000)void this.refresh();
  }
  async refresh(){
    if(this.state.loading)return;this.attemptAt=Date.now();this.state.loading=true;this.state.error='';this.changed();
    try{
      const headers:Record<string,string>={Accept:'application/rss+xml'};
      if(this.etag&&this.state.items.length)headers['If-None-Match']=this.etag;
      const res=await fetch(NEWS_FEED,{headers,signal:AbortSignal.timeout(20000)});
      if(res.status!==304||!this.state.items.length){
        if(!res.ok)throw new Error(`AIHOT 暂时无法访问（${res.status}）`);
        const xml=await res.text();if(xml.length>3*1024*1024)throw new Error('RSS 过大');
        const items=parseFeed(xml);if(!items.length)throw new Error('AIHOT 暂无可显示的条目');
        writeFileSync(join(this.root,'news-source-AIHOT.xml'),xml);
        this.state.items=items;this.etag=res.headers.get('etag')||'';
      }
      this.state.fetchedAt=Date.now();
      const path=join(this.root,'news.json');writeFileSync(path+'.tmp',JSON.stringify({feed:NEWS_FEED,etag:this.etag,state:{...this.state,loading:false}},null,2));renameSync(path+'.tmp',path);
    }catch(e){this.state.error=(e as Error).message;}
    finally{this.state.loading=false;this.changed();}
  }
}
