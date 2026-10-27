pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property var items: []
    property string currentId: ""
    property string selectedId: ""
    property color colSurfaceRaised: "#222730"
    property color colSurface: "#191C22"
    property color colBorderSubtle: "#2D3440"
    property color colPacingCyan: "#38BDF8"
    property color colTelemetryAmber: "#F59E0B"
    property color colTextPrimary: "#F1F5F9"
    property color colTextMuted: "#94A3B8"
    property color colTextDim: "#64748B"

    signal wallpaperSelected(var item)
    signal applyRequested(var item)
    signal favRequested(var item)
    signal inspectRequested(var item)
    signal deleteRequested(var item)
    signal resetFiltersRequested()

    readonly property int colCount: Math.max(2, Math.floor(width / 240))
    readonly property real spacing: 12
    readonly property real itemWidth: Math.floor((width - (spacing * (colCount - 1))) / colCount)
    readonly property real itemHeight: Math.round(itemWidth * 9 / 16)

    // Empty state container
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 12
        visible: root.items.length === 0

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "🔍"
            font.pixelSize: 36
            opacity: 0.6
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "No wallpapers found"
            font.pixelSize: 14
            font.weight: Font.DemiBold
            color: root.colTextMuted
        }

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: 120
            implicitHeight: 28
            radius: 4
            color: resetArea.containsMouse ? root.colPacingCyan : root.colSurfaceRaised
            border.color: root.colPacingCyan
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "Clear filters"
                font.pixelSize: 11
                color: resetArea.containsMouse ? "#111317" : root.colPacingCyan
            }

            MouseArea {
                id: resetArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.resetFiltersRequested()
            }
        }
    }

    // Scrollable Grid View
    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: flowGrid.implicitHeight + 24
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Flow {
            id: flowGrid
            width: root.width
            spacing: root.spacing

            Repeater {
                model: root.items

                WallpaperCard {
                    required property var modelData
                    required property int index

                    width: root.itemWidth
                    height: root.itemHeight
                    itemData: modelData
                    isCurrent: Boolean(modelData && modelData.id && String(modelData.id) === String(root.currentId))
                    isSelected: Boolean(modelData && modelData.id && String(modelData.id) === String(root.selectedId))

                    onCardClicked: (item) => root.wallpaperSelected(item)
                    onApplyRequested: (item) => root.applyRequested(item)
                    onFavRequested: (item) => root.favRequested(item)
                    onInspectRequested: (item) => root.inspectRequested(item)
                    onDeleteRequested: (item) => root.deleteRequested(item)
                }
            }
        }
    }
}
