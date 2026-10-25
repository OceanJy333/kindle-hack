// input: active tab, time and notices; output: shared header, tab bar and pager; pos: frame every desk screen sits in.
import { Canvas, measure, ellipsize } from './canvas.ts';
import { W, H, space, frame, color, type, tabs, tabLabels } from './tokens.ts';
export type Tab = typeof tabs[number];
export function header(c:Canvas,title:string,time:string,notice='',status=''){
  c.text(title,frame.left,80,type.page.size,color.ink,type.page.weight);
  c.right(time,frame.right,78,30,color.ink,500);
  const left=frame.left+measure(title,type.page.size,700)+40,right=frame.right-measure(time,30,500)-32;
  if(notice){
    // A notice is the only inverted element in the header, so it reads as "new" without sound or flashing.
    const label=ellipsize(notice,right-left-36,24,500),w=measure(label,24,500)+36;
    c.box(right-w,44,w,48,color.ink,'none',0,6);c.text(label,right-w+18,77,24,color.paper,500);c.hit(right-w,30,w,76,'dismiss');
  }else if(status)c.right(ellipsize(status,right-left,24),right,77,24,color.muted);
  c.line(frame.left,space.header-2,frame.right,space.header-2,2,color.ink);
}
export function tabBar(c:Canvas,active:Tab,badges:Partial<Record<Tab,boolean>>={}){
  const y=H-space.tab,w=W/tabs.length;
  c.line(0,y+1,W,y+1,2,color.ink);
  tabs.forEach((tab,i)=>{
    const x=i*w,on=tab===active,label=tabLabels[tab],weight=on?700:400;
    if(on)c.box(x+28,y,w-56,8,color.ink);
    c.center(label,y+76,28,on?color.ink:color.sub,weight,x,w);
    if(badges[tab])c.circle(x+(w+measure(label,28,weight))/2+12,y+46,7,color.ink,color.ink,0);
    c.hit(x,y,w,space.tab,tab==='reader'?'reader':'mode',tab);
  });
}
// Bottom pager inside the content frame; PagePress keys reach the same actions.
export function pager(c:Canvas,page:number,pages:number,extra?:{label:string;action:string;disabled?:boolean}){
  const y=frame.bottom-104,h=76;
  c.button('‹ 上一页',frame.left,y,220,h,'page','-1','outline',page<=0);
  c.button('下一页 ›',frame.right-220-(extra?244:0),y,220,h,'page','1','outline',page>=pages-1);
  const mid=extra?(frame.left+220+frame.right-464)/2:W/2;
  c.center(`${page+1} / ${Math.max(1,pages)}`,y+48,26,color.sub,400,mid-100,200);
  if(extra)c.button(extra.label,frame.right-220,y,220,h,extra.action,undefined,'outline',extra.disabled);
  return y;
}
export const pagerTop=frame.bottom-104-24;
