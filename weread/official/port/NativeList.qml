// input: cached task or news records; output: native list and paged text; pos: on-device browsing.
import QtQuick
Item {
 id: list
 property bool agents: true
 property var selected: null
 property int page: 0
 property var rows: agents ? (desk.state.tasks||[]) : ((desk.state.news||{}).items||[])
 function status(t){return ({running:"运行中",done:"已完成",waiting:"待接管",error:"出错",stopped:"已停止",unknown:"空闲"})[t.status]||""}
 onAgentsChanged: {selected=null;page=0}
 Text { x:56;y:160;text:list.agents ? (desk.connected?"电脑已连接":"电脑未连接 · 上次状态") : "AIHOT · 最近同步的日报";font.family:uiFontFamily;font.pixelSize:28;color:"#555" }
 Column { x:56;y:234;width:parent.width-112;spacing:18;visible:!list.selected
  Repeater { model:list.rows.slice(list.page*5,list.page*5+5)
   Item { required property var modelData;width:list.width-112;height:178
    Text { width:parent.width;text:(list.agents?modelData.provider+" · "+list.status(modelData):modelData.source||"AIHOT");font.family:uiFontFamily;font.pixelSize:24;color:"#555" }
    Text { y:44;width:parent.width;text:modelData.title;wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight;font.family:uiFontFamily;font.pixelSize:32 }
    Rectangle { anchors.bottom:parent.bottom;width:parent.width;height:1;color:"#bbb" }
    MouseArea { anchors.fill:parent;onClicked:{list.selected=modelData;body.contentY=0} }
   }
  }
 }
 Text { x:56;y:260;visible:!list.selected&&!list.rows.length;text:"暂时没有内容";font.family:uiFontFamily;font.pixelSize:32 }
 Flickable { id:body;x:56;y:234;width:parent.width-112;height:920;clip:true;visible:!!list.selected;contentHeight:detail.height
  Text { id:detail;width:body.width;text:list.selected?(list.selected.title+"\n\n"+(list.agents?list.selected.progress:list.selected.summary||"")):"";wrapMode:Text.Wrap;textFormat:Text.PlainText;font.family:uiFontFamily;font.pixelSize:30;lineHeight:1.6 }
 }
 DeskButton { x:56;y:1190;text:list.selected?"返回列表":"上一页";onClicked:{if(list.selected)list.selected=null;else list.page=Math.max(0,list.page-1)} }
 DeskButton { anchors.right:parent.right;anchors.rightMargin:56;y:1190;text: list.selected?"往下看":"下一页";onClicked:{if(list.selected)body.contentY=Math.min(Math.max(0,body.contentHeight-body.height),body.contentY+body.height-60);else list.page=Math.min(Math.max(0,Math.ceil(list.rows.length/5)-1),list.page+1)} }
}
