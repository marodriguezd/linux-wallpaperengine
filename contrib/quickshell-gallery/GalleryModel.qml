pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import Quickshell.Io
import qs.core

// Gallery model: reads the fork catalog (we-wallpaper gallery-json) and
// applies the clicked wallpaper. No engine restart, no quickshell reload.
Scope {
    id: root

    property bool visible: false
    property string query: ""
    property string typeFilter: ""
    property var items: []
    property string status: ""
    property string selectedId: ""
    property var props: []
    property var pendingEdits: ({})
    property string libraryPath: (Quickshell.env("HOME") || "") + "/.config/dwm-titus/gallery-library.json"

    function filteredItems() : var {
        const out = [];
        const q = root.query.toLowerCase();

        for (let i = 0; i < root.items.length; i++) {
            const item = root.items[i];

            if (!item) {
                continue;
            }

            if (root.typeFilter !== "" && (item.type || "") !== root.typeFilter) {
                continue;
            }

            if (q !== "") {
                const hay = ((item.id || "") + "\n" + (item.title || "")).toLowerCase();

                if (hay.indexOf(q) === -1) {
                    continue;
                }
            }

            out.push(item);
        }

        return out;
    }

    function open() : void {
        root.visible = true;
    }

    function close() : void {
        root.visible = false;
    }

    function toggle() : void {
        root.visible = !root.visible;
    }

    function setTypeFilter(type : string) : void {
        root.typeFilter = (root.typeFilter === type) ? "" : type;
    }

    function apply(item : var) : void {
        if (!item || !item.id) {
            return;
        }

        root.select(item);
        applyProcess.command = ["we-wallpaper", "apply-id", item.id];
        applyProcess.running = true;
    }

    function select(item : var) : void {
        if (!item || !item.id || root.selectedId === item.id) {
            return;
        }

        root.selectedId = item.id;
        root.pendingEdits = ({});
        loadPropsProcess.command = ["we-wallpaper", "props", item.id];
        loadPropsProcess.running = true;
    }

    function editProp(name : string, value : string) : void {
        const edits = Object.assign({}, root.pendingEdits);
        edits[name] = value;
        root.pendingEdits = edits;
    }

    function saveProps() : void {
        if (root.selectedId === "") {
            return;
        }

        const args = ["we-wallpaper", "props", root.selectedId];

        for (const name in root.pendingEdits) {
            args.push(name + "=" + root.pendingEdits[name]);
        }

        if (args.length <= 3) {
            root.status = "no changes";
            return;
        }

        savePropsProcess.command = args;
        savePropsProcess.running = true;
    }

    function reloadProps() : void {
        const id = root.selectedId;
        root.selectedId = "";
        root.pendingEdits = ({});

        if (id !== "") {
            root.select({ id: id });
        }
    }

    function applySelected() : void {
        if (root.selectedId === "") {
            return;
        }

        applyProcess.command = ["we-wallpaper", "apply-id", root.selectedId];
        applyProcess.running = true;
    }

    function refresh() : void {
        refreshProcess.running = true;
    }

    FileView {
        id: libraryFile
        path: root.libraryPath
        watchChanges: true
        printErrors: false
        adapter: JsonAdapter {
            id: libraryAdapter
            property var items: []
        }
        onLoaded: root.items = libraryAdapter.items || []
        onFileChanged: libraryFile.reload()
        onLoadFailed: root.items = []
    }

    Process {
        id: applyProcess
        running: false
        stdout: StdioCollector {
            onStreamFinished: root.status = this.text
        }
        stderr: StdioCollector {}
    }

    Process {
        id: loadPropsProcess
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                const rows = [];
                const lines = this.text.split("\n");

                for (let i = 0; i < lines.length; i++) {
                    const cols = lines[i].split("\t");

                    if (cols.length < 4 || cols[0] === "") {
                        continue;
                    }

                    rows.push({
                        name: cols[0],
                        type: cols[1],
                        text: cols[2],
                        value: cols[3],
                        saved: cols.length > 4 && cols[4] !== ""
                    });
                }

                root.props = rows;
                root.status = rows.length + " properties";
            }
        }
        stderr: StdioCollector {}
    }

    Process {
        id: savePropsProcess
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                root.status = this.text;
                root.reloadProps();
            }
        }
        stderr: StdioCollector {}
    }

    Process {
        id: refreshProcess
        command: ["we-wallpaper", "gallery-json"]
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                root.status = this.text;
                libraryFile.reload();
            }
        }
        stderr: StdioCollector {}
    }

    Component.onCompleted: root.refresh()
}
