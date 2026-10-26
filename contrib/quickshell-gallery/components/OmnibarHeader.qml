pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    required property var galleryModel
    property bool dockOpen: true
    property color colBaseVoid: "#111317"
    property color colSurface: "#191C22"
    property color colSurfaceRaised: "#222730"
    property color colBorderSubtle: "#2D3440"
    property color colPacingCyan: "#38BDF8"
    property color colTelemetryAmber: "#F59E0B"
    property color colTextPrimary: "#F1F5F9"
    property color colTextMuted: "#94A3B8"
    property color colTextDim: "#64748B"
    property color colSuccess: "#10B981"
    property color colDanger: "#EF4444"

    signal toggleDockRequested()

    implicitWidth: 880
    implicitHeight: 46
    radius: 8
    color: root.colSurface
    border.color: root.colBorderSubtle
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 10

        // ================= ENGINE STATUS TOGGLE PILL =================
        Rectangle {
            implicitWidth: 100
            implicitHeight: 28
            radius: 5
            color: root.galleryModel.isRunning ? Qt.rgba(16/255, 185/255, 129/255, 0.15) : Qt.rgba(239/255, 68/255, 68/255, 0.15)
            border.color: root.galleryModel.isRunning ? root.colSuccess : root.colDanger
            border.width: 1

            RowLayout {
                anchors.centerIn: parent
                spacing: 6

                Rectangle {
                    width: 7
                    height: 7
                    radius: 3.5
                    color: root.galleryModel.isRunning ? root.colSuccess : root.colDanger
                }

                Text {
                    text: root.galleryModel.isRunning ? "ACTIVE" : "STOPPED"
                    font.pixelSize: 10
                    font.family: "JetBrains Mono, monospace"
                    font.weight: Font.Bold
                    color: root.galleryModel.isRunning ? root.colSuccess : root.colDanger
                }
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.galleryModel.toggleActive()
            }
        }

        // ================= MODE TABS: LOCAL / EXPLORAR =================
        Rectangle {
            implicitWidth: 160
            implicitHeight: 28
            radius: 5
            color: root.colBaseVoid
            border.color: root.colBorderSubtle
            border.width: 1

            RowLayout {
                anchors.fill: parent
                spacing: 2

                // Local tab
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 4
                    color: root.galleryModel.mode === "local" ? root.colSurfaceRaised : "transparent"
                    border.color: root.galleryModel.mode === "local" ? root.colPacingCyan : "transparent"
                    border.width: root.galleryModel.mode === "local" ? 1 : 0

                    Text {
                        anchors.centerIn: parent
                        text: "Local (" + root.galleryModel.items.length + ")"
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        color: root.galleryModel.mode === "local" ? root.colPacingCyan : root.colTextMuted
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.galleryModel.setMode("local")
                    }
                }

                // Explore tab
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 4
                    color: root.galleryModel.mode === "explore" ? root.colSurfaceRaised : "transparent"
                    border.color: root.galleryModel.mode === "explore" ? root.colPacingCyan : "transparent"
                    border.width: root.galleryModel.mode === "explore" ? 1 : 0

                    Text {
                        anchors.centerIn: parent
                        text: "Explore"
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        color: root.galleryModel.mode === "explore" ? root.colPacingCyan : root.colTextMuted
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.galleryModel.setMode("explore")
                    }
                }
            }
        }

        // ================= UNIFIED SEARCH INPUT =================
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 28
            radius: 5
            color: root.colBaseVoid
            border.color: searchInput.activeFocus ? root.colPacingCyan : root.colBorderSubtle
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                Text {
                    text: "🔍"
                    font.pixelSize: 11
                    color: root.colTextDim
                }

                TextInput {
                    id: searchInput
                    Layout.fillWidth: true
                    text: root.galleryModel.mode === "local" ? root.galleryModel.query : root.galleryModel.exploreFilter
                    font.pixelSize: 11
                    color: root.colTextPrimary
                    selectByMouse: true
                    clip: true

                    Text {
                        text: root.galleryModel.mode === "local" ? "Search by title, tag, author..." : "Search online wallpapers..."
                        font.pixelSize: 11
                        color: root.colTextDim
                        visible: !searchInput.text && !searchInput.activeFocus
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    onTextChanged: {
                        if (root.galleryModel.mode === "local") {
                            root.galleryModel.query = text;
                        } else {
                            root.galleryModel.exploreFilter = text;
                        }
                    }

                    onAccepted: {
                        if (root.galleryModel.mode === "explore") {
                            root.galleryModel.searchExplore();
                        }
                    }
                }

                Text {
                    visible: searchInput.text !== ""
                    text: "✕"
                    font.pixelSize: 10
                    color: root.colTextDim

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            searchInput.text = "";
                            if (root.galleryModel.mode === "local") {
                                root.galleryModel.query = "";
                            } else {
                                root.galleryModel.exploreFilter = "";
                            }
                        }
                    }
                }
            }
        }

        // ================= QUICK FILTER CHIPS =================
        RowLayout {
            spacing: 4
            visible: root.galleryModel.mode === "local"

            // Favorites chip
            Rectangle {
                implicitWidth: 32
                implicitHeight: 28
                radius: 4
                color: root.galleryModel.showFavorites ? Qt.rgba(245/255, 158/255, 11/255, 0.25) : root.colBaseVoid
                border.color: root.galleryModel.showFavorites ? root.colTelemetryAmber : root.colBorderSubtle
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "★"
                    font.pixelSize: 12
                    color: root.galleryModel.showFavorites ? root.colTelemetryAmber : root.colTextMuted
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.galleryModel.showFavorites = !root.galleryModel.showFavorites
                }
            }

            // Scene chip
            Rectangle {
                implicitWidth: 54
                implicitHeight: 28
                radius: 4
                readonly property bool isActive: (root.galleryModel.typeFilter === "scene")
                color: isActive ? Qt.rgba(56/255, 189/255, 248/255, 0.25) : root.colBaseVoid
                border.color: isActive ? root.colPacingCyan : root.colBorderSubtle
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Scene"
                    font.pixelSize: 10
                    color: parent.isActive ? root.colPacingCyan : root.colTextMuted
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.galleryModel.typeFilter = (root.galleryModel.typeFilter === "scene") ? "" : "scene";
                    }
                }
            }

            // Video chip
            Rectangle {
                implicitWidth: 48
                implicitHeight: 28
                radius: 4
                readonly property bool isActive: (root.galleryModel.typeFilter === "video")
                color: isActive ? Qt.rgba(245/255, 158/255, 11/255, 0.25) : root.colBaseVoid
                border.color: isActive ? root.colTelemetryAmber : root.colBorderSubtle
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Video"
                    font.pixelSize: 10
                    color: parent.isActive ? root.colTelemetryAmber : root.colTextMuted
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.galleryModel.typeFilter = (root.galleryModel.typeFilter === "video") ? "" : "video";
                    }
                }
            }
        }

        // ================= TELEMETRY PILL (GPU & BATTERY) =================
        Rectangle {
            implicitWidth: telemRow.implicitWidth + 14
            implicitHeight: 28
            radius: 5
            color: root.colBaseVoid
            border.color: root.colBorderSubtle
            border.width: 1

            RowLayout {
                id: telemRow
                anchors.centerIn: parent
                spacing: 8

                // Battery badge
                Text {
                    text: root.galleryModel.batteryStatus
                    font.pixelSize: 10
                    font.family: "JetBrains Mono, monospace"
                    font.weight: Font.DemiBold
                    color: root.colTextMuted
                }

                Rectangle {
                    width: 1
                    height: 12
                    color: root.colBorderSubtle
                }

                // GPU badge
                Text {
                    text: root.galleryModel.gpuInfo
                    font.pixelSize: 10
                    font.family: "JetBrains Mono, monospace"
                    font.weight: Font.DemiBold
                    color: root.colPacingCyan
                }
            }
        }

        // ================= DOCK TOGGLE BUTTON =================
        Rectangle {
            implicitWidth: 32
            implicitHeight: 28
            radius: 5
            color: root.dockOpen ? Qt.rgba(56/255, 189/255, 248/255, 0.2) : root.colBaseVoid
            border.color: root.dockOpen ? root.colPacingCyan : root.colBorderSubtle
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "⚙"
                font.pixelSize: 14
                color: root.dockOpen ? root.colPacingCyan : root.colTextMuted
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.toggleDockRequested()
            }
        }
    }
}
