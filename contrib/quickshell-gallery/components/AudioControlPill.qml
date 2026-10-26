pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property int volume: 60
    property bool muted: false
    property color colSurfaceRaised: "#222730"
    property color colBorderSubtle: "#2D3440"
    property color colPacingCyan: "#38BDF8"
    property color colTelemetryAmber: "#F59E0B"
    property color colTextPrimary: "#F1F5F9"
    property color colTextMuted: "#94A3B8"

    signal volumeModified(int newVolume)
    signal muteToggled(bool isMuted)

    implicitWidth: 260
    implicitHeight: 38
    radius: 6
    color: root.colSurfaceRaised
    border.color: root.colBorderSubtle
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 8

        // Mute / Unmute Button
        Rectangle {
            id: muteBtn
            width: 26
            height: 26
            radius: 4
            color: root.muted ? Qt.rgba(245/255, 158/255, 11/255, 0.2) : (muteBtnArea.containsMouse ? Qt.rgba(255,255,255,0.08) : "transparent")
            border.color: root.muted ? root.colTelemetryAmber : root.colBorderSubtle
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: root.muted ? "🔇" : (root.volume > 50 ? "🔊" : (root.volume > 0 ? "🔉" : "🔈"))
                font.pixelSize: 13
            }

            MouseArea {
                id: muteBtnArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.muted = !root.muted;
                    root.muteToggled(root.muted);
                }
            }
        }

        // Volume Mini Track
        Item {
            Layout.fillWidth: true
            implicitHeight: 18

            Rectangle {
                id: trackBg
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: 5
                radius: 2.5
                color: "#191C22"
                border.color: root.colBorderSubtle
                border.width: 1

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: root.muted ? 0 : Math.max(0, Math.min(parent.width, (root.volume / 100.0) * parent.width))
                    radius: 2.5
                    color: root.muted ? root.colTextMuted : root.colPacingCyan
                    opacity: 0.9
                }
            }

            Rectangle {
                width: 12
                height: 12
                radius: 6
                color: volArea.pressed ? root.colPacingCyan : root.colSurfaceRaised
                border.color: root.muted ? root.colBorderSubtle : root.colPacingCyan
                border.width: 2
                anchors.verticalCenter: parent.verticalCenter
                x: root.muted ? 0 : Math.max(0, Math.min(parent.width - width, ((root.volume / 100.0) * (parent.width - width))))
                visible: !root.muted
            }

            MouseArea {
                id: volArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                enabled: !root.muted

                function update(mouseX) {
                    const pct = Math.max(0, Math.min(100, Math.round((mouseX / width) * 100)));
                    if (pct !== root.volume) {
                        root.volume = pct;
                        root.volumeModified(pct);
                    }
                }

                onPressed: (mouse) => update(mouse.x)
                onPositionChanged: (mouse) => {
                    if (pressed) update(mouse.x);
                }
            }
        }

        // Percentage Readout
        Text {
            text: root.muted ? "MUTE" : (root.volume + "%")
            font.pixelSize: 11
            font.family: "JetBrains Mono, monospace"
            font.weight: Font.Bold
            color: root.muted ? root.colTelemetryAmber : root.colPacingCyan
            Layout.minimumWidth: 36
            horizontalAlignment: Text.AlignRight
        }
    }
}
