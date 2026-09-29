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
                subtitle: {
                    if (root.galleryModel.mode === "explore") {
                        switch (root.galleryModel.exploreSource) {
                        case "bing":
                            return "Bing · dailies 4K + archivo";
                        case "wallhaven":
                            return "Wallhaven · top UHD sin key";
                        case "motionbgs":
                            return "MotionBGS · vídeos HD/4K";
                        default:
                            return "Minimalista · flat art";
                        }
                    }
                    return "Type to filter / Enter applies first / Esc closes";
                }
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
                    text: root.galleryModel.mode === "explore" ? root.galleryModel.exploreFilter : root.galleryModel.query
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.inputFontSize
                    clip: true

                    onTextChanged: {
                        if (root.galleryModel.mode === "explore") {
                            root.galleryModel.exploreFilter = text;
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
                                root.galleryModel.searchExplore();
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
                        root.galleryModel.searchExplore();
                    }
                }

                Repeater {
                    model: [
                        { id: "bing", label: "bing" },
                        { id: "wallhaven", label: "wallhaven" },
                        { id: "motionbgs", label: "motion" },
                        { id: "minimal", label: "minimal" }
                    ]

                    delegate: ShellButton {
                        required property var modelData
                        visible: root.galleryModel.mode === "explore"
                        label: modelData.label
                        primary: root.galleryModel.exploreSource === modelData.id
                        onActivated: root.galleryModel.setSource(modelData.id)
                    }
                }

                ShellButton {
                    visible: root.galleryModel.mode === "explore"
                    label: "⟳"
                    onActivated: root.galleryModel.searchExplore()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: root.galleryModel.mode === "local"

                Repeater {
                    model: [
                        { id: "", label: "Todo" },
                        { id: "video", label: "Vídeo" },
                        { id: "image", label: "Imagen" }
                    ]

                    delegate: ShellButton {
                        required property var modelData
                        label: modelData.label + " (" + root.galleryModel.typeCount(modelData.id) + ")"
                        primary: root.galleryModel.typeFilter === modelData.id
                        onActivated: root.galleryModel.setTypeFilter(modelData.id)
                    }
                }

                ShellButton {
                    label: "★ (" + root.galleryModel.typeCount("fav") + ")"
                    primary: root.galleryModel.showFavorites
                    onActivated: root.galleryModel.toggleFavorites()
                }

                ShellButton {
                    label: root.galleryModel.sortOrder === "" ? "A-Z" : (root.galleryModel.sortOrder === "az" ? "A-Z ↓" : "Z-A ↑")
                    primary: root.galleryModel.sortOrder !== ""
                    onActivated: root.galleryModel.toggleSort()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: root.galleryModel.mode === "local" && root.galleryModel.categories.length > 0

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

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: root.galleryModel.mode === "explore" && root.galleryModel.exploreSource === "motionbgs"

                Repeater {
                    model: [
                        { id: "", label: "todo" },
                        { id: "tag:anime", label: "anime" },
                        { id: "tag:nature", label: "nature" },
                        { id: "tag:gaming", label: "gaming" },
                        { id: "tag:space", label: "space" }
                    ]

                    delegate: ShellButton {
                        required property var modelData
                        label: modelData.label
                        primary: root.galleryModel.exploreQuery === modelData.id
                        onActivated: root.galleryModel.setMotionTag(modelData.id)
                    }
                }

                ShellButton {
                    label: "HD"
                    primary: root.galleryModel.exploreQuality === "hd"
                    onActivated: root.galleryModel.exploreQuality = "hd"
                }

                ShellButton {
                    label: "4K"
                    primary: root.galleryModel.exploreQuality === "4k"
                    onActivated: root.galleryModel.exploreQuality = "4k"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: root.galleryModel.mode === "explore" && root.galleryModel.exploreSource === "wallhaven"

                Repeater {
                    model: [
                        { id: "toplist", label: "top" },
                        { id: "hot", label: "hot" },
                        { id: "random", label: "random" }
                    ]

                    delegate: ShellButton {
                        required property var modelData
                        label: modelData.label
                        primary: root.galleryModel.wallhavenSort() === modelData.id
                        onActivated: root.galleryModel.setWallhavenSort(modelData.id)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: root.galleryModel.mode === "explore" && root.galleryModel.exploreSource === "bing"

                ShellButton {
                    label: "recientes"
                    primary: root.galleryModel.exploreQuery !== "archive" && root.galleryModel.exploreQuery.indexOf("archive") !== 0
                    onActivated: {
                        root.galleryModel.exploreQuery = "";
                        root.galleryModel.exploreFilter = "";
                        root.galleryModel.refreshExplore();
                    }
                }

                ShellButton {
                    label: "archivo"
                    primary: root.galleryModel.exploreQuery === "archive" || root.galleryModel.exploreQuery.indexOf("archive") === 0
                    onActivated: {
                        root.galleryModel.exploreQuery = "archive";
                        root.galleryModel.exploreFilter = "";
                        root.galleryModel.refreshExplore();
                    }
                }
            }

            GridView {
                id: galleryGrid

                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                cellWidth: 200
                cellHeight: 204
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
                            anchors.left: parent.left
                            anchors.margins: 6
                            visible: root.galleryModel.mode === "local" && root.galleryModel.currentId === modelData.id
                            text: "ACTIVA"
                            color: Theme.accent
                            font.bold: true
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

                    UiText {
                        width: 190
                        elide: Text.ElideRight
                        color: Theme.menuMutedText
                        text: {
                            if (root.galleryModel.mode === "explore") {
                                return (modelData.kind || "online") + " • " + root.galleryModel.exploreSource;
                            }
                            const t = modelData.type || "";
                            const tl = t === "video" ? "Vídeo" : (t === "image" ? "Imagen" : t);
                            const sz = modelData.size_h || "";
                            return sz !== "" ? tl + " • " + sz : tl;
                        }
                    }

                    ShellButton {
                        visible: root.galleryModel.mode === "explore"
                        enabled: root.galleryModel.downloadingRef === ""
                        label: {
                            if (root.galleryModel.downloadingRef === modelData.ref) {
                                return "Instalando…";
                            }
                            if (root.galleryModel.installedLocalId(modelData.ref) !== "") {
                                return "✓ Instalado (aplicar)";
                            }
                            return "⬇ Instalar";
                        }
                        primary: root.galleryModel.installedLocalId(modelData.ref) !== ""
                        onActivated: root.galleryModel.installOrApply(modelData)
                    }
                }

                UiText {
                    anchors.centerIn: parent
                    visible: root.galleryModel.mode === "local" && galleryGrid.count === 0
                    text: "No wallpapers: run we-wallpaper refresh + gallery-json"
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: root.galleryModel.mode === "explore" && root.galleryModel.exploreLoading

                UiText {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Buscando online…"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: root.galleryModel.mode === "explore" && !root.galleryModel.exploreLoading && root.galleryModel.exploreError !== ""

                UiText {
                    text: root.galleryModel.exploreError
                    color: Theme.accent
                }

                ShellButton {
                    label: "Reintentar"
                    primary: true
                    onActivated: root.galleryModel.searchExplore()
                }
            }

            UiText {
                Layout.alignment: Qt.AlignHCenter
                visible: root.galleryModel.mode === "explore" && !root.galleryModel.exploreLoading && root.galleryModel.exploreError === "" && root.galleryModel.exploreItems.length === 0
                text: "Elige fuente, escribe o pulsa ⟳"
                color: Theme.menuMutedText
            }

            ShellButton {
                Layout.alignment: Qt.AlignHCenter
                visible: {
                    if (root.galleryModel.mode !== "explore" || root.galleryModel.exploreLoading || root.galleryModel.exploreError !== "") {
                        return false;
                    }
                    if (root.galleryModel.exploreItems.length === 0) {
                        return false;
                    }
                    if (root.galleryModel.exploreSource === "bing") {
                        const q = root.galleryModel.exploreQuery;
                        return q === "" || q === "archive" || q.indexOf("archive") === 0;
                    }
                    if (root.galleryModel.exploreSource === "wallhaven" && root.galleryModel.wallhavenSort() === "random") {
                        return false;
                    }
                    return true;
                }
                label: "Cargar más ↓"
                onActivated: {
                    if (root.galleryModel.exploreSource === "bing") {
                        const q = root.galleryModel.exploreQuery;
                        if (q === "" || q === "archive" || q.indexOf("archive") === 0) {
                            const m = q.match(/archive(\d+)/);
                            const n = m ? parseInt(m[1], 10) + 1 : 2;
                            root.galleryModel.exploreQuery = "archive" + n;
                            root.galleryModel.exploreFilter = "";
                            root.galleryModel.explore();
                        }
                        return;
                    }
                    root.galleryModel.exploreMore();
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
