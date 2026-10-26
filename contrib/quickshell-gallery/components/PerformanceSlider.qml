pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property string label: ""
    property real value: 30
    property real from: 0
    property real to: 144
    property real stepSize: 1
    property string unit: "FPS"
    property string zeroLabel: "0"
    property var ticks: []
    property color accentColor: "#38BDF8"
    property color colSurfaceRaised: "#222730"
    property color colBorderSubtle: "#2D3440"
    property color colTextPrimary: "#F1F5F9"
    property color colTextMuted: "#94A3B8"
    property color colTextDim: "#64748B"

    signal valueModified(real newValue)

    implicitWidth: 260
    implicitHeight: ticks.length > 0 ? 56 : 42

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        // Label and exact tabular value readout
        RowLayout {
            Layout.fillWidth: true

            Text {
                text: root.label
                font.pixelSize: 11
                font.weight: Font.DemiBold
                color: root.colTextMuted
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            Text {
                text: (root.value === 0 && root.zeroLabel !== "") 
                      ? root.zeroLabel 
                      : (Math.round(root.value) + " " + root.unit)
                font.pixelSize: 12
                font.family: "JetBrains Mono, monospace"
                font.weight: Font.Bold
                color: root.accentColor
            }
        }

        // Track and thumb container
        Item {
            id: trackContainer
            Layout.fillWidth: true
            implicitHeight: 20

            // Background track
            Rectangle {
                id: bgTrack
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: 6
                radius: 3
                color: root.colSurfaceRaised
                border.color: root.colBorderSubtle
                border.width: 1

                // Active filled portion
                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: Math.max(0, Math.min(parent.width, (root.value - root.from) / Math.max(1, (root.to - root.from)) * parent.width))
                    radius: 3
                    color: root.accentColor
                    opacity: 0.85
                }
            }

            // Draggable Thumb
            Rectangle {
                id: thumb
                width: 16
                height: 16
                radius: 8
                color: sliderArea.pressed ? root.accentColor : root.colSurfaceRaised
                border.color: root.accentColor
                border.width: 2
                anchors.verticalCenter: parent.verticalCenter
                x: Math.max(0, Math.min(trackContainer.width - width, 
                    ((root.value - root.from) / Math.max(1, (root.to - root.from))) * (trackContainer.width - width)))

                Behavior on scale { NumberAnimation { duration: 100 } }
                scale: sliderArea.pressed ? 1.2 : (sliderArea.containsMouse ? 1.1 : 1.0)
            }

            MouseArea {
                id: sliderArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor

                function updateVal(mouseX) {
                    const ratio = Math.max(0, Math.min(1, mouseX / width));
                    let raw = root.from + ratio * (root.to - root.from);
                    if (root.stepSize > 0) {
                        raw = Math.round(raw / root.stepSize) * root.stepSize;
                    }
                    raw = Math.max(root.from, Math.min(root.to, raw));
                    if (raw !== root.value) {
                        root.value = raw;
                        root.valueModified(raw);
                    }
                }

                onPressed: (mouse) => updateVal(mouse.x)
                onPositionChanged: (mouse) => {
                    if (pressed) updateVal(mouse.x);
                }
            }
        }

        // Ticks row (if specified)
        Item {
            Layout.fillWidth: true
            implicitHeight: 14
            visible: root.ticks.length > 0

            Repeater {
                model: root.ticks

                Item {
                    id: tickItem
                    required property real modelData
                    required property int index

                    readonly property real normPos: (modelData - root.from) / Math.max(1, (root.to - root.from))
                    x: Math.round(normPos * (parent.width - 16))
                    width: 16
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        width: 1
                        height: 3
                        color: Math.abs(root.value - tickItem.modelData) < (root.stepSize / 2 + 0.1) 
                               ? root.accentColor : root.colBorderSubtle
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        text: String(tickItem.modelData)
                        font.pixelSize: 9
                        font.family: "JetBrains Mono, monospace"
                        color: Math.abs(root.value - tickItem.modelData) < (root.stepSize / 2 + 0.1) 
                               ? root.accentColor : root.colTextDim
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.value = tickItem.modelData;
                            root.valueModified(tickItem.modelData);
                        }
                    }
                }
            }
        }
    }
}
