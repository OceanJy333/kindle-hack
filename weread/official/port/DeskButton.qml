// input: label and activation; output: shared e-ink touch control; pos: native desk component.
import QtQuick
Rectangle {
 id: control
 property string text: ""
 property bool primary: false
 signal clicked()
 implicitWidth: 220; implicitHeight: 80
 color: primary ? "black" : "white"; border.color: "black"; border.width: 2; radius: 8
 Text { anchors.centerIn: parent; text: control.text; color: !control.enabled ? "#777777" : control.primary ? "white" : "black"; font.family: uiFontFamily; font.pixelSize: 28 }
 MouseArea { anchors.fill: parent; onClicked: control.clicked() }
}
