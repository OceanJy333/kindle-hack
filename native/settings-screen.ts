// input: last device-confirmed settings; output: battery and frontlight menu; pos: native home settings.
import { Canvas } from './canvas.ts';
import { frame,color } from './tokens.ts';
export type DeviceState={percent:number;light:number;automatic:boolean;charging:number};
export function batteryLabel(d?:DeviceState){return d&&d.percent>=0?`电量 ${d.percent}%${d.charging===1?' · 充电中':d.charging===2?' · 已充满':''}`:'电量读取中';}
export function settingsScreen(c:Canvas,d?:DeviceState){
  c.text('屏幕与电量',frame.left,204,36,color.ink,700);
  c.button('返回首页',frame.right-220,152,220,76,'settings-close');
  c.text(batteryLabel(d),frame.left,300,32);
  c.text('前光亮度',frame.left,414,30,color.ink,500);
  c.text(d&&d.light>=0?`${d.light} / 24`:'读取中',frame.left,506,64,color.ink,500);
  c.text(d?.automatic?'自动亮度已开启':'手动亮度',frame.left,564,26,color.sub);
  c.button('−',frame.left,610,180,86,'light-step','-1');
  c.button('+',frame.right-180,610,180,86,'light-step','1');
  for(let i=0;i<5;i++){const level=i*6;c.button(level===0?'关闭':String(level),frame.left+i*196,746,176,82,'light',String(level),d?.light===level&&!d.automatic?'primary':'outline');}
  c.button(d?.automatic?'关闭自动亮度':'开启自动亮度',frame.left,890,440,86,'light-auto',d?.automatic?'0':'1');
  c.button('清除残影',frame.right-440,890,440,86,'display-clean');
  c.text('清除残影会完整刷新一次，屏幕会短暂闪烁。',frame.left,1040,24,color.sub);
  c.text('亮度和电量以设备回传结果为准。',frame.left,1100,24,color.muted);
}
