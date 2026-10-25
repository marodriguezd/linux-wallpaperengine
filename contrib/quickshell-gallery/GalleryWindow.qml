import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Widgets
import qs.core

pragma ComponentBehavior: Bound

FloatingWindow {
    id: root

    required property var galleryModel

    title: "linux-wallpaperengine"
    visible: galleryModel.visible
    implicitWidth: 880
    implicitHeight: 640
    color: Theme.transparent

    // Deliberate design tokens dynamically derived from galleryModel.toolkitTheme
    readonly property color colCanvas: {
        switch (root.galleryModel.toolkitTheme) {
        case "gtk": return "#1e1e1e";
        case "qt": return "#232629";
        case "brave": return "#0d1117";
        default: return (Theme.bg ? Theme.bg : "#2E3440");
        }
    }
    readonly property color colSurface: {
        switch (root.galleryModel.toolkitTheme) {
        case "gtk": return "#242424";
        case "qt": return "#31363b";
        case "brave": return "#161c24";
        default: return (Theme.surface ? Theme.surface : "#434C5E");
        }
    }
    readonly property color colSurfaceHover: {
        switch (root.galleryModel.toolkitTheme) {
        case "gtk": return "#2e2e2e";
        case "qt": return "#3a4046";
        case "brave": return "#1c2430";
        default: return (Theme.surfaceHover ? Theme.surfaceHover : "#4C566A");
        }
    }
    readonly property color colBorderSubtle: {
        switch (root.galleryModel.toolkitTheme) {
        case "gtk": return "#303030";
        case "qt": return "#43494f";
        case "brave": return "#232d3b";
        default: return (Theme.border ? Theme.border : "#3B4252");
        }
    }
    readonly property color colTextPrimary: {
        switch (root.galleryModel.toolkitTheme) {
        case "gtk": return "#ffffff";
        case "qt": return "#eff0f1";
        case "brave": return "#e6edf3";
        default: return (Theme.textStrong ? Theme.textStrong : (Theme.text ? Theme.text : "#ECEFF4"));
        }
    }
    readonly property color colTextMuted: {
        switch (root.galleryModel.toolkitTheme) {
        case "gtk": return "#9a9996";
        case "qt": return "#bdc3c7";
        case "brave": return "#8b9bb0";
        default: return (Theme.textMuted ? Theme.textMuted : "#D8DEE9");
        }
    }
    readonly property color colAccentCyan: {
        switch (root.galleryModel.toolkitTheme) {
        case "gtk": return "#3584e4";
        case "qt": return "#3daee9";
        case "brave": return "#fb542b";
        default: return (Theme.accent ? Theme.accent : "#81A1C1");
        }
    }
    readonly property color colAccent: root.colAccentCyan
    readonly property color colAccentAmber: {
        switch (root.galleryModel.toolkitTheme) {
        case "gtk": return "#f5c211";
        case "qt": return "#f67400";
        case "brave": return "#f59e0b";
        default: return (Theme.warning ? Theme.warning : "#EBCB8B");
        }
    }
    readonly property int cardRadius: {
        switch (root.galleryModel.toolkitTheme) {
        case "gtk": return 10;
        case "qt": return 5;
        case "brave": return 8;
        default: return (Theme.controlRadius !== undefined && Theme.controlRadius > 0) ? Theme.controlRadius : 6;
        }
    }

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
        color: root.colCanvas
        border.color: root.colBorderSubtle
        border.width: 1
        radius: root.cardRadius

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10

            LargeSurfaceHeader {
                Layout.fillWidth: true
                eyebrow: ""
                title: "Catálogo de fondos"
                subtitle: {
                    if (root.galleryModel.mode === "explore") {
                        switch (root.galleryModel.exploreSource) {
                        case "bing":
                            return "Fondos diarios en 4K y archivo histórico de Bing";
                        case "wallhaven":
                            return "Fondos destacados en ultra alta definición de Wallhaven";
                        case "motionbgs":
                            return "Vídeos animados y escenas en alta resolución de MotionBGS";
                        default:
                            return "Ilustraciones minimalistas y arte plano";
                        }
                    }
                    return "Escribe para filtrar, Enter para aplicar el primero o Esc para cerrar";
                }
                status: root.galleryModel.status
                statusColor: root.colAccentCyan
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.largeSurfaceSearchHeight > 0 ? Theme.largeSurfaceSearchHeight : 38
                color: gallerySearch.activeFocus ? root.colSurfaceHover : root.colSurface
                border.color: gallerySearch.activeFocus ? root.colAccentCyan : root.colBorderSubtle
                border.width: gallerySearch.activeFocus ? 2 : 1
                radius: Math.max(3, root.cardRadius - 2)

                TextInput {
                    id: gallerySearch

                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    verticalAlignment: TextInput.AlignVCenter
                    color: root.colTextPrimary
                    selectionColor: root.colAccentCyan
                    selectedTextColor: root.colCanvas
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
                    text: root.galleryModel.mode === "explore" ? "Buscar fondos en el catálogo en línea..." : "Buscar fondos por título, autor o temática..."
                    color: root.colTextMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.inputFontSize
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                ShellButton {
                    label: "Biblioteca local"
                    primary: root.galleryModel.mode === "local"
                    onActivated: root.galleryModel.setMode("local")
                }

                ShellButton {
                    label: "Explorar en línea"
                    primary: root.galleryModel.mode === "explore"
                    onActivated: {
                        root.galleryModel.setMode("explore");
                        root.galleryModel.searchExplore();
                    }
                }

                Repeater {
                    model: [
                        { id: "bing", label: "Bing" },
                        { id: "wallhaven", label: "Wallhaven" },
                        { id: "motionbgs", label: "MotionBGS" },
                        { id: "desktophut", label: "DesktopHut" },
                        { id: "minimal", label: "Minimal" }
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
                    label: "Actualizar"
                    icon: "view-refresh-symbolic"
                    onActivated: root.galleryModel.searchExplore()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: root.galleryModel.mode === "local"

                Repeater {
                    model: [
                        { id: "", label: "Todos" },
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
                    label: "Favoritos (" + root.galleryModel.typeCount("fav") + ")"
                    icon: "emblem-favorite-symbolic"
                    primary: root.galleryModel.showFavorites
                    onActivated: root.galleryModel.toggleFavorites()
                }

                ShellButton {
                    label: root.galleryModel.sortOrder === "" ? "Ordenar" : (root.galleryModel.sortOrder === "az" ? "Nombre A-Z" : "Nombre Z-A")
                    primary: root.galleryModel.sortOrder !== ""
                    onActivated: root.galleryModel.toggleSort()
                }

                ShellButton {
                    label: root.galleryModel.showPower ? "Ocultar energía" : "Perfil de energía"
                    primary: root.galleryModel.showPower
                    onActivated: root.galleryModel.showPower = !root.galleryModel.showPower
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: root.galleryModel.mode === "local" && root.galleryModel.activeCategories().length > 0

                UiText {
                    text: "Temática:"
                    color: root.colTextMuted
                }

                Repeater {
                    model: root.galleryModel.activeCategories()

                    delegate: ShellButton {
                        required property string modelData
                        label: modelData + " (" + root.galleryModel.categoryCount(modelData) + ")"
                        primary: root.galleryModel.categoryFilter === modelData
                        onActivated: root.galleryModel.setCategoryFilter(modelData)
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: powerCol.implicitHeight + 20
                visible: root.galleryModel.mode === "local" && root.galleryModel.powerLoaded && root.galleryModel.showPower
                color: root.colSurface
                border.color: root.colBorderSubtle
                border.width: 1
                radius: root.cardRadius

                ColumnLayout {
                    id: powerCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 10
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: root.colAccentAmber
                        }

                        UiText {
                            text: "Perfil de energía y rendimiento"
                            font.bold: true
                            color: root.colTextPrimary
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        ShellButton {
                            label: "Aplicar cambios"
                            primary: true
                            onActivated: root.galleryModel.applyPower()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        UiText {
                            Layout.preferredWidth: 120
                            text: "Perfil activo"
                            color: root.colTextMuted
                        }

                        Repeater {
                            model: [
                                { id: "lite", label: "Bajo consumo" },
                                { id: "balanced", label: "Equilibrado" },
                                { id: "full", label: "Rendimiento" }
                            ]

                            delegate: ShellButton {
                                required property var modelData
                                label: modelData.label
                                primary: root.galleryModel.powerProfile === modelData.id
                                onActivated: root.galleryModel.setPowerProfile(modelData.id)
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        UiText {
                            Layout.preferredWidth: 120
                            text: "Alimentación CA"
                            color: root.colTextMuted
                        }

                        ShellButton {
                            label: "-"
                            onActivated: root.galleryModel.stepAcFps(-1)
                        }

                        UiText {
                            Layout.preferredWidth: 54
                            text: root.galleryModel.acFpsLabel()
                            color: root.colTextPrimary
                            font.bold: true
                        }

                        ShellButton {
                            label: "+"
                            onActivated: root.galleryModel.stepAcFps(1)
                        }

                        UiText {
                            Layout.preferredWidth: 90
                            text: "Batería"
                            color: root.colAccentAmber
                        }

                        ShellButton {
                            label: "-"
                            onActivated: root.galleryModel.stepBattFps(-1)
                        }

                        UiText {
                            Layout.preferredWidth: 44
                            text: String(root.galleryModel.powerBattFps) + " fps"
                            color: root.colAccentAmber
                            font.bold: true
                        }

                        ShellButton {
                            label: "+"
                            onActivated: root.galleryModel.stepBattFps(1)
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        ShellButton {
                            label: root.galleryModel.powerPauseOnBatt ? "Pausar en batería: Sí" : "Pausar en batería: No"
                            primary: root.galleryModel.powerPauseOnBatt
                            onActivated: root.galleryModel.powerPauseOnBatt = !root.galleryModel.powerPauseOnBatt
                        }

                        UiText {
                            text: "Inactividad X11:"
                            color: root.colTextMuted
                        }

                        ShellButton {
                            label: "-"
                            onActivated: root.galleryModel.stepIdleMin(-1)
                        }

                        UiText {
                            Layout.preferredWidth: 64
                            text: root.galleryModel.idleMinLabel()
                            color: root.colTextPrimary
                        }

                        ShellButton {
                            label: "+"
                            onActivated: root.galleryModel.stepIdleMin(1)
                        }
                    }

                    UiText {
                        Layout.fillWidth: true
                        color: root.colTextMuted
                        text: "Aplica y guarda los límites de tasa de cuadros y ahorro de energía. Las variables de entorno tienen prioridad."
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: backendCol.implicitHeight + 20
                visible: root.galleryModel.mode === "local" && root.galleryModel.powerLoaded && root.galleryModel.showPower
                color: root.colSurface
                border.color: root.colBorderSubtle
                border.width: 1
                radius: root.cardRadius

                ColumnLayout {
                    id: backendCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 10
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        UiText {
                            text: "Servidor gráfico y aceleración"
                            font.bold: true
                            color: root.colTextPrimary
                        }

                        UiText {
                            color: root.colTextMuted
                            text: "detectado: " + (root.galleryModel.backendDetected || "desconocido")
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        ShellButton {
                            label: "Aplicar ajustes"
                            primary: true
                            onActivated: root.galleryModel.applyPower()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        UiText {
                            Layout.preferredWidth: 120
                            text: "Tema de toolkit"
                            color: root.colTextMuted
                        }

                        Repeater {
                            model: [
                                { id: "auto", label: "Auto / Sistema" },
                                { id: "gtk", label: "GTK / Libadwaita" },
                                { id: "qt", label: "Qt / Breeze" },
                                { id: "brave", label: "Brave / Obsidian" }
                            ]

                            delegate: ShellButton {
                                required property var modelData
                                label: modelData.label
                                primary: root.galleryModel.toolkitTheme === modelData.id
                                onActivated: root.galleryModel.toolkitTheme = modelData.id
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        UiText {
                            Layout.preferredWidth: 120
                            text: "Aceleración GPU"
                            color: root.colTextMuted
                        }

                        Repeater {
                            model: [
                                { id: "intel", label: "Intel iGPU (Batería)" },
                                { id: "nvidia", label: "dGPU Rendimiento" },
                                { id: "auto", label: "Auto" }
                            ]

                            delegate: ShellButton {
                                required property var modelData
                                label: modelData.label
                                primary: root.galleryModel.gpuTarget === modelData.id
                                onActivated: root.galleryModel.gpuTarget = modelData.id
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        UiText {
                            Layout.preferredWidth: 120
                            text: "Forzar servidor"
                            color: root.colTextMuted
                        }

                        ShellButton {
                            label: root.galleryModel.backendOverrideLabel()
                            primary: root.galleryModel.powerBackendOverride !== ""
                            onActivated: root.galleryModel.cycleBackendOverride()
                        }

                        UiText {
                            color: root.colTextMuted
                            text: "efectivo: " + (root.galleryModel.effBackend() || "desconocido")
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.galleryModel.effBackend() !== "sway"

                        UiText {
                            Layout.preferredWidth: 120
                            text: "Modo X11"
                            color: root.colTextMuted
                        }

                        ShellButton {
                            label: root.galleryModel.powerFullscreenPause ? "Pausa en pantalla completa: Sí" : "Pausa en pantalla completa: No"
                            primary: root.galleryModel.powerFullscreenPause
                            onActivated: root.galleryModel.powerFullscreenPause = !root.galleryModel.powerFullscreenPause
                        }

                        UiText {
                            text: "Decodificación:"
                            color: root.colTextMuted
                        }

                        ShellButton {
                            label: "-"
                            onActivated: root.galleryModel.stepHwdec(-1)
                        }

                        UiText {
                            Layout.preferredWidth: 92
                            text: root.galleryModel.powerHwdec
                            color: root.colTextPrimary
                        }

                        ShellButton {
                            label: "+"
                            onActivated: root.galleryModel.stepHwdec(1)
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.galleryModel.effBackend() === "sway"

                        UiText {
                            Layout.preferredWidth: 120
                            text: "Capa Wayland"
                            color: root.colTextMuted
                        }

                        Repeater {
                            model: root.galleryModel.layerSteps

                            delegate: ShellButton {
                                required property string modelData
                                label: modelData
                                primary: root.galleryModel.powerLayer === modelData
                                onActivated: root.galleryModel.powerLayer = modelData
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.galleryModel.effBackend() === "sway"

                        ShellButton {
                            label: "Pausar solo ventana activa"
                            primary: root.galleryModel.powerPauseActiveOnly
                            onActivated: root.galleryModel.powerPauseActiveOnly = !root.galleryModel.powerPauseActiveOnly
                        }

                        UiText {
                            text: "Ignorar app:"
                            color: root.colTextMuted
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            color: root.colSurfaceHover
                            border.color: root.colBorderSubtle
                            radius: 4

                            TextInput {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                verticalAlignment: TextInput.AlignVCenter
                                color: root.colTextPrimary
                                selectionColor: root.colAccentCyan
                                selectedTextColor: root.colCanvas
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.inputFontSize
                                clip: true
                                text: root.galleryModel.powerIgnoreAppid
                                onTextChanged: root.galleryModel.powerIgnoreAppid = text
                                Keys.onPressed: function(event) {
                                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                        root.galleryModel.applyPower();
                                        event.accepted = true;
                                    }
                                }
                            }

                            UiText {
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                visible: root.galleryModel.powerIgnoreAppid === ""
                                text: "firefox, steam..."
                                color: root.colTextMuted
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.galleryModel.effBackend() === "sway"

                        UiText {
                            text: "Decodificación:"
                            color: root.colTextMuted
                        }

                        ShellButton {
                            label: "-"
                            onActivated: root.galleryModel.stepHwdec(-1)
                        }

                        UiText {
                            Layout.preferredWidth: 92
                            text: root.galleryModel.powerHwdec
                            color: root.colTextPrimary
                        }

                        ShellButton {
                            label: "+"
                            onActivated: root.galleryModel.stepHwdec(1)
                        }
                    }

                    UiText {
                        Layout.fillWidth: true
                        color: root.colTextMuted
                        text: "En gráficos Intel integrados se recomienda vaapi-copy. Forzar sway en X11 impedirá aplicar fondos hasta corregirlo."
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: root.galleryModel.mode === "explore" && root.galleryModel.exploreSource === "motionbgs"
                Repeater {
                    model: [
                        { id: "", label: "Todos" },
                        { id: "tag:anime", label: "Anime" },
                        { id: "tag:nature", label: "Naturaleza" },
                        { id: "tag:gaming", label: "Videojuegos" },
                        { id: "tag:space", label: "Espacio" }
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
                    label: "4K UHD"
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
                        { id: "toplist", label: "Populares" },
                        { id: "hot", label: "Tendencias" },
                        { id: "random", label: "Aleatorio" }
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
                    label: "Recientes"
                    primary: root.galleryModel.exploreQuery !== "archive" && root.galleryModel.exploreQuery.indexOf("archive") !== 0
                    onActivated: {
                        root.galleryModel.exploreQuery = "";
                        root.galleryModel.exploreFilter = "";
                        root.galleryModel.refreshExplore();
                    }
                }

                ShellButton {
                    label: "Archivo histórico"
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
                cellWidth: 212
                cellHeight: root.galleryModel.mode === "explore" ? 212 : 180
                model: root.galleryModel.mode === "explore" ? root.galleryModel.filteredExplore() : root.galleryModel.filteredItems()

                delegate: Rectangle {
                    id: cardItem
                    required property var modelData

                    readonly property bool isCurrent: root.galleryModel.mode === "local" && root.galleryModel.currentId === modelData.id
                    readonly property bool isSelected: root.galleryModel.mode === "local" && root.galleryModel.selectedId === modelData.id

                    width: 202
                    height: cardColumn.implicitHeight + 8
                    radius: root.cardRadius
                    color: isCurrent ? Qt.rgba(root.colAccentCyan.r, root.colAccentCyan.g, root.colAccentCyan.b, 0.12) : (cardHover.hovered ? root.colSurfaceHover : root.colSurface)
                    border.color: isCurrent ? root.colAccentCyan : (isSelected ? root.colAccentCyan : (cardHover.hovered ? root.colAccentCyan : root.colBorderSubtle))
                    border.width: (isCurrent || isSelected) ? 2 : 1
                    clip: true

                    HoverHandler {
                        id: cardHover
                    }

                    Column {
                        id: cardColumn
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 4
                        spacing: 5

                        // 16:9 media-first preview container
                        Rectangle {
                            id: thumbBox
                            width: parent.width
                            height: Math.round(width * 9 / 16)
                            radius: Math.max(2, root.cardRadius - 3)
                            color: root.colCanvas
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
                                opacity: modelData.missing ? 0.35 : 1.0
                            }

                            UiText {
                                anchors.centerIn: parent
                                visible: (modelData.thumb || "") === ""
                                text: {
                                    const t = modelData.type || "";
                                    return t === "video" ? "Vídeo" : (t === "scene" ? "Escena" : (t === "image" ? "Imagen" : "Fondo"));
                                }
                                color: root.colTextMuted
                                font.pixelSize: 11
                            }

                            // Active badge
                            Rectangle {
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.margins: 6
                                visible: cardItem.isCurrent
                                color: root.colCanvas
                                border.color: root.colAccentCyan
                                border.width: 1
                                radius: 3
                                implicitWidth: activeLabel.implicitWidth + 8
                                implicitHeight: 18
                                z: 2

                                UiText {
                                    id: activeLabel
                                    anchors.centerIn: parent
                                    text: "Activo"
                                    color: root.colAccentCyan
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }

                            // Missing badge
                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.margins: 6
                                visible: root.galleryModel.mode === "local" && modelData.missing === true
                                color: root.colCanvas
                                border.color: root.colAccentAmber
                                border.width: 1
                                radius: 3
                                implicitWidth: missingLabel.implicitWidth + 8
                                implicitHeight: 18
                                z: 2

                                UiText {
                                    id: missingLabel
                                    anchors.centerIn: parent
                                    text: "No disponible"
                                    color: root.colAccentAmber
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }

                            // Media type tag
                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.right: parent.right
                                anchors.margins: 6
                                visible: (modelData.type || "") !== "" && !modelData.missing
                                color: Qt.rgba(13/255, 17/255, 23/255, 0.85)
                                radius: 3
                                implicitWidth: typeTagLabel.implicitWidth + 6
                                implicitHeight: 16
                                z: 2

                                UiText {
                                    id: typeTagLabel
                                    anchors.centerIn: parent
                                    text: {
                                        const t = modelData.type || "";
                                        return t === "video" ? "Vídeo" : (t === "image" ? "Imagen" : (t === "scene" ? "Escena" : t));
                                    }
                                    color: root.colTextPrimary
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                            }

                            // Favorite button
                            Rectangle {
                                anchors.top: parent.top
                                anchors.right: parent.right
                                anchors.margins: 6
                                width: 22
                                height: 22
                                radius: 11
                                color: Qt.rgba(13/255, 17/255, 23/255, 0.75)
                                visible: root.galleryModel.mode === "local" && (modelData.type || "") !== "image"
                                z: 3

                                IconImage {
                                    anchors.centerIn: parent
                                    width: 14
                                    height: 14
                                    source: Quickshell.iconPath(modelData.favorite ? "emblem-favorite-symbolic" : "non-starred-symbolic", true)
                                    opacity: modelData.favorite ? 1.0 : 0.65
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -4
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: mouse => {
                                        mouse.accepted = true;
                                        root.galleryModel.toggleFavorite(modelData);
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (modelData.missing) {
                                        root.galleryModel.status = (modelData.title || modelData.id) + " ya no está instalado";
                                        return;
                                    }
                                    if (root.galleryModel.mode === "explore") {
                                        root.galleryModel.selectExplore(modelData);
                                    } else {
                                        root.galleryModel.apply(modelData);
                                    }
                                }
                            }
                        }

                        // Title & technical info
                        Column {
                            width: parent.width
                            spacing: 2
                            leftPadding: 4
                            rightPadding: 4

                            UiText {
                                width: parent.width - 8
                                elide: Text.ElideRight
                                text: (modelData.title || modelData.id) + (modelData.missing ? " (no disponible)" : ((modelData.valid === false) ? " (inválido)" : ""))
                                color: root.colTextPrimary
                                font.bold: true
                                font.pixelSize: 12

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (modelData.missing) {
                                            root.galleryModel.status = (modelData.title || modelData.id) + " ya no está instalado";
                                            return;
                                        }
                                        if (root.galleryModel.mode === "explore") {
                                            root.galleryModel.selectExplore(modelData);
                                        } else {
                                            root.galleryModel.apply(modelData);
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                width: parent.width - 8
                                spacing: 6

                                UiText {
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    color: root.colTextMuted
                                    font.pixelSize: 11
                                    text: {
                                        if (root.galleryModel.mode === "explore") {
                                            return root.galleryModel.exploreSource;
                                        }
                                        return modelData.size_h ? modelData.size_h : "";
                                    }
                                }

                                UiText {
                                    visible: root.galleryModel.mode === "explore" && (modelData.kind || "") !== ""
                                    color: root.colTextMuted
                                    font.pixelSize: 10
                                    text: modelData.kind || ""
                                }
                            }
                        }

                        // Explore install button
                        ShellButton {
                            width: parent.width
                            visible: root.galleryModel.mode === "explore"
                            enabled: root.galleryModel.downloadingRef === ""
                            label: {
                                if (root.galleryModel.downloadingRef === modelData.ref) {
                                    return "Descargando...";
                                }
                                if (root.galleryModel.installedLocalId(modelData.ref) !== "") {
                                    return "Instalado";
                                }
                                return "Instalar fondo";
                            }
                            primary: root.galleryModel.installedLocalId(modelData.ref) !== ""
                            icon: root.galleryModel.installedLocalId(modelData.ref) !== "" ? "emblem-ok-symbolic" : "folder-download-symbolic"
                            onActivated: root.galleryModel.installOrApply(modelData)
                        }
                    }
                }

                UiText {
                    anchors.centerIn: parent
                    visible: root.galleryModel.mode === "local" && galleryGrid.count === 0
                    text: "No se encontraron fondos. Ejecuta: we-wallpaper refresh"
                    color: root.colTextMuted
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: root.galleryModel.mode === "explore" && root.galleryModel.exploreLoading

                UiText {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Consultando catálogo en línea..."
                    color: root.colTextMuted
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: root.galleryModel.mode === "explore" && !root.galleryModel.exploreLoading && root.galleryModel.exploreError !== ""

                UiText {
                    text: root.galleryModel.exploreError
                    color: root.colAccentAmber
                }

                ShellButton {
                    label: "Reintentar búsqueda"
                    primary: true
                    onActivated: root.galleryModel.searchExplore()
                }
            }

            UiText {
                Layout.alignment: Qt.AlignHCenter
                visible: root.galleryModel.mode === "explore" && !root.galleryModel.exploreLoading && root.galleryModel.exploreError === "" && root.galleryModel.exploreItems.length === 0
                text: "Selecciona una fuente y pulsa Enter para buscar"
                color: root.colTextMuted
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
                label: "Cargar más resultados"
                icon: "go-down-symbolic"
                onActivated: {
                    if (root.galleryModel.exploreSource === "bing") {
                        const q = root.galleryModel.exploreQuery;
                        if (q === "" || q === "archive" || q.indexOf("archive") === 0) {
                            const m = q.match(/archive(\d+)/);
                            const n = m ? parseInt(m[1], 10) + 1 : 2;
                            root.galleryModel.exploreQuery = "archive" + n;
                            root.galleryModel.exploreFilter = "";
                            root.galleryModel.explorePage = n;
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
                color: root.colSurface
                border.color: root.colBorderSubtle
                border.width: 1
                radius: root.cardRadius

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: root.colAccentCyan
                        }

                        UiText {
                            text: {
                                const sel = root.galleryModel.selectedItem();
                                if (sel && (sel.type || "") === "image") {
                                    return "Propiedades del fondo: " + (sel.title || root.galleryModel.selectedId) + " (imagen estática)";
                                }
                                return "Propiedades del fondo: " + (sel && sel.title ? sel.title : root.galleryModel.selectedId);
                            }
                            font.bold: true
                            color: root.colTextPrimary
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        ShellButton {
                            label: "Eliminar"
                            icon: "user-trash-symbolic"
                            danger: true
                            visible: root.galleryModel.isStoreItem(root.galleryModel.selectedId)
                            onActivated: {
                                const sel = root.galleryModel.selectedItem();

                                if (sel) {
                                    root.galleryModel.remove(sel);
                                }
                            }
                        }

                        ShellButton {
                            label: "Guardar perfil"
                            visible: root.galleryModel.props.length > 0
                            onActivated: root.galleryModel.saveProps()
                        }

                        ShellButton {
                            label: "Aplicar fondo"
                            primary: true
                            onActivated: root.galleryModel.applySelected()
                        }
                    }

                    UiText {
                        Layout.fillWidth: true
                        visible: root.galleryModel.props.length > 0
                        color: root.colTextMuted
                        text: "Guardar perfil conserva estos ajustes para futuras sesiones. Aplicar actualiza la escena activa de inmediato."
                    }

                    Flickable {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentHeight: propsColumn.implicitHeight
                        clip: true

                        Column {
                            id: propsColumn
                            width: parent.width
                            spacing: 6

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
                                        color: root.colTextPrimary
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

                                    Rectangle {
                                        visible: !(modelData.type === "boolean" || modelData.type === "bool")
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 28
                                        color: root.colSurfaceHover
                                        border.color: root.colBorderSubtle
                                        radius: 4

                                        TextInput {
                                            anchors.fill: parent
                                            anchors.leftMargin: 8
                                            anchors.rightMargin: 8
                                            verticalAlignment: TextInput.AlignVCenter
                                            color: root.colTextPrimary
                                            selectionColor: root.colAccentCyan
                                            selectedTextColor: root.colCanvas
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.inputFontSize
                                            text: modelData.value || ""
                                            onTextChanged: root.galleryModel.editProp(modelData.name, text)
                                        }
                                    }
                                }
                            }

                            UiText {
                                visible: root.galleryModel.props.length === 0
                                color: root.colTextMuted
                                text: {
                                    const sel = root.galleryModel.selectedItem();
                                    if (sel && (sel.type || "") === "image") {
                                        return "Fondo de imagen fija: aplicado mediante feh sin parámetros interactivos";
                                    }
                                    return "Este vídeo o escena utiliza los valores predeterminados del motor. Aplicar reinicia la reproducción.";
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
