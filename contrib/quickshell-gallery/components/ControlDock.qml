pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    required property var galleryModel
    property int activeTab: root.galleryModel ? root.galleryModel.controlDockTab : 0
    property bool isCollapsed: false

    Binding {
        target: root
        property: "activeTab"
        value: root.galleryModel ? root.galleryModel.controlDockTab : 0
    }

    onActiveTabChanged: {
        if (root.galleryModel && root.galleryModel.controlDockTab !== root.activeTab) {
            root.galleryModel.controlDockTab = root.activeTab;
        }
    }

    readonly property string selectedTitle: {
        if (!root.galleryModel || !root.galleryModel.selectedId) return "";
        if (root.galleryModel.selectedWallpaper && root.galleryModel.selectedWallpaper.title) {
            return String(root.galleryModel.selectedWallpaper.title);
        }
        const it = root.galleryModel.selectedItem();
        if (it && it.title) return String(it.title);
        return String(root.galleryModel.selectedId);
    }
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

    signal collapseToggled()

    implicitWidth: isCollapsed ? 36 : 310
    color: root.colSurface
    border.color: root.colBorderSubtle
    border.width: 1
    radius: 8
    clip: true

    Behavior on implicitWidth { NumberAnimation { duration: 150 } }

    // Collapsed vertical bar
    Item {
        anchors.fill: parent
        visible: root.isCollapsed

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 16

            Text {
                text: "⚙"
                font.pixelSize: 16
                color: root.colPacingCyan
            }

            Text {
                text: "D\nO\nC\nK"
                font.pixelSize: 10
                font.family: "JetBrains Mono, monospace"
                font.weight: Font.Bold
                color: root.colTextMuted
                horizontalAlignment: Text.AlignHCenter
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.collapseToggled()
        }
    }

    // Expanded dock contents
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10
        visible: !root.isCollapsed

        // Dock Header: Title & Collapse Button
        RowLayout {
            Layout.fillWidth: true

            Text {
                text: "CONTROL DOCK"
                font.pixelSize: 11
                font.family: "JetBrains Mono, monospace"
                font.weight: Font.Bold
                color: root.colTextMuted
                Layout.fillWidth: true
            }

            Rectangle {
                width: 22
                height: 22
                radius: 4
                color: collapseBtnArea.containsMouse ? root.colSurfaceRaised : "transparent"
                border.color: root.colBorderSubtle
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "⇥"
                    font.pixelSize: 12
                    color: root.colTextPrimary
                }

                MouseArea {
                    id: collapseBtnArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.collapseToggled()
                }
            }
        }

        // Dock Tabs Row
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 30
            radius: 5
            color: root.colBaseVoid
            border.color: root.colBorderSubtle
            border.width: 1

            RowLayout {
                anchors.fill: parent
                spacing: 2

                // Tab 0: Pacing
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 4
                    color: root.activeTab === 0 ? root.colSurfaceRaised : "transparent"
                    border.color: root.activeTab === 0 ? root.colPacingCyan : "transparent"
                    border.width: root.activeTab === 0 ? 1 : 0

                    Text {
                        anchors.centerIn: parent
                        text: "⚡ Pacing"
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        color: root.activeTab === 0 ? root.colPacingCyan : root.colTextMuted
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.galleryModel) root.galleryModel.controlDockTab = 0;
                            root.activeTab = 0;
                        }
                    }
                }

                // Tab 1: Monitores
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 4
                    color: root.activeTab === 1 ? root.colSurfaceRaised : "transparent"
                    border.color: root.activeTab === 1 ? root.colPacingCyan : "transparent"
                    border.width: root.activeTab === 1 ? 1 : 0

                    Text {
                        anchors.centerIn: parent
                        text: "🖥 Displays"
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        color: root.activeTab === 1 ? root.colPacingCyan : root.colTextMuted
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.galleryModel) root.galleryModel.controlDockTab = 1;
                            root.activeTab = 1;
                        }
                    }
                }

                // Tab 2: Escena & Audio
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 4
                    color: root.activeTab === 2 ? root.colSurfaceRaised : "transparent"
                    border.color: root.activeTab === 2 ? root.colPacingCyan : "transparent"
                    border.width: root.activeTab === 2 ? 1 : 0

                    Text {
                        anchors.centerIn: parent
                        text: "🎛 Scene"
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        color: root.activeTab === 2 ? root.colPacingCyan : root.colTextMuted
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.galleryModel) root.galleryModel.controlDockTab = 2;
                            root.activeTab = 2;
                        }
                    }
                }
            }
        }

        // ==================== TAB 0: PACING & RENDIMIENTO ====================
        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.activeTab === 0
            contentWidth: width
            contentHeight: pacingCol.implicitHeight + 10
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ColumnLayout {
                id: pacingCol
                width: parent.width
                spacing: 12

                // 1-Click Presets Header
                Text {
                    text: "PERFORMANCE PRESETS"
                    font.pixelSize: 10
                    font.family: "JetBrains Mono, monospace"
                    font.weight: Font.Bold
                    color: root.colTextDim
                }

                // 1-Click Presets Button Row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    // ECO
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 28
                        radius: 4
                        readonly property bool isActive: (root.galleryModel.powerProfile === "lite")
                        color: isActive ? Qt.rgba(16/255, 185/255, 129/255, 0.2) : (ecoArea.containsMouse ? root.colSurfaceRaised : root.colBaseVoid)
                        border.color: isActive ? root.colSuccess : root.colBorderSubtle
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "ECO"
                            font.pixelSize: 10
                            font.family: "JetBrains Mono, monospace"
                            font.weight: Font.Bold
                            color: parent.isActive ? root.colSuccess : root.colTextPrimary
                        }

                        MouseArea {
                            id: ecoArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.galleryModel.applyPreset("eco")
                        }
                    }

                    // BALANCED
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 28
                        radius: 4
                        readonly property bool isActive: (root.galleryModel.powerProfile === "balanced")
                        color: isActive ? Qt.rgba(56/255, 189/255, 248/255, 0.2) : (balArea.containsMouse ? root.colSurfaceRaised : root.colBaseVoid)
                        border.color: isActive ? root.colPacingCyan : root.colBorderSubtle
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "BALANCED"
                            font.pixelSize: 10
                            font.family: "JetBrains Mono, monospace"
                            font.weight: Font.Bold
                            color: parent.isActive ? root.colPacingCyan : root.colTextPrimary
                        }

                        MouseArea {
                            id: balArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.galleryModel.applyPreset("balanced")
                        }
                    }

                    // ULTRA
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 28
                        radius: 4
                        readonly property bool isActive: (root.galleryModel.powerProfile === "full")
                        color: isActive ? Qt.rgba(245/255, 158/255, 11/255, 0.2) : (ultraArea.containsMouse ? root.colSurfaceRaised : root.colBaseVoid)
                        border.color: isActive ? root.colTelemetryAmber : root.colBorderSubtle
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "ULTRA"
                            font.pixelSize: 10
                            font.family: "JetBrains Mono, monospace"
                            font.weight: Font.Bold
                            color: parent.isActive ? root.colTelemetryAmber : root.colTextPrimary
                        }

                        MouseArea {
                            id: ultraArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.galleryModel.applyPreset("ultra")
                        }
                    }
                }

                // AC FPS Slider
                PerformanceSlider {
                    Layout.fillWidth: true
                    label: "FPS on AC Power"
                    value: root.galleryModel.powerAcFps > 0 ? root.galleryModel.powerAcFps : 60
                    from: 15
                    to: 144
                    stepSize: 1
                    unit: "FPS"
                    ticks: [15, 30, 60, 120, 144]
                    onValueModified: (v) => {
                        root.galleryModel.powerAcFps = Math.round(v);
                        root.galleryModel.applyPower();
                    }
                }

                // Battery FPS Slider
                PerformanceSlider {
                    Layout.fillWidth: true
                    label: "FPS on Battery"
                    value: root.galleryModel.powerBattFps
                    from: 0
                    to: 60
                    stepSize: 1
                    unit: "FPS"
                    zeroLabel: "Pause (0 FPS)"
                    accentColor: root.colTelemetryAmber
                    ticks: [0, 10, 15, 24, 30]
                    onValueModified: (v) => {
                        root.galleryModel.powerBattFps = Math.round(v);
                        root.galleryModel.applyPower();
                    }
                }

                // Smart Pause switches section
                Text {
                    text: "SMART PAUSE POLICIES"
                    font.pixelSize: 10
                    font.family: "JetBrains Mono, monospace"
                    font.weight: Font.Bold
                    color: root.colTextDim
                }

                // Pause on battery switch
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 32
                    radius: 4
                    color: root.colBaseVoid
                    border.color: root.colBorderSubtle
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        Text {
                            text: "Pause on battery"
                            font.pixelSize: 11
                            color: root.colTextPrimary
                            Layout.fillWidth: true
                        }
                        Rectangle {
                            width: 32
                            height: 18
                            radius: 9
                            color: root.galleryModel.powerPauseOnBatt ? root.colPacingCyan : root.colSurfaceRaised
                            Rectangle {
                                width: 14
                                height: 14
                                radius: 7
                                color: root.galleryModel.powerPauseOnBatt ? "#111317" : root.colTextDim
                                anchors.verticalCenter: parent.verticalCenter
                                x: root.galleryModel.powerPauseOnBatt ? 16 : 2
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.galleryModel.powerPauseOnBatt = !root.galleryModel.powerPauseOnBatt;
                                    root.galleryModel.applyPower();
                                }
                            }
                        }
                    }
                }

                // Pause on fullscreen switch
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 32
                    radius: 4
                    color: root.colBaseVoid
                    border.color: root.colBorderSubtle
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        Text {
                            text: "Pause on fullscreen"
                            font.pixelSize: 11
                            color: root.colTextPrimary
                            Layout.fillWidth: true
                        }
                        Rectangle {
                            width: 32
                            height: 18
                            radius: 9
                            color: root.galleryModel.powerFullscreenPause ? root.colPacingCyan : root.colSurfaceRaised
                            Rectangle {
                                width: 14
                                height: 14
                                radius: 7
                                color: root.galleryModel.powerFullscreenPause ? "#111317" : root.colTextDim
                                anchors.verticalCenter: parent.verticalCenter
                                x: root.galleryModel.powerFullscreenPause ? 16 : 2
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.galleryModel.powerFullscreenPause = !root.galleryModel.powerFullscreenPause;
                                    root.galleryModel.applyPower();
                                }
                            }
                        }
                    }
                }

                // Active window pause switch
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 32
                    radius: 4
                    color: root.colBaseVoid
                    border.color: root.colBorderSubtle
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        Text {
                            text: "Pause only when active"
                            font.pixelSize: 11
                            color: root.colTextPrimary
                            Layout.fillWidth: true
                        }
                        Rectangle {
                            width: 32
                            height: 18
                            radius: 9
                            color: root.galleryModel.powerPauseActiveOnly ? root.colPacingCyan : root.colSurfaceRaised
                            Rectangle {
                                width: 14
                                height: 14
                                radius: 7
                                color: root.galleryModel.powerPauseActiveOnly ? "#111317" : root.colTextDim
                                anchors.verticalCenter: parent.verticalCenter
                                x: root.galleryModel.powerPauseActiveOnly ? 16 : 2
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.galleryModel.powerPauseActiveOnly = !root.galleryModel.powerPauseActiveOnly;
                                    root.galleryModel.applyPower();
                                }
                            }
                        }
                    }
                }
            }
        }

        // ==================== TAB 1: MONITORES & ASIGNACIÓN ====================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.activeTab === 1

            ColumnLayout {
                anchors.fill: parent
                spacing: 12

                Text {
                    text: "DISPLAY MATRIX"
                    font.pixelSize: 10
                    font.family: "JetBrains Mono, monospace"
                    font.weight: Font.Bold
                    color: root.colTextDim
                }

                ScreenMatrixSelector {
                    Layout.fillWidth: true
                    screens: root.galleryModel.screens
                    selectedScreen: root.galleryModel.selectedScreen
                    multimonitorMode: root.galleryModel.multimonitorMode
                    targetWallpaperId: root.galleryModel.selectedId
                    onScreenSelected: (name) => root.galleryModel.selectedScreen = name
                    onModeChanged: (m) => root.galleryModel.setMultimonitor(m)
                    onAssignRequested: (sc, id) => root.galleryModel.applyToScreen(sc, id, "normal")
                }

                Item { Layout.fillHeight: true }
            }
        }

        // ==================== TAB 2: ESCENA & AUDIO ====================
        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.activeTab === 2
            contentWidth: width
            contentHeight: sceneCol.implicitHeight + 10
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ColumnLayout {
                id: sceneCol
                width: parent.width
                spacing: 12

                // Audio Controls Section
                Text {
                    text: "AUDIO & VOLUME"
                    font.pixelSize: 10
                    font.family: "JetBrains Mono, monospace"
                    font.weight: Font.Bold
                    color: root.colTextDim
                }

                AudioControlPill {
                    Layout.fillWidth: true
                    volume: root.galleryModel.audioVolume
                    muted: !root.galleryModel.audioEnabled
                    onVolumeModified: (v) => root.galleryModel.setVolume(v)
                    onMuteToggled: (m) => root.galleryModel.toggleMute()
                }

                // Scene Properties Header
                Text {
                    text: root.selectedTitle !== "" 
                          ? ("PROPERTIES · " + root.selectedTitle) 
                          : "SCENE PROPERTIES"
                    font.pixelSize: 10
                    font.family: "JetBrains Mono, monospace"
                    font.weight: Font.Bold
                    color: root.colTextDim
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                // Message if no properties
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 64
                    radius: 5
                    color: root.colBaseVoid
                    border.color: root.colBorderSubtle
                    border.width: 1
                    visible: root.galleryModel.props.length === 0

                    ColumnLayout {
                        anchors.centerIn: parent
                        anchors.margins: 8
                        spacing: 4

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: root.selectedTitle !== "" ? root.selectedTitle : "No wallpaper selected"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                            color: root.colTextPrimary
                            elide: Text.ElideRight
                            visible: root.selectedTitle !== ""
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: root.galleryModel.selectedId === ""
                                  ? "Select a wallpaper in the catalog"
                                  : "No dynamic properties"
                            font.pixelSize: 11
                            color: root.colTextDim
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }

                // List of polymorphic PropertyFields
                Repeater {
                    model: root.galleryModel.props

                    PropertyField {
                        required property var modelData
                        required property int index

                        Layout.fillWidth: true
                        prop: modelData
                        onPropertyEdited: (name, val) => {
                            root.galleryModel.saveProp(name, val);
                        }
                    }
                }

                // Clear overrides button
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 28
                    radius: 4
                    color: resetPropsArea.containsMouse ? root.colSurfaceRaised : root.colBaseVoid
                    border.color: root.colBorderSubtle
                    border.width: 1
                    visible: root.galleryModel.props.length > 0

                    Text {
                        anchors.centerIn: parent
                        text: "Reset to defaults"
                        font.pixelSize: 10
                        color: root.colTextMuted
                    }

                    MouseArea {
                        id: resetPropsArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.galleryModel.resetProps()
                    }
                }

                // Wallpaper Actions & Delete Section
                Text {
                    text: "WALLPAPER ACTIONS"
                    font.pixelSize: 10
                    font.family: "JetBrains Mono, monospace"
                    font.weight: Font.Bold
                    color: root.colTextDim
                    visible: root.galleryModel.selectedId !== ""
                }

                Rectangle {
                    id: dockManageBox
                    Layout.fillWidth: true
                    implicitHeight: manageCol.implicitHeight + 16
                    radius: 6
                    color: root.colBaseVoid
                    border.color: root.colBorderSubtle
                    border.width: 1
                    visible: root.galleryModel.selectedId !== ""

                    readonly property var currentWp: root.galleryModel.selectedItem()
                    readonly property bool isWPE: root.galleryModel.isWallpaperEngine(currentWp)

                    ColumnLayout {
                        id: manageCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 8
                        spacing: 8

                        // Protection Notice Banner for Wallpaper Engine
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: wpeCol.implicitHeight + 12
                            radius: 5
                            color: Qt.rgba(245/255, 158/255, 11/255, 0.12)
                            border.color: Qt.rgba(245/255, 158/255, 11/255, 0.35)
                            border.width: 1
                            visible: dockManageBox.isWPE

                            ColumnLayout {
                                id: wpeCol
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 8
                                spacing: 4

                                RowLayout {
                                    spacing: 6
                                    Text {
                                        text: "🔒"
                                        font.pixelSize: 11
                                    }
                                    Text {
                                        text: "Wallpaper Engine Protegido"
                                        font.pixelSize: 10
                                        font.weight: Font.Bold
                                        color: root.colTelemetryAmber
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: "Los fondos de Wallpaper Engine deben gestionarse y eliminarse desde Steam / Wallpaper Engine."
                                    font.pixelSize: 9
                                    color: root.colTextMuted
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }

                        // Delete button
                        Rectangle {
                            id: dockDeleteBtn
                            Layout.fillWidth: true
                            implicitHeight: 28
                            radius: 4
                            property bool confirming: false

                            Timer {
                                id: dockConfirmTimer
                                interval: 4000
                                running: false
                                repeat: false
                                onTriggered: dockDeleteBtn.confirming = false
                            }

                            color: {
                                if (dockManageBox.isWPE) return Qt.rgba(45/255, 52/255, 64/255, 0.3);
                                if (dockDeleteBtn.confirming) return root.colDanger;
                                return dockDelArea.containsMouse ? Qt.rgba(239/255, 68/255, 68/255, 0.2) : root.colSurfaceRaised;
                            }
                            border.color: {
                                if (dockManageBox.isWPE) return root.colBorderSubtle;
                                if (dockDeleteBtn.confirming) return root.colDanger;
                                return dockDelArea.containsMouse ? root.colDanger : root.colBorderSubtle;
                            }
                            border.width: 1

                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 6

                                Text {
                                    text: dockManageBox.isWPE ? "🔒" : "🗑"
                                    font.pixelSize: 11
                                    color: dockManageBox.isWPE ? root.colTextDim : (dockDeleteBtn.confirming ? "#FFFFFF" : root.colTextPrimary)
                                }

                                Text {
                                    text: {
                                        if (dockManageBox.isWPE) return "Eliminación bloqueada (Steam)";
                                        if (dockDeleteBtn.confirming) return "Confirmar eliminación definitiva";
                                        return "Eliminar fondo de pantalla";
                                    }
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                    color: dockManageBox.isWPE ? root.colTextDim : (dockDeleteBtn.confirming ? "#FFFFFF" : (dockDelArea.containsMouse ? root.colDanger : root.colTextPrimary))
                                }
                            }

                            MouseArea {
                                id: dockDelArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: dockManageBox.isWPE ? Qt.ForbiddenCursor : Qt.PointingHandCursor
                                onClicked: {
                                    if (dockManageBox.isWPE) {
                                        root.galleryModel.showNotice("Los fondos de Wallpaper Engine deben gestionarse y eliminarse desde Steam / Wallpaper Engine.", "blocked");
                                        return;
                                    }
                                    if (!dockDeleteBtn.confirming) {
                                        dockDeleteBtn.confirming = true;
                                        dockConfirmTimer.restart();
                                    } else {
                                        dockDeleteBtn.confirming = false;
                                        dockConfirmTimer.stop();
                                        root.galleryModel.confirmDelete(dockManageBox.currentWp);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
