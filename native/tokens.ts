// input: device profile; output: design tokens and shared geometry; pos: single style source for every desk screen.
import { readFileSync } from 'node:fs';
const profile=JSON.parse(readFileSync(new URL('../device.json',import.meta.url),'utf8'));
export const W:number=profile.width, H:number=profile.height;
// Voyage is 300 ppi: 56 px ≈ 4.7 mm side margin; tab bar 124 px ≈ 10.5 mm touch height.
// TAB_H must match TAB_BAR_HEIGHT in client.c and the reader tab bar in LoginHarness.qml.
export const space={margin:56,gap:24,row:28,header:124,tab:124};
export const frame={top:space.header,bottom:H-space.tab,left:space.margin,right:W-space.margin,width:W-space.margin*2};
export const color={ink:'#000',text:'#222',sub:'#555',muted:'#777',line:'#bbb',soft:'#e8e8e8',paper:'#fff'};
export const type={
  page:{size:44,weight:700 as const},
  title:{size:34,weight:500 as const,line:50},
  body:{size:30,weight:400 as const,line:50},
  small:{size:26,weight:400 as const,line:40},
  meta:{size:23,weight:400 as const,line:34},
  label:{size:28,weight:500 as const},
};
export const tabs=['clock','news','agent','reader'] as const;
export const tabLabels:Record<typeof tabs[number],string>={clock:'首页',news:'AI 日报',agent:'Agent',reader:'微信读书'};
