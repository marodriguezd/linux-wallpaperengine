pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Widgets
import qs.core
import "components"

FloatingWindow {
    id: root

    required property var galleryModel

    title: "linux-wallpaperengine"
    visible: galleryModel.visible
    screen: galleryModel.targetScreen ? galleryModel.targetScreen : undefined
    implicitWidth: 1080
    implicitHeight: 680
    color: Theme.transparent

    // "Precision Foundry" Design Tokens
    readonly property color colBaseVoid: "#111317"
    readonly property color colSurface: "#191C22"
    readonly property color colSurfaceRaised: "#222730"
    readonly property color colBorderSubtle: "#2D3440"
    readonly property color colPacingCyan: "#38BDF8"
    readonly property color colTelemetryAmber: "#F59E0B"
    readonly property color colTextPrimary: "#F1F5F9"
    readonly property color colTextMuted: "#94A3B8"
    readonly property color colTextDim: "#64748B"
    readonly property color colSuccess: "#10B981"
    readonly property color colDanger: "#EF4444"
    readonly property int cardRadius: 10

    // Keyboard shortcuts: ESC closes
    Item {
        focus: true
        Keys.onEscapePressed: root.galleryModel.visible = false
    }

    ShellSurface {
        anchors.fill: parent
        color: root.colBaseVoid
        border.color: root.colBorderSubtle
        border.width: 1
        radius: root.cardRadius

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            // 1. OMNIBAR HEADER
            OmnibarHeader {
                Layout.fillWidth: true
                galleryModel: root.galleryModel
                dockOpen: root.galleryModel.controlDockOpen
                colBaseVoid: root.colBaseVoid
                colSurface: root.colSurface
                colSurfaceRaised: root.colSurfaceRaised
                colBorderSubtle: root.colBorderSubtle
                colPacingCyan: root.colPacingCyan
                colTelemetryAmber: root.colTelemetryAmber
                colTextPrimary: root.colTextPrimary
                colTextMuted: root.colTextMuted
                colTextDim: root.colTextDim
                colSuccess: root.colSuccess
                colDanger: root.colDanger
                onToggleDockRequested: {
                    root.galleryModel.controlDockOpen = !root.galleryModel.controlDockOpen;
                }
            }

            // 2. MASTER-DETAIL BODY LAYOUT
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                // ================= LEFT: CATALOG VIEW (~70%) =================
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    // --- LOCAL WALLPAPER GRID ---
                    WallpaperGrid {
                        anchors.fill: parent
                        visible: root.galleryModel.mode === "local"
                        items: root.galleryModel.filteredItems()
                        currentId: root.galleryModel.currentId
                        selectedId: root.galleryModel.selectedId
                        colSurface: root.colSurface
                        colSurfaceRaised: root.colSurfaceRaised
                        colBorderSubtle: root.colBorderSubtle
                        colPacingCyan: root.colPacingCyan
                        colTelemetryAmber: root.colTelemetryAmber
                        colTextPrimary: root.colTextPrimary
                        colTextMuted: root.colTextMuted
                        colTextDim: root.colTextDim

                        onWallpaperSelected: (item) => {
                            root.galleryModel.select(item);
                        }
                        onApplyRequested: (item) => {
                            root.galleryModel.apply(item);
                        }
                        onFavRequested: (item) => {
                            root.galleryModel.toggleFav(item);
                        }
                        onInspectRequested: (item) => {
                            root.galleryModel.inspect(item);
                        }
                        onResetFiltersRequested: {
                            root.galleryModel.query = "";
                            root.galleryModel.typeFilter = "";
                            root.galleryModel.categoryFilter = "";
                            root.galleryModel.showFavorites = false;
                        }
                    }

                    // --- ONLINE EXPLORE VIEW ---
                    ColumnLayout {
                        anchors.fill: parent
                        visible: root.galleryModel.mode === "explore"
                        spacing: 8

                        // Explore source toolbar
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Rectangle {
                                implicitWidth: 320
                                implicitHeight: 28
                                radius: 5
                                color: root.colSurface
                                border.color: root.colBorderSubtle
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    spacing: 2

                                    Repeater {
                                        model: [
                                            { id: "bing", label: "Bing Daily" },
                                            { id: "wallhaven", label: "Wallhaven" },
                                            { id: "motionbgs", label: "MotionBGS" },
                                            { id: "minimal", label: "Minimal" }
                                        ]

                                        Rectangle {
                                            required property var modelData
                                            required property int index
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            radius: 4
                                            readonly property bool isActive: (root.galleryModel.exploreSource === modelData.id)
                                            color: isActive ? root.colSurfaceRaised : "transparent"
                                            border.color: isActive ? root.colPacingCyan : "transparent"
                                            border.width: isActive ? 1 : 0

                                            Text {
                                                anchors.centerIn: parent
                                                text: parent.modelData.label
                                                font.pixelSize: 10
                                                font.weight: Font.DemiBold
                                                color: parent.isActive ? root.colPacingCyan : root.colTextMuted
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: root.galleryModel.setSource(parent.modelData.id)
                                            }
                                        }
                                    }
                                }
                            }

                            Item { Layout.fillWidth: true }

                            // Refresh explore
                            Rectangle {
                                implicitWidth: 70
                                implicitHeight: 28
                                radius: 5
                                color: root.colSurface
                                border.color: root.colBorderSubtle
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: "↻ Search"
                                    font.pixelSize: 11
                                    color: root.colTextPrimary
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.galleryModel.refreshExplore()
                                }
                            }
                        }

                        // Explore Grid View
                        Flickable {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            contentWidth: width
                            contentHeight: exploreFlow.implicitHeight + 60
                            boundsBehavior: Flickable.StopAtBounds
                            clip: true

                            Flow {
                                id: exploreFlow
                                width: parent.width
                                spacing: 12

                                Repeater {
                                    model: root.galleryModel.exploreItems

                                    Rectangle {
                                        id: exploreCard
                                        required property var modelData
                                        required property int index

                                        width: Math.floor((exploreFlow.width - 24) / 3)
                                        height: Math.round(width * 9 / 16)
                                        radius: 8
                                        color: root.colSurfaceRaised
                                        border.color: exploreHover.containsMouse ? root.colPacingCyan : root.colBorderSubtle
                                        border.width: 1
                                        clip: true

                                        Image {
                                            anchors.fill: parent
                                            source: exploreCard.modelData.thumb || ""
                                            fillMode: Image.PreserveAspectCrop
                                            asynchronous: true
                                        }

                                        // Dark gradient
                                        Rectangle {
                                            anchors.fill: parent
                                            gradient: Gradient {
                                                GradientStop { position: 0.5; color: "transparent" }
                                                GradientStop { position: 1.0; color: Qt.rgba(17/255, 19/255, 23/255, 0.9) }
                                            }
                                        }

                                        // Title & install button
                                        ColumnLayout {
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.bottom: parent.bottom
                                            anchors.margins: 8
                                            spacing: 4

                                            Text {
                                                text: exploreCard.modelData.title || exploreCard.modelData.ref || ""
                                                font.pixelSize: 11
                                                font.weight: Font.Bold
                                                color: root.colTextPrimary
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }

                                            Rectangle {
                                                Layout.fillWidth: true
                                                implicitHeight: 22
                                                radius: 4
                                                color: root.colPacingCyan

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "Download / Apply"
                                                    font.pixelSize: 10
                                                    font.weight: Font.Bold
                                                    color: "#111317"
                                                }

                                                MouseArea {
                                                    anchors.fill: parent
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: root.galleryModel.installOrApply(exploreCard.modelData)
                                                }
                                            }
                                        }

                                        MouseArea {
                                            id: exploreHover
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            z: -1
                                            onClicked: root.galleryModel.installOrApply(exploreCard.modelData)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // ================= RIGHT: CONTROL DOCK (~30%) =================
                ControlDock {
                    Layout.fillHeight: true
                    galleryModel: root.galleryModel
                    activeTab: root.galleryModel.controlDockTab
                    isCollapsed: !root.galleryModel.controlDockOpen
                    colBaseVoid: root.colBaseVoid
                    colSurface: root.colSurface
                    colSurfaceRaised: root.colSurfaceRaised
                    colBorderSubtle: root.colBorderSubtle
                    colPacingCyan: root.colPacingCyan
                    colTelemetryAmber: root.colTelemetryAmber
                    colTextPrimary: root.colTextPrimary
                    colTextMuted: root.colTextMuted
                    colTextDim: root.colTextDim
                    colSuccess: root.colSuccess
                    onCollapseToggled: {
                        root.galleryModel.controlDockOpen = !root.galleryModel.controlDockOpen;
                    }
                }
            }

            // 3. SUBTLE STATUS FOOTER BAR
            RowLayout {
                Layout.fillWidth: true
                implicitHeight: 20

                Text {
                    text: root.galleryModel.status !== "" ? root.galleryModel.status : "Ready"
                    font.pixelSize: 10
                    font.family: "JetBrains Mono, monospace"
                    color: root.colTextDim
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Text {
                    text: "linux-wallpaperengine 2.0.0 · Precision Foundry"
                    font.pixelSize: 9
                    font.family: "JetBrains Mono, monospace"
                    color: Qt.rgba(148/255, 163/255, 184/255, 0.4)
                }
            }
        }
    }
}
