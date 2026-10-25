// input: AIHOT digest and view position; output: news index and article pages; pos: AI 日报 layout.
import { Canvas, wrap, ellipsize } from './canvas.ts';
import { pager, pagerTop } from './chrome.ts';
import { frame, color } from './tokens.ts';
import { dateParts, dayKey } from './desk-model.ts';
import type { Desk, NewsItem } from './desk-model.ts';
const listTop=210,timeColumn=104,titleWidth=frame.width-timeColumn;
type Row={item:NewsItem;title:string[];summary:string;height:number};
function rows(items:NewsItem[]):Row[]{
  return items.map(item=>{
    const title=wrap(item.title,titleWidth,32,500).slice(0,2);
    if(wrap(item.title,titleWidth,32,500).length>2)title[1]=ellipsize(title[1]+'…',titleWidth,32,500);
    // One-line headlines carry a summary line, so rows stay close to the same height.
    const summary=title.length===1?ellipsize(item.summary.replace(/\s+/g,' '),titleWidth,24):'';
    return {item,title,summary,height:40+title.length*46+(summary?38:0)};
  });
}
export function newsPages(items:NewsItem[]){
  const pages:Row[][]=[[]];let used=0;
  for(const row of rows(items)){if(used+row.height>pagerTop-listTop&&pages.at(-1)!.length){pages.push([]);used=0;}pages.at(-1)!.push(row);used+=row.height;}
  return pages;
}
const stamp=(at:number,now:number)=>{const p=dateParts(at);return dayKey(at)===dayKey(now)?`${p.hour}:${p.minute}`:`${Number(p.month)}/${Number(p.day)}`;};
export function newsStatus(d:Desk){
  const n=d.news;if(n.loading)return '正在更新…';
  if(!n.fetchedAt)return '等待首次更新';
  const p=dateParts(n.fetchedAt);return `${n.items.length} 条 · ${p.hour}:${p.minute} 更新`;
}
export function newsList(c:Canvas,d:Desk,page:number){
  const items=d.news.items,pages=newsPages(items);c.pages=pages.length;
  c.text('卡兹克 AIHOT 精选 · 每天 08:00 更新',frame.left,176,24,color.muted);
  if(!items.length){c.center(d.news.loading?'正在读取 AIHOT':'暂无新闻',560,34,color.ink,500);c.center(d.news.error||'连上网络后会自动更新',620,24,color.muted);}
  let y=listTop;
  for(const row of pages[Math.min(page,pages.length-1)]||[]){
    c.line(frame.left,y,frame.right,y,1,color.line);
    c.text(stamp(row.item.publishedAt,d.now),frame.left,y+52,24,color.muted);
    row.title.forEach((l,i)=>c.text(l,frame.left+timeColumn,y+54+i*46,32,color.ink,500));
    if(row.summary)c.text(row.summary,frame.left+timeColumn,y+54+46,24,color.sub);
    c.hit(frame.left,y,frame.width,row.height,'news-article',row.item.id);y+=row.height;
  }
  if(d.news.error&&items.length)c.text(ellipsize(d.news.error,frame.width,22),frame.left,pagerTop-6,22,color.muted);
  pager(c,page,pages.length,{label:d.news.loading?'更新中':'刷新',action:'news-refresh',disabled:d.news.loading});
}
const bodyTop=(titleLines:number)=>310+titleLines*58+56;
export function articlePages(item:NewsItem){
  const title=wrap(item.title,frame.width,40,700).slice(0,4),body=wrap(item.summary||'原始订阅未提供摘要。',frame.width,30);
  const first=Math.floor((pagerTop-bodyTop(title.length)-60)/52),rest=Math.floor((pagerTop-260)/52);
  const pages=[body.slice(0,first)];for(let i=first;i<body.length;i+=rest)pages.push(body.slice(i,i+rest));
  return {title,pages};
}
export function newsArticle(c:Canvas,d:Desk,id:string,page:number){
  const items=d.news.items,index=items.findIndex(i=>i.id===id),item=items[index];
  c.button('‹ 返回列表',frame.left,148,200,64,'news-back',undefined,'ghost');
  if(!item)return;
  c.right(`第 ${index+1} / ${items.length} 条`,frame.right,190,24,color.muted);
  const {title,pages}=articlePages(item),p=Math.min(page,pages.length-1);c.pages=pages.length;
  let y=260;
  if(p===0){
    const t=dateParts(item.publishedAt);c.text(`${item.source} · ${dayKey(item.publishedAt)} ${t.hour}:${t.minute}`,frame.left,272,24,color.muted);
    title.forEach((l,i)=>c.text(l,frame.left,330+i*58,40,color.ink,700));
    y=bodyTop(title.length);c.line(frame.left,y-50,frame.left+96,y-50,4,color.ink);
  }
  pages[p].forEach((l,i)=>c.text(l,frame.left,y+i*52,30,color.text));
  if(p===pages.length-1&&item.originalUrl)c.text(`原文 · ${new URL(item.originalUrl).hostname}`,frame.left,pagerTop-12,24,color.muted);
  // At a page edge, the same control moves to the neighbouring story.
  const y0=frame.bottom-104;
  c.button(p>0?'‹ 上一页':'‹ 上一条',frame.left,y0,220,76,'page','-1','outline',p===0&&index<=0);
  c.center(pages.length>1?`${p+1} / ${pages.length} 页`:'',y0+48,26,color.sub);
  c.button(p<pages.length-1?'下一页 ›':'下一条 ›',frame.right-220,y0,220,76,'page','1','outline',p===pages.length-1&&index>=items.length-1);
}
