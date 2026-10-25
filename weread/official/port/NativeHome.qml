// input: local clock and metadata; output: native leisure home; pos: Voyage landing view.
import QtQuick
Item {
 id: home
 signal open(string mode)
 property date now: new Date()
 property bool analog: false
 property var tasks: desk.state.tasks || []
 readonly property int running: tasks.filter(function(t){return t.status === "running"}).length
 Timer { interval: 60000; running: home.visible && !desk.gameActive; repeat: true; onTriggered: home.now=new Date() }
 onVisibleChanged: if(visible) now=new Date()
 Text { x: 56; y: 168; width: parent.width-112; horizontalAlignment: Text.AlignHCenter; text: Qt.formatDate(home.now,"M 月 d 日")+" 星期"+["日","一","二","三","四","五","六"][home.now.getDay()]; font.family: uiFontFamily; font.pixelSize: 32; color: "#555" }
 Text { x:56; y:250; width:parent.width-112; horizontalAlignment:Text.AlignHCenter; text:Qt.formatTime(home.now,"hh:mm"); font.family:uiFontFamily; font.pixelSize:200 }
 Text { x:56; y:540; width:parent.width-112; horizontalAlignment:Text.AlignHCenter; text:desk.connected ? (home.running + " 个 Agent 运行中 · 有结果会提醒你") : "电脑未连接 · 阅读和游戏照常使用"; font.family:uiFontFamily; font.pixelSize:28; color:"#555" }
 Rectangle { x:56;y:630;width:parent.width-112;height:2;color:"black" }
 Text { x:56;y:670;text:"让 Agent 干活，休息一下";font.family:uiFontFamily;font.pixelSize:34;font.bold:true }
 Repeater {
  model:[{title:"读一会儿",sub:"微信读书",mode:"reader"},{title:"玩一局",sub:"Doom / 合金弹头",mode:"games"},{title:"看看 AI 新闻",sub:"卡兹克 AIHOT",mode:"news"}]
  Item { required property var modelData; required property int index;x:56;y:750+index*104;width:home.width-112;height:94
   Text { y:20;text:modelData.title;font.family:uiFontFamily;font.pixelSize:32 }
   Text { x:310;y:24;text:modelData.sub;font.family:uiFontFamily;font.pixelSize:26;color:"#555" }
   Text { anchors.right:parent.right;y:20;text:"进入";font.family:uiFontFamily;font.pixelSize:28 }
   Rectangle { anchors.bottom:parent.bottom;width:parent.width;height:1;color:"#bbb" }
   MouseArea { anchors.fill:parent;onClicked:home.open(modelData.mode) }
  }
 }
 Text { x:56;y:1110;width:parent.width-112;text:(desk.state.media||{}).title || "电脑没有播放音乐";elide:Text.ElideRight;font.family:uiFontFamily;font.pixelSize:30;font.bold:true }
 Text { x:56;y:1160;width:parent.width-112;text:(desk.state.media||{}).artist || "跟随电脑播放器 · 提醒保持静音";elide:Text.ElideRight;font.family:uiFontFamily;font.pixelSize:26;color:"#555" }
}
