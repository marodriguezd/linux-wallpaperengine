pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property var screens: []
    property string selectedScreen: ""
    property string multimonitorMode: "clone"
    property string targetWallpaperId: ""
    property color colSurfaceRaised: "#222730"
    property color colSurface: "#191C22"
    property color colBorderSubtle: "#2D3440"
    property color colPacingCyan: "#38BDF8"
    property color colTelemetryAmber: "#F59E0B"
    property color colTextPrimary: "#F1F5F9"
    property color colTextMuted: "#94A3B8"
    property color colTextDim: "#64748B"

    signal screenSelected(string screenName)
    signal modeChanged(string newMode)
    signal assignRequested(string screenName, string wpId)

    implicitWidth: 280
    implicitHeight: 180

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        // Mode switch row: Clone vs Individual
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: "Display Mode:"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                color: root.colTextMuted
                Layout.fillWidth: true
            }

            Rectangle {
                implicitWidth: 160
                implicitHeight: 26
                radius: 4
                color: root.colSurface
                border.color: root.colBorderSubtle
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Clone tab
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 3
                        color: root.multimonitorMode === "clone" ? root.colPacingCyan : "transparent"

                        Text {
                            anchors.centerIn: parent
                            text: "Clone"
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            color: root.multimonitorMode === "clone" ? "#111317" : root.colTextMuted
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.multimonitorMode !== "clone") {
                                    root.multimonitorMode = "clone";
                                    root.modeChanged("clone");
                                }
                            }
                        }
                    }

                    // Independent tab
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 3
                        color: root.multimonitorMode === "independent" ? root.colPacingCyan : "transparent"

                        Text {
                            anchors.centerIn: parent
                            text: "Individual"
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            color: root.multimonitorMode === "independent" ? "#111317" : root.colTextMuted
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.multimonitorMode !== "independent") {
                                    root.multimonitorMode = "independent";
                                    root.modeChanged("independent");
                                }
                            }
                        }
                    }
                }
            }
        }

        // Graphical Monitor Stage / Canvas
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 90
            radius: 6
            color: root.colSurface
            border.color: root.colBorderSubtle
            border.width: 1

            RowLayout {
                anchors.centerIn: parent
                spacing: 10

                Repeater {
                    model: root.screens

                    Rectangle {
                        id: screenBox
                        required property var modelData
                        required property int index

                        readonly property bool isSelected: (root.selectedScreen === modelData.name)
                        readonly property bool isPrimary: Boolean(modelData.primary)
                        width: 110
                        height: 64
                        radius: 5
                        color: isSelected ? Qt.rgba(56/255, 189/255, 248/255, 0.12) : root.colSurfaceRaised
                        border.color: isSelected ? root.colPacingCyan : root.colBorderSubtle
                        border.width: isSelected ? 2 : 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 4
                            spacing: 2

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: screenBox.modelData.name || ("Display " + (screenBox.index + 1))
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                    color: screenBox.isSelected ? root.colPacingCyan : root.colTextPrimary
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                Rectangle {
                                    visible: screenBox.isPrimary
                                    width: 32
                                    height: 12
                                    radius: 2
                                    color: Qt.rgba(245/255, 158/255, 11/255, 0.2)
                                    border.color: root.colTelemetryAmber
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: "PRI"
                                        font.pixelSize: 7
                                        font.weight: Font.Bold
                                        color: root.colTelemetryAmber
                                    }
                                }
                            }

                            Text {
                                text: (screenBox.modelData.width || 1920) + "x" + (screenBox.modelData.height || 1080)
                                font.pixelSize: 8
                                font.family: "JetBrains Mono, monospace"
                                color: root.colTextDim
                            }

                            Text {
                                text: screenBox.modelData.current_id ? ("ID: " + screenBox.modelData.current_id) : "No wallpaper"
                                font.pixelSize: 8
                                font.family: "JetBrains Mono, monospace"
                                color: screenBox.modelData.current_id ? root.colPacingCyan : root.colTextDim
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.selectedScreen = screenBox.modelData.name;
                                root.screenSelected(screenBox.modelData.name);
                            }
                        }
                    }
                }
            }
        }

        // Action: Apply to selected screen
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 30
            radius: 4
            color: assignArea.containsMouse ? root.colPacingCyan : Qt.rgba(56/255, 189/255, 248/255, 0.15)
            border.color: root.colPacingCyan
            border.width: 1
            visible: root.selectedScreen !== "" && root.targetWallpaperId !== ""

            Text {
                anchors.centerIn: parent
                text: "Apply to " + root.selectedScreen + " (" + root.targetWallpaperId + ")"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                color: assignArea.containsMouse ? "#111317" : root.colPacingCyan
            }

            MouseArea {
                id: assignArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.assignRequested(root.selectedScreen, root.targetWallpaperId);
                }
            }
        }
    }
}
