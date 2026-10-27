pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    required property var itemData
    property bool isCurrent: false
    property bool isSelected: false
    property color colSurfaceRaised: "#222730"
    property color colSurface: "#191C22"
    property color colBorderSubtle: "#2D3440"
    property color colPacingCyan: "#38BDF8"
    property color colTelemetryAmber: "#F59E0B"
    property color colTextPrimary: "#F1F5F9"
    property color colTextMuted: "#94A3B8"
    property color colTextDim: "#64748B"
    property color colDanger: "#EF4444"
    property bool confirmDeleteState: false

    signal cardClicked(var item)
    signal applyRequested(var item)
    signal favRequested(var item)
    signal inspectRequested(var item)
    signal deleteRequested(var item)

    readonly property string itemTitle: (root.itemData && (root.itemData.title || root.itemData.id)) ? String(root.itemData.title || root.itemData.id) : "Wallpaper"
    readonly property string itemType: (root.itemData && root.itemData.type) ? String(root.itemData.type).toUpperCase() : "WALLPAPER"
    readonly property bool isFav: Boolean(root.itemData && root.itemData.favorite)
    readonly property bool isWPEngine: {
        if (!root.itemData) return false;
        if (root.itemData.source === "wallpaperengine") return true;
        if (root.itemData.source && root.itemData.source !== "wallpaperengine") return false;
        const id = String(root.itemData.id || "");
        return id.indexOf("desktophut-") !== 0 && id.indexOf("motionbgs-") !== 0 &&
               id.indexOf("bing-") !== 0 && id.indexOf("img-") !== 0 &&
               id.indexOf("fetch-") !== 0 && id.indexOf("import-") !== 0;
    }
    readonly property string thumbUrl: {
        if (!root.itemData) return "";
        if (root.itemData.thumb) {
            return root.itemData.thumb.indexOf("://") !== -1 ? root.itemData.thumb : ("file://" + root.itemData.thumb);
        }
        if (root.itemData.preview && root.itemData.path) {
            return "file://" + root.itemData.path + "/" + root.itemData.preview;
        }
        return "";
    }

    readonly property bool isHovered: (cardHoverHandler && cardHoverHandler.hovered) || cardHover.containsMouse || applyBtnArea.containsMouse || inspectArea.containsMouse || favArea.containsMouse || trashArea.containsMouse

    Timer {
        id: deleteConfirmTimer
        interval: 3000
        running: false
        repeat: false
        onTriggered: root.confirmDeleteState = false
    }

    HoverHandler {
        id: cardHoverHandler
    }

    implicitWidth: 260
    implicitHeight: Math.round(implicitWidth * 9 / 16)
    radius: 8
    color: root.colSurfaceRaised
    border.color: root.isCurrent ? root.colPacingCyan : (root.isSelected ? Qt.rgba(56/255, 189/255, 248/255, 0.5) : (root.isHovered ? root.colPacingCyan : root.colBorderSubtle))
    border.width: root.isCurrent ? 2 : (root.isSelected || root.isHovered ? 1.5 : 1)
    clip: true

    // Background thumbnail
    Image {
        id: previewImg
        anchors.fill: parent
        source: root.thumbUrl
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        smooth: true

        // Fallback placeholder if image is loading or missing
        Rectangle {
            anchors.fill: parent
            color: root.colSurfaceRaised
            visible: previewImg.status !== Image.Ready

            Text {
                anchors.centerIn: parent
                text: "🎬"
                font.pixelSize: 28
                opacity: 0.3
            }
        }
    }

    // Vignette gradient overlay for readable text
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(17/255, 19/255, 23/255, 0.3) }
            GradientStop { position: 0.5; color: "transparent" }
            GradientStop { position: 1.0; color: Qt.rgba(17/255, 19/255, 23/255, 0.92) }
        }
    }

    // Top badges row: Type badge on left, Favorite button on right
    RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8

        // Type badge (SCENE, VIDEO, 4K)
        Rectangle {
            implicitWidth: typeLabel.implicitWidth + 10
            implicitHeight: 18
            radius: 4
            color: root.itemType === "SCENE" ? Qt.rgba(56/255, 189/255, 248/255, 0.25) : Qt.rgba(245/255, 158/255, 11/255, 0.25)
            border.color: root.itemType === "SCENE" ? root.colPacingCyan : root.colTelemetryAmber
            border.width: 1

            Text {
                id: typeLabel
                anchors.centerIn: parent
                text: root.itemType
                font.pixelSize: 9
                font.family: "JetBrains Mono, monospace"
                font.weight: Font.Bold
                color: root.itemType === "SCENE" ? root.colPacingCyan : root.colTelemetryAmber
            }
        }

        Item { Layout.fillWidth: true }

        // Active State Badge (if current)
        Rectangle {
            visible: root.isCurrent
            implicitWidth: 54
            implicitHeight: 18
            radius: 4
            color: root.colPacingCyan

            Text {
                anchors.centerIn: parent
                text: "ACTIVE"
                font.pixelSize: 8
                font.family: "JetBrains Mono, monospace"
                font.weight: Font.Bold
                color: "#111317"
            }
        }

        // Favorite Toggle Button
        Rectangle {
            width: 22
            height: 22
            radius: 11
            color: root.isFav ? Qt.rgba(245/255, 158/255, 11/255, 0.25) : (favArea.containsMouse ? Qt.rgba(0,0,0,0.5) : Qt.rgba(0,0,0,0.3))
            border.color: root.isFav ? root.colTelemetryAmber : Qt.rgba(255,255,255,0.2)
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: root.isFav ? "★" : "☆"
                font.pixelSize: 12
                color: root.isFav ? root.colTelemetryAmber : "#FFFFFF"
            }

            MouseArea {
                id: favArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.favRequested(root.itemData)
            }
        }
    }

    // Bottom info & Microinteraction Overlay
    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 8
        spacing: 4

        Text {
            text: root.itemTitle
            font.pixelSize: 12
            font.weight: Font.Bold
            color: root.colTextPrimary
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        // Action Pills row on hover or selection
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: root.isHovered || root.isSelected || root.isCurrent

            // Quick Apply
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 22
                radius: 4
                color: applyBtnArea.containsMouse ? root.colPacingCyan : Qt.rgba(56/255, 189/255, 248/255, 0.2)
                border.color: root.colPacingCyan
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Apply"
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    color: applyBtnArea.containsMouse ? "#111317" : root.colPacingCyan
                }

                MouseArea {
                    id: applyBtnArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.applyRequested(root.itemData)
                }
            }

            // Inspect Properties
            Rectangle {
                width: 22
                height: 22
                radius: 4
                color: inspectArea.containsMouse ? root.colPacingCyan : Qt.rgba(0,0,0,0.5)
                border.color: root.colBorderSubtle
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "⚙"
                    font.pixelSize: 11
                    color: inspectArea.containsMouse ? "#111317" : root.colTextPrimary
                }

                MouseArea {
                    id: inspectArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.inspectRequested(root.itemData)
                }
            }

            // Delete / Protection Action Pill
            Rectangle {
                implicitWidth: root.confirmDeleteState ? 54 : 22
                implicitHeight: 22
                radius: 4
                color: {
                    if (root.isWPEngine) {
                        return trashArea.containsMouse ? Qt.rgba(245/255, 158/255, 11/255, 0.25) : Qt.rgba(0,0,0,0.5);
                    }
                    if (root.confirmDeleteState) {
                        return root.colDanger;
                    }
                    return trashArea.containsMouse ? Qt.rgba(239/255, 68/255, 68/255, 0.3) : Qt.rgba(0,0,0,0.5);
                }
                border.color: {
                    if (root.isWPEngine) {
                        return trashArea.containsMouse ? root.colTelemetryAmber : root.colBorderSubtle;
                    }
                    if (root.confirmDeleteState) {
                        return root.colDanger;
                    }
                    return trashArea.containsMouse ? root.colDanger : root.colBorderSubtle;
                }
                border.width: 1

                Behavior on implicitWidth { NumberAnimation { duration: 120 } }

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 2

                    Text {
                        text: root.isWPEngine ? "🔒" : "🗑"
                        font.pixelSize: 10
                        color: root.isWPEngine ? root.colTelemetryAmber : (root.confirmDeleteState ? "#FFFFFF" : root.colTextPrimary)
                    }

                    Text {
                        visible: root.confirmDeleteState && !root.isWPEngine
                        text: "Del?"
                        font.pixelSize: 9
                        font.weight: Font.Bold
                        color: "#FFFFFF"
                    }
                }

                MouseArea {
                    id: trashArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: root.isWPEngine ? Qt.ForbiddenCursor : Qt.PointingHandCursor
                    onClicked: {
                        if (root.isWPEngine) {
                            root.deleteRequested(root.itemData);
                            return;
                        }
                        if (!root.confirmDeleteState) {
                            root.confirmDeleteState = true;
                            deleteConfirmTimer.restart();
                        } else {
                            root.confirmDeleteState = false;
                            deleteConfirmTimer.stop();
                            root.deleteRequested(root.itemData);
                        }
                    }
                }
            }
        }
    }

    // Main Card MouseArea
    MouseArea {
        id: cardHover
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        z: -1
        onClicked: root.cardClicked(root.itemData)
    }
}
