// input: existing monochrome desktop; output: shared settings dimensions; pos: e-ink design tokens.
import QtQuick
QtObject {
 readonly property color paper: "white"
 readonly property color ink: "black"
 readonly property color secondary: "#555555"
 readonly property color line: "#bbbbbb"
 readonly property int margin: 56
 readonly property int gap: 12
 readonly property int body: 28
 readonly property int small: 26
 readonly property int heading: 36
 readonly property int rowHeight: 108
 readonly property int keyHeight: 80
}
