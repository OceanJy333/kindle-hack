// input: handoff record; output: native silent interruption and result text; pos: highest-priority application surface.
import QtQuick
Rectangle {
 id: panel;color:"white";visible:Object.keys(desk.notice).length>0
 property var item:desk.notice
 onItemChanged: body.contentY=0
 Text { x:56;y:110;text:panel.item.status==="done"?"结果已就绪":panel.item.status==="waiting"?"等你接管":"任务遇到问题";font.family:uiFontFamily;font.pixelSize:52;font.bold:true }
 Text { x:56;y:190;text:panel.item.provider||"";font.family:uiFontFamily;font.pixelSize:28;color:"#555" }
 Text { x:56;y:278;width:parent.width-112;text:panel.item.title||"";font.family:uiFontFamily;font.pixelSize:38;wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight }
 Rectangle { x:56;y:430;width:parent.width-112;height:2;color:"black" }
 Flickable { id:body;x:56;y:470;width:parent.width-112;height:680;contentHeight:content.height;clip:true
  Text { id:content;width:body.width;text:panel.item.body||"回到电脑查看结果";textFormat:Text.PlainText;wrapMode:Text.Wrap;font.family:uiFontFamily;font.pixelSize:30;lineHeight:1.6 }
 }
 DeskButton { x:56;y:1170;text:"往下看";onClicked:body.contentY=Math.min(Math.max(0,body.contentHeight-body.height),body.contentY+body.height-60) }
 DeskButton { x:56;y:1280;width:456;height:96;primary:true;text:"已看，继续消遣";onClicked:desk.dismiss(false) }
 DeskButton { x:536;y:1280;width:480;height:96;text:"稍后再看";onClicked:desk.dismiss(true) }
}
