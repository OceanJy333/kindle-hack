// input: local power and WPA controller; output: brightness and Wi-Fi setup; pos: independent device settings.
import QtQuick
Item {
 id: settings
 property var selected: null
 property int page: 0
 signal close()
 signal shoot()
 InkStyle { id: theme }
 function clearSelection() { selected = null; password.clear(); password.showPassword = false }
 function choose(network) { selected = network; password.clear(); password.showPassword = false; password.forceActiveFocus() }
 onVisibleChanged: { wifi.setActive(visible); if (!visible) clearSelection() }
 Component.onCompleted: wifi.setActive(visible)
 Connections {
  target: wifi
  function onChanged() {
   settings.page = Math.min(settings.page, Math.max(0, Math.ceil(wifi.networks.length / 5) - 1))
   if (settings.selected && wifi.connectedSsid === settings.selected.ssid && !wifi.busy && !wifi.error) {
    settings.clearSelection()
   }
  }
 }
 Item {
  anchors.fill: parent
  visible: !settings.selected
  Text { x: theme.margin; y: 166; text: "亮度 " + desk.light + " / 24"; font.family: uiFontFamily; font.pixelSize: theme.heading }
  DeskButton { x: parent.width - 396; y: 154; width: 140; text: "调暗"; onClicked: desk.setLight(desk.light - 1) }
  DeskButton { x: parent.width - 206; y: 154; width: 150; text: "调亮"; onClicked: desk.setLight(desk.light + 1) }
  Rectangle { x: theme.margin; y: 280; width: parent.width - 2 * theme.margin; height: 1; color: theme.line }
  Text { x: theme.margin; y: 320; text: "Wi-Fi"; font.family: uiFontFamily; font.pixelSize: theme.heading; font.bold: true }
  Text {
   x: theme.margin; y: 380; width: parent.width - 2 * theme.margin - 240
   text: wifi.connectedSsid ? wifi.connectedSsid + " · " + (wifi.address || wifi.status) : wifi.status
   font.family: uiFontFamily; font.pixelSize: theme.body; color: theme.secondary; elide: Text.ElideRight
  }
  DeskButton { x: parent.width - 276; y: 344; text: wifi.scanning ? "正在查找" : "刷新网络"; enabled: !wifi.scanning && !wifi.busy; onClicked: wifi.refresh() }
  Text {
   x: theme.margin; y: 460; width: parent.width - 2 * theme.margin; wrapMode: Text.Wrap
   text: wifi.error || (wifi.networks.length ? "点选网络即可连接。" : wifi.scanning ? "正在查找附近的网络…" : "没有找到网络，请靠近路由器后刷新。")
   font.family: uiFontFamily; font.pixelSize: theme.small; color: theme.secondary
  }
  Column {
   x: theme.margin; y: 540; width: parent.width - 2 * theme.margin
   Repeater {
    model: wifi.networks.slice(settings.page * 5, settings.page * 5 + 5)
    Item {
     required property var modelData
     width: parent.width; height: theme.rowHeight
     Text { y: 12; width: parent.width - 260; text: modelData.ssid; font.family: uiFontFamily; font.pixelSize: theme.body; elide: Text.ElideRight }
     Text { y: 56; text: modelData.security + (modelData.ssid === wifi.connectedSsid ? " · 已连接" : ""); font.family: uiFontFamily; font.pixelSize: theme.small; color: theme.secondary }
     DeskButton { anchors.right: parent.right; y: 10; width: 200; text: "连接"; enabled: modelData.supported && !wifi.busy; onClicked: settings.choose(modelData) }
     Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: theme.line }
    }
   }
  }
  DeskButton { x: theme.margin; y: 1140; text: "上一页"; enabled: settings.page > 0; onClicked: settings.page-- }
  DeskButton { x: 306; y: 1140; text: "下一页"; enabled: (settings.page + 1) * 5 < wifi.networks.length; onClicked: settings.page++ }
  DeskButton { x: parent.width - 276; y: 1040; text: "拍摄清单"; onClicked: settings.shoot() }
  DeskButton { x: parent.width - 276; y: 1140; text: "返回首页"; onClicked: settings.close() }
 }
 Item {
  anchors.fill: parent
  visible: settings.selected !== null
  Text { x: theme.margin; y: 166; width: parent.width - 2 * theme.margin; text: settings.selected ? settings.selected.ssid : ""; font.family: uiFontFamily; font.pixelSize: theme.heading; elide: Text.ElideRight }
  Text { x: theme.margin; y: 240; text: settings.selected && settings.selected.secured ? "Wi-Fi 密码" : "开放网络，无需密码"; font.family: uiFontFamily; font.pixelSize: theme.body }
  Rectangle {
   x: theme.margin; y: 300; width: parent.width - 2 * theme.margin; height: 100
   visible: settings.selected ? settings.selected.secured : false
   color: theme.paper; border.color: theme.ink; border.width: 2; radius: 8
   TextInput {
    id: password
    objectName: "wifiPassword"
    x: 24; y: 24; width: parent.width - 210; height: 54; clip: true
    font.family: uiFontFamily; font.pixelSize: theme.body; color: theme.ink
    echoMode: showPassword ? TextInput.Normal : TextInput.Password
    property bool showPassword: false
    maximumLength: 64; selectByMouse: true; cursorVisible: activeFocus; readOnly: wifi.busy
    onAccepted: { if (!wifi.busy) wifi.join(settings.selected, text) }
   }
   DeskButton { anchors.right: parent.right; anchors.rightMargin: 12; y: 10; width: 150; text: password.showPassword ? "隐藏" : "显示"; onClicked: password.showPassword = !password.showPassword }
  }
  Text { x: theme.margin; y: 430; width: parent.width - 2 * theme.margin; text: wifi.error || (wifi.busy ? "正在保存连接设置…" : wifi.status); wrapMode: Text.Wrap; font.family: uiFontFamily; font.pixelSize: theme.small; color: theme.secondary }
  DeskButton { x: theme.margin; y: 530; text: wifi.busy ? "正在连接" : "连接网络"; primary: true; enabled: !wifi.busy; onClicked: wifi.join(settings.selected, password.text) }
  DeskButton { x: 336; y: 530; text: "返回网络列表"; enabled: !wifi.busy; onClicked: settings.clearSelection() }
  InkKeyboard { x: theme.margin; y: 680; width: parent.width - 2 * theme.margin; target: password; enabled: !wifi.busy; visible: settings.selected ? settings.selected.secured : false }
 }
}
