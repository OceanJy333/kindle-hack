// input: Auth QR/session state; output: independent scan-to-login page; pos: public reader login.
import QtQuick
Rectangle {
    id: login
    color: "white"
    signal exitRequested()
    property bool accepted: false
    property bool policyOpen: false
    function showPolicy(url) {
        policyText.contentY = 0; policyOpen = true
        appController.loadPolicyDocument(url)
    }
    onVisibleChanged: { if (!visible) { policyOpen = false; appController.cancelPolicyDocument() } }
    component Action: Rectangle {
        property string label
        property bool primary: false
        signal activated()
        width: 280; height: 80; color: primary ? "black" : "white"; border.width: 2; border.color: "black"; radius: 8
        Text { anchors.centerIn: parent; text: parent.label; color: parent.primary ? "white" : "black"; font.family: uiFontFamily; font.pixelSize: 28 }
        MouseArea { anchors.fill: parent; onClicked: parent.activated() }
    }
    Text { x: 64; y: 52; text: "微信读书"; font.family: uiFontFamily; font.pixelSize: 48; font.bold: true }
    Text { anchors.right: parent.right; anchors.rightMargin: 64; y: 66; text: "退出"; font.pixelSize: 28
        MouseArea { anchors.fill: parent; anchors.margins: -24; onClicked: login.exitRequested() } }
    Text { anchors.horizontalCenter: parent.horizontalCenter; y: 270; text: "用微信扫码，接着读"; font.family: uiFontFamily; font.pixelSize: 42; font.bold: true }
    Rectangle {
        x: (parent.width - width) / 2; y: 390; width: 440; height: 440; border.width: 2; border.color: "black"; color: "white"
        Image { anchors.fill: parent; anchors.margins: 16; cache: false; smooth: false
            source: login.accepted && appController.qrUrl ? "image://qr/" + encodeURIComponent(appController.qrUrl) : "" }
        Text { anchors.centerIn: parent; width: parent.width - 48; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
            visible: !login.accepted || !appController.qrUrl
            text: !login.accepted ? "同意协议后生成二维码" : appController.loginStatusMessage || "正在生成二维码…"
            font.family: uiFontFamily; font.pixelSize: 28 }
    }
    Text {
        x: 80; y: 870; width: parent.width - 160; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
        text: appController.loginHasError ? appController.errorMessage : appController.loginStatusMessage
        font.family: uiFontFamily; font.pixelSize: 28; color: "#555555"
    }
    Action {
        anchors.horizontalCenter: parent.horizontalCenter; y: 970; primary: true
        label: login.accepted ? "刷新二维码" : "同意并生成二维码"
        onActivated: { login.accepted = true; appController.refreshQrCode() }
    }
    Text { anchors.horizontalCenter: parent.horizontalCenter; y: 1120; text: "登录即同意以下协议"; font.pixelSize: 24; color: "#555555" }
    Row {
        anchors.horizontalCenter: parent.horizontalCenter; y: 1174; spacing: 42
        Text { text: "微信读书用户协议"; font.pixelSize: 25; font.underline: true
            MouseArea { anchors.fill: parent; anchors.margins: -18; onClicked: login.showPolicy("https://cdn.weread.qq.com/app/weread_user_agreement_android.html") } }
        Text { text: "隐私保护指引"; font.pixelSize: 25; font.underline: true
            MouseArea { anchors.fill: parent; anchors.margins: -18; onClicked: login.showPolicy("https://weread.qq.com/policy") } }
    }
    Rectangle {
        anchors.fill: parent; visible: login.policyOpen; color: "white"; z: 5
        Text { x: 56; y: 42; text: "‹ 返回登录"; font.pixelSize: 30; font.bold: true
            MouseArea { anchors.fill: parent; anchors.margins: -20; onClicked: { login.policyOpen = false; appController.cancelPolicyDocument() } } }
        Flickable {
            id: policyText; x: 56; y: 130; width: parent.width - 112; height: parent.height - 260; clip: true; interactive: false
            contentHeight: policyBody.implicitHeight; contentWidth: width
            Text { id: policyBody; width: parent.width; wrapMode: Text.Wrap; font.family: uiFontFamily; font.pixelSize: 26
                text: appController.policyDocumentLoading ? "正在加载…" : appController.policyDocumentError || appController.policyDocumentHtml
                textFormat: Text.RichText }
        }
        Row { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 24; spacing: 32
            Action { width: 200; label: "上一页"; onActivated: policyText.contentY = Math.max(0, policyText.contentY - policyText.height) }
            Action { width: 200; label: "下一页"; onActivated: policyText.contentY = Math.min(Math.max(0, policyText.contentHeight - policyText.height), policyText.contentY + policyText.height) }
        }
    }
}
