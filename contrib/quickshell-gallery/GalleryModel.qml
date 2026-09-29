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
    property string mode: "local"
    property string query: ""
    property string typeFilter: ""
    property string categoryFilter: ""
    property bool showFavorites: false
    property var items: []
    property var exploreItems: []
    property string exploreSource: "bing"
    property string exploreQuery: ""
    property string selectedId: ""
    property var props: []
    property var pendingEdits: ({})
    property string libraryPath: (Quickshell.env("HOME") || "") + "/.config/dwm-titus/gallery-library.json"

    readonly property var categories: ["anime", "nature", "sci-fi", "cyberpunk", "gaming", "minimalist"]

    function categoryOf(item) : string {
        if (!item) {
            return "";
        }

        const tags = item.tags || [];

        for (let i = 0; i < tags.length; i++) {
            const tag = String(tags[i]).toLowerCase();

            for (let c = 0; c < categories.length; c++) {
                if (tag.indexOf(categories[c]) !== -1 || categories[c].indexOf(tag) !== -1) {
                    return categories[c];
                }
            }
        }

        return "";
    }

    function filteredItems() : var {
        const out = [];
        const q = root.query.toLowerCase();

        for (let i = 0; i < root.items.length; i++) {
            const item = root.items[i];

            if (!item) {
                continue;
            }

            if (root.showFavorites && !item.favorite) {
                continue;
            }

            if (root.typeFilter !== "" && (item.type || "") !== root.typeFilter) {
                continue;
            }

            if (root.categoryFilter !== "" && root.categoryOf(item) !== root.categoryFilter) {
                continue;
            }

            if (q !== "") {
                let hay = ((item.id || "") + "\n" + (item.title || "") + "\n" + (item.description || "")).toLowerCase();
                const tags = item.tags || [];

                for (let t = 0; t < tags.length; t++) {
                    hay += "\n" + String(tags[t]).toLowerCase();
                }

                if (hay.indexOf(q) === -1) {
                    continue;
                }
            }

            out.push(item);
        }

        return out;
    }

    function setMode(m : string) : void {
        root.mode = m;
    }

    function explore() : void {
        exploreProcess.command = ["we-wallpaper", "catalog", root.exploreSource, root.exploreQuery];
        exploreProcess.running = true;
    }

    function download(item : var) : void {
        if (!item || !item.ref) {
            return;
        }

        downloadProcess.command = ["we-wallpaper", "catalog-get", item.ref];
        downloadProcess.running = true;
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

    function setCategoryFilter(cat : string) : void {
        root.categoryFilter = (root.categoryFilter === cat) ? "" : cat;
    }

    function toggleFavorites() : void {
        root.showFavorites = !root.showFavorites;
    }

    function toggleFavorite(item : var) : void {
        if (!item || !item.id) {
            return;
        }

        favProcess.command = ["we-wallpaper", "fav", item.id];
        favProcess.running = true;
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

    Process {
        id: favProcess
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                root.status = this.text;
                refreshProcess.running = true;
            }
        }
        stderr: StdioCollector {}
    }

    Process {
        id: exploreProcess
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                const rows = [];
                const lines = this.text.split("\n");

                for (let i = 0; i < lines.length; i++) {
                    const cols = lines[i].split("|");

                    if (cols.length < 5 || cols[0] === "") {
                        continue;
                    }

                    rows.push({
                        ref: cols[0],
                        title: cols[1],
                        thumb: cols[2],
                        file: cols[3],
                        kind: cols[4]
                    });
                }

                root.exploreItems = rows;
                root.status = rows.length + " online";
            }
        }
        stderr: StdioCollector {}
    }

    Process {
        id: downloadProcess
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                root.status = this.text;
                refreshProcess.running = true;
            }
        }
        stderr: StdioCollector {}
    }

    Component.onCompleted: root.refresh()
}
