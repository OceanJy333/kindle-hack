// input: desk controller and navigation; output: tap-to-start list for filming the demo shots; pos: hidden desk page opened from settings.
import QtQuick
Item {
 id: shoot
 signal open(string next)
 property var done: ({})
 // Games first, then login, so reading and sync run on a fresh session.
 readonly property var shots: [
  {n:"1",title:"合金弹头 · 开头试玩",line:"它能玩《合金弹头》",act:"mslug"},
  {n:"2",title:"合金弹头 · 清晰版 A2",line:"看得清清楚楚",act:"mslug"},
  {n:"3",title:"合金弹头 · 模糊版 旧刷新",line:"残影就糊成一片",act:"mslug-old"},
  {n:"4",title:"Doom · 小掌机",line:"就完全变成了一个小掌机",act:"doom"},
  {n:"5",title:"微信读书 · 扫码登录",line:"熟悉的书架就出来了",act:"login"},
  {n:"6",title:"微信读书 · 翻一页",line:"还装上了自己做的微信读书",act:"reader"},
  {n:"7",title:"微信读书 · 进度同步",line:"两边来回切都能接上",act:"reader"}
 ]
 function start(s){
  var d=Object.assign({},done);d[s.n]=true;done=d
  if(s.act==="mslug-old")desk.launchGame("mslug","old")
  else if(s.act==="mslug"||s.act==="doom")desk.launchGame(s.act,"")
  else{if(s.act==="login")appController.logout();open("reader")}
 }
 Text { x:56;y:160;width:parent.width-112;text:"每条点「开始」直接进入，拍完最后点「返回首页」拍桌面。";wrapMode:Text.Wrap;font.family:uiFontFamily;font.pixelSize:26;color:"#555" }
 Repeater { model:shoot.shots
  Item { required property var modelData;required property int index;x:56;y:250+index*132;width:shoot.width-112;height:120
   Text { y:14;text:modelData.n;font.family:uiFontFamily;font.pixelSize:40;font.bold:true;color:shoot.done[modelData.n]?"#999":"black" }
   Text { x:64;y:12;text:modelData.title;font.family:uiFontFamily;font.pixelSize:32;font.bold:true;color:shoot.done[modelData.n]?"#999":"black" }
   Text { x:64;y:62;text:"“"+modelData.line+"”";font.family:uiFontFamily;font.pixelSize:24;color:"#555" }
   DeskButton { anchors.right:parent.right;y:14;width:170;text:shoot.done[modelData.n]?"再拍":"开始";primary:!shoot.done[modelData.n];onClicked:shoot.start(modelData) }
   Rectangle { y:118;width:parent.width;height:1;color:"#bbb" }
  }
 }
 Text { x:56;y:1180;width:parent.width-420;text:desk.error||"第 5 条会先退出登录，用手机扫码登回。";wrapMode:Text.Wrap;font.family:uiFontFamily;font.pixelSize:24;color:"#555" }
 DeskButton { x:parent.width-276;y:1170;text:"返回首页";onClicked:shoot.open("clock") }
}
