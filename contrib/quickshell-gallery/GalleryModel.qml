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
    property string exploreFilter: ""
    property var exploreSelected: null
    property string status: ""
    property string currentId: ""
    property string sortOrder: ""
    property bool exploreLoading: false
    property string exploreError: ""
    property string downloadingRef: ""
    property string exploreQuality: "hd"
    property int explorePage: 1
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

        if (root.sortOrder === "az" || root.sortOrder === "za") {
            out.sort(function(a, b) {
                const ta = ((a.title || a.id || "") + "").toLowerCase();
                const tb = ((b.title || b.id || "") + "").toLowerCase();
                if (ta < tb) {
                    return root.sortOrder === "az" ? -1 : 1;
                }
                if (ta > tb) {
                    return root.sortOrder === "az" ? 1 : -1;
                }
                return 0;
            });
        }

        return out;
    }

    function toggleSort() : void {
        root.sortOrder = root.sortOrder === "az" ? "za" : (root.sortOrder === "za" ? "" : "az");
    }

    function typeCount(t : string) : int {
        if (t === "fav") {
            let n = 0;
            for (let i = 0; i < root.items.length; i++) {
                if (root.items[i] && root.items[i].favorite) {
                    n++;
                }
            }
            return n;
        }

        if (t === "") {
            return root.items.length;
        }

        let n = 0;
        for (let i = 0; i < root.items.length; i++) {
            if (root.items[i] && (root.items[i].type || "") === t) {
                n++;
            }
        }
        return n;
    }

    function setMode(m : string) : void {
        root.mode = m;
    }

    function explore() : void {
        root.exploreSelected = null;
        root.exploreError = "";
        root.exploreLoading = true;
        root.status = "buscando online...";
        const args = ["we-wallpaper", "catalog", root.exploreSource, root.exploreQuery];
        if (root.exploreSource === "motionbgs" || root.exploreSource === "wallhaven" || root.exploreSource === "minimal") {
            args.push(String(root.explorePage));
        }
        exploreProcess.command = args;
        exploreProcess.running = true;
    }

    function exploreMore() : void {
        root.explorePage += 1;
        explore();
    }

    function searchExplore() : void {
        root.exploreQuery = root.exploreFilter;
        root.exploreFilter = "";
        refreshExplore();
    }

    function refreshExplore() : void {
        root.explorePage = 1;
        root.exploreItems = [];
        explore();
    }

    function setMotionTag(t : string) : void {
        root.exploreQuery = t;
        root.exploreFilter = "";
        refreshExplore();
    }

    function setSource(s : string) : void {
        root.exploreSource = s;
        root.exploreQuery = "";
        root.exploreFilter = "";
        refreshExplore();
    }

    function setWallhavenSort(s : string) : void {
        const parts = root.exploreQuery.split(/\s+/).filter(function(p) {
            return p !== "" && p.indexOf("sort:") !== 0;
        });
        parts.unshift("sort:" + s);
        root.exploreQuery = parts.join(" ");
        root.exploreFilter = "";
        refreshExplore();
    }

    function wallhavenSort() : string {
        const m = root.exploreQuery.match(/sort:([a-z]+)/);
        return m ? m[1] : "toplist";
    }

    function installedLocalId(ref : string) : string {
        if (!ref) {
            return "";
        }
        const ci = ref.indexOf(":");
        if (ci === -1) {
            return "";
        }
        const src = ref.slice(0, ci);
        const key = ref.slice(ci + 1).toLowerCase();
        for (let i = 0; i < root.items.length; i++) {
            const it = root.items[i];
            if (!it || !it.id) {
                continue;
            }
            const lid = String(it.id);
            if (src === "motionbgs" && lid.toLowerCase().indexOf("motionbgs-" + key + "-") === 0) {
                return lid;
            }
            if ((src === "wallhaven" || src === "minimal" || src === "bing") && (it.type || "") === "image") {
                const stem = key.split(".")[0];
                if (stem.length >= 4 && lid.toLowerCase().indexOf(stem.slice(0, 12)) !== -1) {
                    return lid;
                }
                if (src === "bing" && lid.toLowerCase() === "img-bing-" + key) {
                    return lid;
                }
            }
        }
        return "";
    }

    function installOrApply(item : var) : void {
        if (!item || !item.ref) {
            return;
        }
        const lid = installedLocalId(item.ref);
        if (lid !== "") {
            apply({ id: lid });
            return;
        }
        download(item);
    }

    function filteredExplore() : var {
        const q = root.exploreFilter.toLowerCase();
        if (q === "") {
            return root.exploreItems;
        }

        const out = [];

        for (let i = 0; i < root.exploreItems.length; i++) {
            const item = root.exploreItems[i];
            const hay = ((item.ref || "") + "\n" + (item.title || "")).toLowerCase();

            if (hay.indexOf(q) !== -1) {
                out.push(item);
            }
        }

        return out;
    }

    function selectExplore(item : var) : void {
        root.exploreSelected = item;
        root.status = (item && item.title) ? item.title : "";
    }

    function download(item : var) : void {
        if (!item || !item.ref || root.downloadingRef !== "") {
            return;
        }

        root.downloadingRef = item.ref;
        root.status = "instalando " + (item.title || item.ref) + "...";
        const src = item.ref.split(":")[0];
        const args = ["we-wallpaper", "catalog-get", item.ref];
        if (src === "motionbgs") {
            args.push(root.exploreQuality);
        }
        if (item.title) {
            args.push(item.title);
        }
        downloadProcess.command = args;
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

        if ((item.type || "") === "image") {
            root.props = [];
            return;
        }

        loadPropsProcess.command = ["we-wallpaper", "props", item.id];
        loadPropsProcess.running = true;
    }

    function selectedItem() : var {
        for (let i = 0; i < root.items.length; i++) {
            if (root.items[i] && root.items[i].id === root.selectedId) {
                return root.items[i];
            }
        }

        return null;
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
        currentProcess.running = true;
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
            onStreamFinished: {
                root.status = this.text;
                currentProcess.running = true;
            }
        }
        stderr: StdioCollector {}
    }

    Process {
        id: currentProcess
        command: ["we-wallpaper", "current-id"]
        running: false
        stdout: StdioCollector {
            onStreamFinished: root.currentId = this.text.trim()
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

                root.exploreLoading = false;
                if (rows.length === 0) {
                    if (root.explorePage <= 1) {
                        root.exploreItems = [];
                    }
                    root.exploreError = "Sin resultados o red fallida. Reintenta.";
                    root.status = "explore: sin resultados";
                    return;
                }

                root.exploreError = "";
                if (root.explorePage > 1) {
                    root.exploreItems = root.exploreItems.concat(rows);
                } else {
                    root.exploreItems = rows;
                }
                root.status = root.exploreItems.length + " online";
            }
        }
        stderr: StdioCollector {}
    }

    Process {
        id: downloadProcess
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                root.downloadingRef = "";
                root.status = this.text;
                refreshProcess.running = true;
            }
        }
        stderr: StdioCollector {}
    }

    Component.onCompleted: root.refresh()
}
