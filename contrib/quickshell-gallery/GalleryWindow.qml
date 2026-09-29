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

                    onTextChanged: {
                        if (root.galleryModel.mode === "explore") {
                            root.galleryModel.exploreQuery = text;
                        } else {
                            root.galleryModel.query = text;
                        }
                    }

                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Escape) {
                            root.galleryModel.close();
                            event.accepted = true;
                        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            if (root.galleryModel.mode === "explore") {
                                root.galleryModel.explore();
                            } else {
                                const items = root.galleryModel.filteredItems();

                                if (items.length > 0) {
                                    root.galleryModel.apply(items[0]);
                                }
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

                ShellButton {
                    label: "Local"
                    primary: root.galleryModel.mode === "local"
                    onActivated: root.galleryModel.setMode("local")
                }

                ShellButton {
                    label: "Explorar"
                    primary: root.galleryModel.mode === "explore"
                    onActivated: {
                        root.galleryModel.setMode("explore");
                        root.galleryModel.explore();
                    }
                }

                Repeater {
                    model: [
                        { id: "bing", label: "bing" },
                        { id: "wallhaven", label: "wallhaven" }
                    ]

                    delegate: ShellButton {
                        required property var modelData
                        visible: root.galleryModel.mode === "explore"
                        label: modelData.label
                        primary: root.galleryModel.exploreSource === modelData.id
                        onActivated: {
                            root.galleryModel.exploreSource = modelData.id;
                            root.galleryModel.explore();
                        }
                    }
                }

                ShellButton {
                    visible: root.galleryModel.mode === "explore"
                    enabled: root.galleryModel.exploreSelected !== null
                    label: "⬇ Instalar"
                    primary: true
                    onActivated: root.galleryModel.installSelected()
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
                        { id: "image", label: "image" },
                        { id: "web", label: "web" }
                    ]

                    delegate: ShellButton {
                        required property var modelData
                        label: modelData.label
                        primary: root.galleryModel.typeFilter === modelData.id
                        onActivated: root.galleryModel.setTypeFilter(modelData.id)
                    }
                }

                ShellButton {
                    label: "★ favs"
                    primary: root.galleryModel.showFavorites
                    onActivated: root.galleryModel.toggleFavorites()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: root.galleryModel.categories.length > 0

                UiText {
                    text: "mood:"
                }

                Repeater {
                    model: root.galleryModel.categories

                    delegate: ShellButton {
                        required property string modelData
                        label: modelData
                        primary: root.galleryModel.categoryFilter === modelData
                        onActivated: root.galleryModel.setCategoryFilter(modelData)
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
                model: root.galleryModel.mode === "explore" ? root.galleryModel.filteredExplore() : root.galleryModel.filteredItems()

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
                            source: {
                                const t = modelData.thumb || "";
                                if (t === "") {
                                    return "";
                                }
                                return (t.indexOf("http://") === 0 || t.indexOf("https://") === 0) ? t : "file://" + t;
                            }
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
                            onClicked: {
                                if (root.galleryModel.mode === "explore") {
                                    root.galleryModel.selectExplore(modelData);
                                } else {
                                    root.galleryModel.apply(modelData);
                                }
                            }
                        }

                        UiText {
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.margins: 6
                            visible: root.galleryModel.mode === "local" && (modelData.type || "") !== "image"
                            text: modelData.favorite ? "★" : "☆"
                            color: modelData.favorite ? Theme.accent : Theme.menuMutedText
                            font.pixelSize: 20

                            MouseArea {
                                anchors.fill: parent
                                anchors.margins: -8
                                cursorShape: Qt.PointingHandCursor
                                onClicked: mouse => {
                                    mouse.accepted = true;
                                    root.galleryModel.toggleFavorite(modelData);
                                }
                            }
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

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
                visible: root.galleryModel.mode === "local" && root.galleryModel.selectedId !== ""
                color: Theme.controlNormalFill
                radius: 8

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                        UiText {
                            text: "Properties: " + root.galleryModel.selectedId
                            font.bold: true
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        ShellButton {
                            label: "Save"
                            onActivated: root.galleryModel.saveProps()
                        }

                        ShellButton {
                            label: "Apply"
                            primary: true
                            onActivated: root.galleryModel.applySelected()
                        }
                    }

                    Flickable {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentHeight: propsColumn.implicitHeight
                        clip: true

                        Column {
                            id: propsColumn
                            width: parent.width
                            spacing: 4

                            Repeater {
                                model: root.galleryModel.props

                                delegate: RowLayout {
                                    required property var modelData
                                    width: propsColumn.width
                                    spacing: 8

                                    UiText {
                                        Layout.preferredWidth: 220
                                        elide: Text.ElideRight
                                        text: (modelData.text || modelData.name) + (modelData.saved ? " *" : "")
                                    }

                                    PanelToggleSwitch {
                                        visible: modelData.type === "boolean" || modelData.type === "bool"
                                        accessibleName: modelData.text || modelData.name
                                        checked: {
                                            const p = root.galleryModel.pendingEdits[modelData.name];
                                            const v = (p !== undefined) ? p : modelData.value;
                                            return v === "1" || v === "true";
                                        }
                                        onToggled: root.galleryModel.editProp(modelData.name, checked ? "0" : "1")
                                    }

                                    TextInput {
                                        visible: !(modelData.type === "boolean" || modelData.type === "bool")
                                        Layout.fillWidth: true
                                        color: Theme.controlFocusText
                                        selectionColor: Theme.accent
                                        selectedTextColor: Theme.accentText
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.inputFontSize
                                        text: modelData.value || ""
                                        onTextChanged: root.galleryModel.editProp(modelData.name, text)
                                    }
                                }
                            }

                            UiText {
                                visible: root.galleryModel.props.length === 0
                                text: {
                                    const sel = root.galleryModel.selectedItem();
                                    if (sel && (sel.type || "") === "image") {
                                        return "Imagen estática: se aplica con feh (sin motor)";
                                    }
                                    return "No properties (video) or failed to read";
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
