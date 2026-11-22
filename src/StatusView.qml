import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// What a list shows instead of rows: a spinner while it loads, what went
// wrong if it failed, or a line saying it's empty.
Item {
    id: status
    property var model
    property string emptyText
    signal retry()

    readonly property bool loading: !!model && model.loading && model.count === 0
    readonly property bool failed: !!model && model.error.length > 0 && model.count === 0
    readonly property bool empty: !!model && !model.loading && !failed && model.count === 0
    visible: loading || failed || (empty && emptyText.length > 0)

    BusyIndicator {
        anchors.centerIn: parent
        running: status.loading
        visible: status.loading
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 480)
        visible: !status.loading
        spacing: 12

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: status.failed ? status.model.error : status.emptyText
            color: status.failed ? theme.danger : theme.muted
        }
        Button {
            Layout.alignment: Qt.AlignHCenter
            visible: status.failed
            text: qsTr("Try again")
            flat: true
            focusPolicy: Qt.NoFocus
            onClicked: status.retry()
        }
    }
}
