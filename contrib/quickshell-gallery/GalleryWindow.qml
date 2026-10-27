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

        // ================= FLOATING NOTICE / BANNER TOAST =================
        Rectangle {
            id: noticeBanner
            visible: root.galleryModel.noticeMessage !== ""
            anchors.top: parent.top
            anchors.topMargin: 12
            anchors.horizontalCenter: parent.horizontalCenter
            z: 999
            implicitWidth: Math.min(parent.width - 48, noticeLayout.implicitWidth + 32)
            implicitHeight: noticeLayout.implicitHeight + 16
            radius: 8
            color: {
                if (root.galleryModel.noticeType === "blocked" || root.galleryModel.noticeType === "warning") {
                    return "#1F1A15";
                }
                if (root.galleryModel.noticeType === "success") {
                    return "#12241A";
                }
                return root.colSurfaceRaised;
            }
            border.color: {
                if (root.galleryModel.noticeType === "blocked" || root.galleryModel.noticeType === "warning") {
                    return root.colTelemetryAmber;
                }
                if (root.galleryModel.noticeType === "success") {
                    return root.colSuccess;
                }
                return root.colPacingCyan;
            }
            border.width: 1.5

            RowLayout {
                id: noticeLayout
                anchors.centerIn: parent
                spacing: 10

                Text {
                    text: {
                        if (root.galleryModel.noticeType === "blocked") return "🔒";
                        if (root.galleryModel.noticeType === "warning") return "⚠️";
                        if (root.galleryModel.noticeType === "success") return "✓";
                        return "ℹ";
                    }
                    font.pixelSize: 14
                }

                Text {
                    text: root.galleryModel.noticeMessage
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    color: root.colTextPrimary
                    wrapMode: Text.WordWrap
                    Layout.maximumWidth: root.width - 120
                }

                Rectangle {
                    implicitWidth: 20
                    implicitHeight: 20
                    radius: 10
                    color: Qt.rgba(255, 255, 255, 0.1)

                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        font.pixelSize: 10
                        color: root.colTextMuted
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.galleryModel.clearNotice()
                    }
                }
            }
        }

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
                        onDeleteRequested: (item) => {
                            root.galleryModel.requestDelete(item);
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
                                implicitWidth: 420
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
                                            { id: "desktophut", label: "DesktopHut" },
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

                        // Category chips for DesktopHut
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            visible: root.galleryModel.exploreSource === "desktophut"

                            Repeater {
                                model: [
                                    { id: "", label: "Trending" },
                                    { id: "anime", label: "Anime" },
                                    { id: "gaming", label: "Gaming" },
                                    { id: "nature", label: "Nature" },
                                    { id: "sci-fi", label: "Sci-Fi" },
                                    { id: "cars", label: "Cars" },
                                    { id: "cyberpunk", label: "Cyberpunk" },
                                    { id: "fantasy", label: "Fantasy" }
                                ]

                                Rectangle {
                                    required property var modelData
                                    required property int index
                                    implicitWidth: catChipText.implicitWidth + 14
                                    implicitHeight: 22
                                    radius: 11
                                    readonly property bool isSelected: (root.galleryModel.exploreQuery === modelData.id) || (modelData.id === "" && root.galleryModel.exploreQuery === "")
                                    color: isSelected ? Qt.rgba(56/255, 189/255, 248/255, 0.25) : root.colSurface
                                    border.color: isSelected ? root.colPacingCyan : root.colBorderSubtle
                                    border.width: 1

                                    Text {
                                        id: catChipText
                                        anchors.centerIn: parent
                                        text: parent.modelData.label
                                        font.pixelSize: 10
                                        font.weight: parent.isSelected ? Font.Bold : Font.Normal
                                        color: parent.isSelected ? root.colPacingCyan : root.colTextMuted
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            root.galleryModel.exploreQuery = parent.modelData.id;
                                            root.galleryModel.refreshExplore();
                                        }
                                    }
                                }
                            }
                        }

                        // Explore Grid View
                        Flickable {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            contentWidth: width
                            contentHeight: exploreCol.implicitHeight + 40
                            boundsBehavior: Flickable.StopAtBounds
                            clip: true

                            ColumnLayout {
                                id: exploreCol
                                width: parent.width
                                spacing: 14

                                Flow {
                                    id: exploreFlow
                                    Layout.fillWidth: true
                                    spacing: 12

                                    Repeater {
                                        model: root.galleryModel.exploreItems

                                        Rectangle {
                                            id: exploreCard
                                            required property var modelData
                                            required property int index

                                            readonly property string installedId: root.galleryModel.installedLocalId(exploreCard.modelData.ref)
                                            readonly property bool isInstalled: installedId !== ""
                                            readonly property bool isDownloading: root.galleryModel.downloadingRef === exploreCard.modelData.ref

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
                                                    GradientStop { position: 0.3; color: "transparent" }
                                                    GradientStop { position: 1.0; color: Qt.rgba(17/255, 19/255, 23/255, 0.92) }
                                                }
                                            }

                                            // Top badges row: Resolution & Installed status
                                            RowLayout {
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.top: parent.top
                                                anchors.margins: 6

                                                // Resolution badge (4K, HD)
                                                Rectangle {
                                                    visible: Boolean(exploreCard.modelData.resolution)
                                                    implicitWidth: resText.implicitWidth + 8
                                                    implicitHeight: 18
                                                    radius: 4
                                                    color: exploreCard.modelData.resolution === "4K" ? Qt.rgba(245/255, 158/255, 11/255, 0.3) : Qt.rgba(56/255, 189/255, 248/255, 0.3)
                                                    border.color: exploreCard.modelData.resolution === "4K" ? root.colTelemetryAmber : root.colPacingCyan
                                                    border.width: 1

                                                    Text {
                                                        id: resText
                                                        anchors.centerIn: parent
                                                        text: exploreCard.modelData.resolution || ""
                                                        font.pixelSize: 9
                                                        font.family: "JetBrains Mono, monospace"
                                                        font.weight: Font.Bold
                                                        color: exploreCard.modelData.resolution === "4K" ? root.colTelemetryAmber : root.colPacingCyan
                                                    }
                                                }

                                                Item { Layout.fillWidth: true }

                                                // Installed badge
                                                Rectangle {
                                                    visible: exploreCard.isInstalled
                                                    implicitWidth: 62
                                                    implicitHeight: 18
                                                    radius: 4
                                                    color: root.colSuccess

                                                    Text {
                                                        anchors.centerIn: parent
                                                        text: "INSTALLED"
                                                        font.pixelSize: 8
                                                        font.family: "JetBrains Mono, monospace"
                                                        font.weight: Font.Bold
                                                        color: "#111317"
                                                    }
                                                }
                                            }

                                            // Title, slug & install button
                                            ColumnLayout {
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.bottom: parent.bottom
                                                anchors.margins: 8
                                                spacing: 4

                                                Text {
                                                    text: exploreCard.modelData.title || exploreCard.modelData.slug || exploreCard.modelData.ref || ""
                                                    font.pixelSize: 11
                                                    font.weight: Font.Bold
                                                    color: root.colTextPrimary
                                                    elide: Text.ElideRight
                                                    Layout.fillWidth: true
                                                }

                                                Text {
                                                    visible: Boolean(exploreCard.modelData.slug)
                                                    text: exploreCard.modelData.slug || ""
                                                    font.pixelSize: 9
                                                    font.family: "JetBrains Mono, monospace"
                                                    color: root.colTextDim
                                                    elide: Text.ElideRight
                                                    Layout.fillWidth: true
                                                }

                                                Rectangle {
                                                    Layout.fillWidth: true
                                                    implicitHeight: 22
                                                    radius: 4
                                                    color: {
                                                        if (exploreCard.isDownloading) return Qt.rgba(245/255, 158/255, 11/255, 0.25);
                                                        if (exploreCard.isInstalled) return Qt.rgba(16/255, 185/255, 129/255, 0.25);
                                                        return installBtnHover.containsMouse ? root.colPacingCyan : Qt.rgba(56/255, 189/255, 248/255, 0.2);
                                                    }
                                                    border.color: {
                                                        if (exploreCard.isDownloading) return root.colTelemetryAmber;
                                                        if (exploreCard.isInstalled) return root.colSuccess;
                                                        return root.colPacingCyan;
                                                    }
                                                    border.width: 1

                                                    RowLayout {
                                                        anchors.centerIn: parent
                                                        spacing: 4

                                                        Text {
                                                            text: {
                                                                if (exploreCard.isDownloading) return "⏳ Downloading...";
                                                                if (exploreCard.isInstalled) return "▶ Apply";
                                                                return "⬇ Download / Install";
                                                            }
                                                            font.pixelSize: 10
                                                            font.weight: Font.Bold
                                                            color: {
                                                                if (exploreCard.isDownloading) return root.colTelemetryAmber;
                                                                if (exploreCard.isInstalled) return root.colSuccess;
                                                                return installBtnHover.containsMouse ? "#111317" : root.colPacingCyan;
                                                            }
                                                        }
                                                    }

                                                    MouseArea {
                                                        id: installBtnHover
                                                        anchors.fill: parent
                                                        hoverEnabled: true
                                                        cursorShape: Qt.PointingHandCursor
                                                        enabled: !exploreCard.isDownloading
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

                                // Load More Button
                                Rectangle {
                                    Layout.alignment: Qt.AlignHCenter
                                    implicitWidth: 140
                                    implicitHeight: 30
                                    radius: 5
                                    color: loadMoreArea.containsMouse ? root.colSurfaceRaised : root.colSurface
                                    border.color: root.colPacingCyan
                                    border.width: 1
                                    visible: root.galleryModel.exploreItems.length > 0 && !root.galleryModel.exploreLoading

                                    RowLayout {
                                        anchors.centerIn: parent
                                        spacing: 6
                                        Text { text: "⬇"; font.pixelSize: 11; color: root.colPacingCyan }
                                        Text {
                                            text: "Load More (p. " + (root.galleryModel.explorePage + 1) + ")"
                                            font.pixelSize: 11
                                            font.weight: Font.DemiBold
                                            color: root.colPacingCyan
                                        }
                                    }

                                    MouseArea {
                                        id: loadMoreArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.galleryModel.exploreMore()
                                    }
                                }

                                // Searching indicator
                                Rectangle {
                                    Layout.alignment: Qt.AlignHCenter
                                    implicitWidth: 160
                                    implicitHeight: 30
                                    radius: 5
                                    color: root.colSurface
                                    border.color: root.colBorderSubtle
                                    border.width: 1
                                    visible: root.galleryModel.exploreLoading

                                    RowLayout {
                                        anchors.centerIn: parent
                                        spacing: 6
                                        Text { text: "⏳"; font.pixelSize: 12 }
                                        Text {
                                            text: "Searching online..."
                                            font.pixelSize: 11
                                            color: root.colTextMuted
                                        }
                                    }
                                }

                                // Error indicator with retry
                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: 40
                                    radius: 6
                                    color: Qt.rgba(239/255, 68/255, 68/255, 0.1)
                                    border.color: Qt.rgba(239/255, 68/255, 68/255, 0.3)
                                    border.width: 1
                                    visible: root.galleryModel.exploreError !== "" && !root.galleryModel.exploreLoading

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        spacing: 8

                                        Text {
                                            text: "⚠️ " + root.galleryModel.exploreError
                                            font.pixelSize: 11
                                            color: root.colDanger
                                            Layout.fillWidth: true
                                        }

                                        Rectangle {
                                            implicitWidth: 64
                                            implicitHeight: 24
                                            radius: 4
                                            color: root.colSurfaceRaised
                                            border.color: root.colBorderSubtle
                                            border.width: 1

                                            Text {
                                                anchors.centerIn: parent
                                                text: "Retry"
                                                font.pixelSize: 10
                                                color: root.colTextPrimary
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: root.galleryModel.explore()
                                            }
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
