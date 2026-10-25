// input: password text input; output: direct ASCII touch typing; pos: device-native e-ink keyboard.
import QtQuick
Item {
 id: keyboard
 property var target
 property bool upper: false
 property bool symbols: false
 readonly property var rows: symbols ? ["1234567890", "!@#$%^&*()", "-_=+[]{}\\|", ";:'\",.<>/?"] : ["1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"]
 InkStyle { id: theme }
 function insert(value) {
  if (!target) return
  var position = target.selectedText.length ? target.selectionStart : target.cursorPosition
  if (target.selectedText.length) target.remove(target.selectionStart, target.selectionEnd)
  var before = target.length
  target.insert(position, value)
  target.cursorPosition = position + target.length - before
  target.forceActiveFocus()
 }
 function erase() {
  if (!target) return
  var start = target.selectedText.length ? target.selectionStart : Math.max(0, target.cursorPosition - 1)
  var end = target.selectedText.length ? target.selectionEnd : target.cursorPosition
  target.remove(start, end)
  target.cursorPosition = start
  target.forceActiveFocus()
 }
 Column {
  spacing: theme.gap
  width: parent.width
  Repeater {
   model: keyboard.rows
   Row {
    required property string modelData
    anchors.horizontalCenter: parent.horizontalCenter
    spacing: theme.gap
    Repeater {
     model: modelData.split("")
     DeskButton {
      required property string modelData
      width: (keyboard.width - 9 * theme.gap) / 10
      height: theme.keyHeight
      text: keyboard.upper ? modelData.toUpperCase() : modelData
      onClicked: keyboard.insert(text)
     }
    }
   }
  }
  Row {
   spacing: theme.gap
   DeskButton { width: 152; text: keyboard.upper ? "小写" : "大写"; onClicked: keyboard.upper = !keyboard.upper }
   DeskButton { width: 152; text: keyboard.symbols ? "字母" : "符号"; onClicked: keyboard.symbols = !keyboard.symbols }
   DeskButton { width: keyboard.width - 152 - 152 - 220 - 3 * theme.gap; text: "空格"; onClicked: keyboard.insert(" ") }
   DeskButton { width: 220; text: "删除"; onClicked: keyboard.erase() }
  }
  Row {
   visible: keyboard.symbols
   spacing: theme.gap
   DeskButton { width: 152; text: "`"; onClicked: keyboard.insert("`") }
   DeskButton { width: 152; text: "~"; onClicked: keyboard.insert("~") }
  }
 }
}
