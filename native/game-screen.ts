// input: canonical game catalog; output: native game launcher rows; pos: home game surface.
import { readFileSync } from 'node:fs';
import { Canvas } from './canvas.ts';
import { frame, color } from './tokens.ts';
export const games=JSON.parse(readFileSync(new URL('../games/catalog.json',import.meta.url),'utf8')) as {id:string;code:number;title:string;year:number;engine:string;action:string}[];
export function gameScreen(c:Canvas){
 c.text('等 Agent 的时候，玩一局',frame.left,206,28,color.sub);
 games.forEach((game,i)=>{
  const y=292+i*328;
  c.text(game.title,frame.left,y,44,color.ink,700);
  c.text(String(game.year),frame.left,y+54,26,color.sub);
  c.text('任务有结果时，会在这里提醒你',frame.left,y+108,26,color.sub);
  c.button(game.action,frame.right-240,y+56,240,84,'game',game.id,'primary');
  c.line(frame.left,y+192,frame.right,y+192,1,color.line);
 });
 c.text('十字方向键 + A/B · Voyage 压感键可用',frame.left,1070,28,color.sub);
 c.text('点击游戏右上角，返回时钟和 Agent 桌面。',frame.left,1124,26,color.sub);
 c.button('返回首页',frame.left,1200,240,80,'games-back');
}
