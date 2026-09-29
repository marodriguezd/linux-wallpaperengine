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

        applyProcess.command = ["we-wallpaper", "apply-id", item.id];
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
