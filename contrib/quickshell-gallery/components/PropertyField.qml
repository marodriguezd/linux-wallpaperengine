pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property var prop: ({})
    property color colSurface: "#191C22"
    property color colSurfaceRaised: "#222730"
    property color colBorderSubtle: "#2D3440"
    property color colPacingCyan: "#38BDF8"
    property color colTelemetryAmber: "#F59E0B"
    property color colTextPrimary: "#F1F5F9"
    property color colTextMuted: "#94A3B8"
    property color colTextDim: "#64748B"

    signal propertyEdited(string name, string newValue)

    readonly property string propType: (root.prop && root.prop.type) ? String(root.prop.type).toLowerCase() : "text"
    readonly property string propName: (root.prop && root.prop.name) ? String(root.prop.name) : ""
    readonly property string propLabel: (root.prop && root.prop.text) ? String(root.prop.text) : root.propName
    readonly property string propValue: (root.prop && root.prop.value !== undefined) ? String(root.prop.value) : ""
    readonly property bool isSaved: Boolean(root.prop && root.prop.saved)

    implicitWidth: 260
    implicitHeight: {
        if (propType === "slider") return 50;
        if (propType === "color") return 40;
        if (propType === "bool") return 36;
        return 32;
    }

    radius: 5
    color: root.colSurface
    border.color: root.isSaved ? root.colPacingCyan : root.colBorderSubtle
    border.width: root.isSaved ? 1 : 1

    // Helper to parse "r g b" space-separated float color (0..1)
    function parseRgbColor(str) {
        if (!str) return "#38BDF8";
        const parts = str.trim().split(/\s+/);
        if (parts.length >= 3) {
            let r = parseFloat(parts[0]);
            let g = parseFloat(parts[1]);
            let b = parseFloat(parts[2]);
            if (r > 1.0 || g > 1.0 || b > 1.0) {
                r = r / 255.0;
                g = g / 255.0;
                b = b / 255.0;
            }
            return Qt.rgba(Math.max(0, Math.min(1, r)), Math.max(0, Math.min(1, g)), Math.max(0, Math.min(1, b)), 1.0);
        }
        return "#38BDF8";
    }

    Item {
        anchors.fill: parent
        anchors.margins: 8

        // ================= TYPE: BOOL =================
        RowLayout {
            anchors.fill: parent
            visible: root.propType === "bool"
            spacing: 8

            Text {
                text: root.propLabel
                font.pixelSize: 11
                color: root.colTextPrimary
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Rectangle {
                width: 36
                height: 20
                radius: 10
                color: (root.propValue === "true" || root.propValue === "1") ? root.colPacingCyan : root.colSurfaceRaised
                border.color: root.colBorderSubtle
                border.width: 1

                Rectangle {
                    width: 16
                    height: 16
                    radius: 8
                    color: (root.propValue === "true" || root.propValue === "1") ? "#111317" : root.colTextDim
                    anchors.verticalCenter: parent.verticalCenter
                    x: (root.propValue === "true" || root.propValue === "1") ? 18 : 2

                    Behavior on x { NumberAnimation { duration: 120 } }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        const nextVal = (root.propValue === "true" || root.propValue === "1") ? "false" : "true";
                        root.propertyEdited(root.propName, nextVal);
                    }
                }
            }
        }

        // ================= TYPE: SLIDER =================
        ColumnLayout {
            anchors.fill: parent
            visible: root.propType === "slider"
            spacing: 4

            PerformanceSlider {
                Layout.fillWidth: true
                label: root.propLabel
                value: {
                    const parsed = parseFloat(root.propValue);
                    return isNaN(parsed) ? 0 : parsed;
                }
                from: !isNaN(root.prop.min) ? root.prop.min : 0
                to: !isNaN(root.prop.max) ? root.prop.max : 1
                stepSize: !isNaN(root.prop.step) ? root.prop.step : 0.05
                unit: ""
                onValueModified: (v) => {
                    root.propertyEdited(root.propName, String(Math.round(v * 1000) / 1000));
                }
            }
        }

        // ================= TYPE: COLOR =================
        RowLayout {
            anchors.fill: parent
            visible: root.propType === "color"
            spacing: 8

            Text {
                text: root.propLabel
                font.pixelSize: 11
                color: root.colTextPrimary
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            // Color Chip Preview
            Rectangle {
                width: 48
                height: 22
                radius: 4
                color: root.parseRgbColor(root.propValue)
                border.color: "#FFFFFF"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "RGB"
                    font.pixelSize: 8
                    font.weight: Font.Bold
                    color: "#FFFFFF"
                    style: Text.Outline
                    styleColor: "#000000"
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        // Quick cycle through primary themes/colors if clicked
                        const presets = [
                            "0.22 0.74 0.97", // Cyan
                            "0.96 0.62 0.04", // Amber
                            "0.06 0.73 0.51", // Emerald
                            "0.93 0.28 0.59", // Rose / Pink
                            "0.11 0.12 0.15"  // Dark void
                        ];
                        let curIdx = presets.indexOf(root.propValue.trim());
                        let nextIdx = (curIdx + 1) % presets.length;
                        root.propertyEdited(root.propName, presets[nextIdx]);
                    }
                }
            }
        }

        // ================= TYPE: TEXT / DEFAULT =================
        RowLayout {
            anchors.fill: parent
            visible: root.propType !== "bool" && root.propType !== "slider" && root.propType !== "color"
            spacing: 8

            Text {
                text: root.propLabel
                font.pixelSize: 11
                color: root.colTextPrimary
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Text {
                text: root.propValue
                font.pixelSize: 10
                font.family: "JetBrains Mono, monospace"
                color: root.colTextDim
                elide: Text.ElideMiddle
                Layout.maximumWidth: 100
            }
        }
    }
}
