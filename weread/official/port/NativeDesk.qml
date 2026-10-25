// input: local navigation and desk controller; output: native home, agents, news and games; pos: independent Voyage desktop.
import QtQuick
Item {
 id: deskView
 property string mode:"clock"
 signal openReader()
 signal modeRequested(string next)
 function navigate(next){if(next==="reader")openReader();else modeRequested(next)}
 NativeHome { anchors.fill:parent;visible:deskView.mode==="clock";onOpen:function(next){deskView.navigate(next)} }
 NativeList { anchors.fill:parent;visible:deskView.mode==="news"||deskView.mode==="agent";agents:deskView.mode==="agent" }
 NativeSettings { anchors.fill:parent;visible:deskView.mode==="settings";onClose:deskView.navigate("clock");onShoot:deskView.navigate("shoot") }
 ShootList { anchors.fill:parent;visible:deskView.mode==="shoot";onOpen:function(next){deskView.navigate(next)} }
 Item { anchors.fill:parent;visible:deskView.mode==="games"
  Text { x:56;y:180;text:"等 Agent 的时候，玩一局";font.family:uiFontFamily;font.pixelSize:32 }
  Repeater { model:[{name:"Doom",id:"doom",year:"1993"},{name:"合金弹头",id:"mslug",year:"1996"}]
   Item { required property var modelData;required property int index;x:56;y:290+index*300;width:deskView.width-112;height:240
    Text { text:modelData.name;font.family:uiFontFamily;font.pixelSize:44;font.bold:true }
    Text { y:70;text:modelData.year+" · 静音";font.family:uiFontFamily;font.pixelSize:28;color:"#555" }
    DeskButton { anchors.right:parent.right;y:70;text:"开始游戏";primary:true;onClicked:desk.launchGame(modelData.id) }
    Rectangle { y:215;width:parent.width;height:1;color:"#bbb" }
   }
  }
  Text { x:56;y:980;width:parent.width-112;text:desk.error||"有结果时暂停游戏并提醒，关闭提醒继续玩。";wrapMode:Text.Wrap;font.family:uiFontFamily;font.pixelSize:28 }
  DeskButton { x:56;y:1170;text:"返回首页";onClicked:deskView.navigate("clock") }
 }
 Rectangle { x:0;y:0;width:parent.width;height:124;color:"white"
  Text { x:56;y:28;text:({clock:"等一会儿",agent:"Agent",news:"AI 日报",games:"玩一局",settings:"设置",shoot:"拍摄清单"})[deskView.mode];font.family:uiFontFamily;font.pixelSize:44;font.bold:true }
  Text { anchors.right:parent.right;anchors.rightMargin:56;y:38;text:desk.battery<0?"电量读取中":desk.battery+"%";font.family:uiFontFamily;font.pixelSize:30 }
  Rectangle { x:56;y:122;width:parent.width-112;height:2;color:"black" }
 }
 DeskButton { x:parent.width-206;y:142;width:150;text:"设置";visible:deskView.mode==="clock";onClicked:deskView.navigate("settings") }
}
