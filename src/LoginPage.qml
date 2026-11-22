import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Signing in to a Jellyfin server: its address, a user name, a password.
// Only the token the server returns is kept.
FocusScope {
    id: page

    function submit() {
        jellyfin.signIn(server.text, user.text, password.text)
    }

    onVisibleChanged: if (visible) focusFirstEmpty()
    Component.onCompleted: focusFirstEmpty()
    function focusFirstEmpty() {
        if (server.text.length === 0) server.forceActiveFocus()
        else if (user.text.length === 0) user.forceActiveFocus()
        else password.forceActiveFocus()
    }

    Connections {
        target: jellyfin
        function onSessionChanged() { password.text = "" }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 400)
        spacing: 14

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "lyre"
            font.pixelSize: Math.round(theme.fontSize * 3.2)
            font.weight: Font.Bold
            color: theme.accent
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 12
            text: qsTr("Sign in to your Jellyfin server")
            color: theme.muted
        }

        TextField {
            id: server
            Layout.fillWidth: true
            placeholderText: qsTr("Server address")
            text: jellyfin.server
            inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
            Material.accent: theme.accent
            Material.containerStyle: Material.Outlined
            enabled: !jellyfin.busy
            onAccepted: user.forceActiveFocus()
        }
        TextField {
            id: user
            Layout.fillWidth: true
            placeholderText: qsTr("User name")
            text: jellyfin.userName
            inputMethodHints: Qt.ImhNoAutoUppercase
            Material.accent: theme.accent
            Material.containerStyle: Material.Outlined
            enabled: !jellyfin.busy
            onAccepted: password.forceActiveFocus()
        }
        TextField {
            id: password
            Layout.fillWidth: true
            placeholderText: qsTr("Password")
            echoMode: TextInput.Password
            Material.accent: theme.accent
            Material.containerStyle: Material.Outlined
            enabled: !jellyfin.busy
            onAccepted: page.submit()
        }

        Label {
            Layout.fillWidth: true
            visible: jellyfin.error.length > 0
            text: jellyfin.error
            wrapMode: Text.WordWrap
            color: theme.danger
        }

        AccentButton {
            Layout.fillWidth: true
            Layout.topMargin: 6
            text: jellyfin.busy ? qsTr("Signing in…") : qsTr("Sign in")
            enabled: !jellyfin.busy
            onClicked: page.submit()
        }
    }
}
