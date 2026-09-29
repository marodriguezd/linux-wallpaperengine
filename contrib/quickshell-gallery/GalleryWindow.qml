import QtQuick
import QtQuick.Layouts
import Quickshell
import qs.core

pragma ComponentBehavior: Bound

FloatingWindow {
    id: root

    required property var galleryModel

    title: "wallpaper gallery"
    visible: galleryModel.visible
    implicitWidth: 880
    implicitHeight: 640
    color: Theme.transparent

    function focusSearch() {
        gallerySearch.forceActiveFocus();
        gallerySearch.cursorPosition = gallerySearch.text.length;
    }

    onVisibleChanged: {
        if (visible) {
            Qt.callLater(root.focusSearch);
        }
    }

    ShellSurface {
        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10

            LargeSurfaceHeader {
                Layout.fillWidth: true
                eyebrow: "Fork catalog"
                title: "Wallpapers"
                subtitle: "Type to filter / Enter applies first / Esc closes"
                status: root.galleryModel.status
                statusColor: Theme.accent
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.largeSurfaceSearchHeight
                color: gallerySearch.activeFocus ? Theme.controlFocusFill : Theme.controlNormalFill
                border.color: gallerySearch.activeFocus ? Theme.controlFocusBorder : Theme.controlNormalBorder
                border.width: gallerySearch.activeFocus ? Theme.controlFocusBorderWidth : Theme.controlBorderWidth
                radius: Theme.largeSurfaceCardRadius

                TextInput {
                    id: gallerySearch

                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    verticalAlignment: TextInput.AlignVCenter
                    color: Theme.controlFocusText
                    selectionColor: Theme.accent
                    selectedTextColor: Theme.accentText
                    text: root.galleryModel.query
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.inputFontSize
                    clip: true

                    onTextChanged: root.galleryModel.query = text

                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Escape) {
                            root.galleryModel.close();
                            event.accepted = true;
                        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            const items = root.galleryModel.filteredItems();

                            if (items.length > 0) {
                                root.galleryModel.apply(items[0]);
                            }

                            event.accepted = true;
                        }
                    }
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    visible: gallerySearch.text.length === 0
                    text: "Search wallpapers"
                    color: Theme.placeholder
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.inputFontSize
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: [
                        { id: "", label: "all" },
                        { id: "scene", label: "scene" },
                        { id: "video", label: "video" },
                        { id: "web", label: "web" }
                    ]

                    delegate: ShellButton {
                        required property var modelData
                        label: modelData.label
                        primary: root.galleryModel.typeFilter === modelData.id
                        onActivated: root.galleryModel.setTypeFilter(modelData.id)
                    }
                }
            }

            GridView {
                id: galleryGrid

                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                cellWidth: 200
                cellHeight: 160
                model: root.galleryModel.filteredItems()

                delegate: Column {
                    required property var modelData

                    width: 190
                    spacing: 4

                    Rectangle {
                        width: 190
                        height: 120
                        radius: 8
                        color: Theme.controlNormalFill
                        clip: true

                        Image {
                            anchors.fill: parent
                            source: (modelData.thumb || "") !== "" ? "file://" + modelData.thumb : ""
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: true
                            smooth: true
                        }

                        UiText {
                            anchors.centerIn: parent
                            visible: (modelData.thumb || "") === ""
                            text: modelData.type || "?"
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.galleryModel.apply(modelData)
                        }
                    }

                    UiText {
                        width: 190
                        elide: Text.ElideRight
                        text: (modelData.title || modelData.id) + ((modelData.valid === false) ? " (invalid)" : "")
                    }
                }

                UiText {
                    anchors.centerIn: parent
                    visible: galleryGrid.count === 0
                    text: "No wallpapers: run we-wallpaper refresh + gallery-json"
                }
            }
        }
    }
}
