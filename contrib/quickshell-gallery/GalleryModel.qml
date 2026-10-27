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
    property bool isRunning: false
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
    property string noticeMessage: ""
    property string noticeType: "info"
    property var deleteTarget: null
    property string selectedId: ""
    property var selectedWallpaper: null
    property var props: []
    property var pendingEdits: ({})
    // Multimonitor and Screens
    property var screens: []
    property string selectedScreen: ""
    property string multimonitorMode: "clone"
    // Audio and Volume
    property int audioVolume: 60
    property bool audioEnabled: false
    // Telemetry and UI
    property string batteryStatus: "🔋 100% AC"
    property string gpuInfo: "⚡ GPU Auto"
    property bool controlDockOpen: true
    property int controlDockTab: 0
    // Power panel: pending values (0/empty = auto/none), applied via applyPower()
    property string powerProfile: ""
    property int powerAcFps: 0
    property int powerBattFps: 10
    property bool powerPauseOnBatt: false
    property int powerIdleMin: 0
    property bool powerLoaded: false
    // Power/Backend panels start collapsed so the grid keeps its height
    property bool showPower: false
    // Backend panel: override "" = auto-detect; detected filled at refresh
    property string powerBackendOverride: ""
    property string backendDetected: ""
    property string powerLayer: "bottom"
    property bool powerPauseActiveOnly: false
    property string powerIgnoreAppid: ""
    property bool powerFullscreenPause: true
    property string powerHwdec: "auto"
    property string toolkitTheme: "auto"
    property string gpuTarget: "auto"
    property string libraryPath: (Quickshell.env("HOME") || "") + "/.config/dwm-titus/gallery-library.json"

    onVisibleChanged: {
        if (root.visible) {
            checkActiveProcess.running = true;
            root.loadScreens();
            root.loadTelemetry();
        }
    }

    readonly property var categories: ["anime", "nature", "sci-fi", "cyberpunk", "gaming", "minimalist"]

    function categoryCount(cat : string) : int {
        let n = 0;

        for (let i = 0; i < root.items.length; i++) {
            if (root.items[i] && root.categoryOf(root.items[i]) === cat) {
                n++;
            }
        }

        return n;
    }

    // moods with at least one wallpaper: dead chips never render
    function activeCategories() : var {
        const out = [];

        for (let c = 0; c < root.categories.length; c++) {
            if (root.categoryCount(root.categories[c]) > 0) {
                out.push(root.categories[c]);
            }
        }

        return out;
    }

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
        root.status = "searching online...";
        let query = root.exploreQuery;
        if (root.exploreSource === "bing" && (query === "archive" || query.indexOf("archive") === 0)) {
            query = "archive" + root.explorePage;
        }
        const args = ["we-wallpaper", "catalog", root.exploreSource, query];
        if (root.exploreSource === "motionbgs" || root.exploreSource === "wallhaven" || root.exploreSource === "desktophut" || root.exploreSource === "minimal") {
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
            if (src === "desktophut" && (lid.toLowerCase() === "desktophut-" + key || lid.toLowerCase() === key)) {
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
        root.status = "Downloading " + (item.title || item.ref) + "...";
        const src = item.ref.split(":")[0];
        downloadProcess.command = ["we-wallpaper", "catalog-get", item.ref, (src === "motionbgs" ? root.exploreQuality : ""), (item.title || "")];
        downloadProcess.running = true;
    }

    property var targetScreen: null

    function openOnScreen(screen : var) : void {
        if (root.visible && root.targetScreen === screen) {
            root.close();
            return;
        }
        root.targetScreen = screen;
        root.visible = true;
    }

    function open() : void {
        root.visible = true;
    }

    function close() : void {
        root.visible = false;
        root.targetScreen = null;
    }

    function toggle() : void {
        if (root.visible) {
            root.close();
        } else {
            root.open();
        }
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

    function toggleFav(item : var) : void {
        root.toggleFavorite(item);
    }

    function apply(item : var) : void {
        if (!item || !item.id) {
            return;
        }

        if (item.missing === true) {
            root.status = (item.title || item.id) + " is no longer installed";
            return;
        }

        root.select(item);
        applyProcess.command = ["we-wallpaper", "apply-id", item.id];
        applyProcess.running = true;
    }

    function toggleActive() : void {
        controlProcess.command = ["we-wallpaper", "toggle"];
        controlProcess.running = true;
    }

    function stopWallpaper() : void {
        controlProcess.command = ["we-wallpaper", "stop"];
        controlProcess.running = true;
    }

    function startWallpaper() : void {
        controlProcess.command = ["we-wallpaper", "restore"];
        controlProcess.running = true;
    }

    function select(item : var) : void {
        if (!item || !item.id) {
            return;
        }

        root.selectedId = item.id;
        root.selectedWallpaper = item;
        root.pendingEdits = ({});

        if ((item.type || "") === "image") {
            root.props = [];
            return;
        }

        loadPropsProcess.command = ["we-wallpaper", "props", item.id];
        loadPropsProcess.running = true;
    }

    function loadProps(id : string) : void {
        if (!id) return;
        for (let i = 0; i < root.items.length; i++) {
            if (root.items[i] && String(root.items[i].id) === String(id)) {
                root.select(root.items[i]);
                return;
            }
        }
        root.select({ id: id });
    }

    function inspect(item : var) : void {
        if (!item || !item.id) return;
        root.select(item);
        root.controlDockOpen = true;
        root.controlDockTab = 2; // Tab 2: Scene & Audio
    }

    function inspectId(id : string) : void {
        if (!id) return;
        for (let i = 0; i < root.items.length; i++) {
            if (root.items[i] && String(root.items[i].id) === String(id)) {
                root.inspect(root.items[i]);
                return;
            }
        }
        root.inspect({ id: id });
    }

    function selectedItem() : var {
        for (let i = 0; i < root.items.length; i++) {
            if (root.items[i] && root.items[i].id === root.selectedId) {
                return root.items[i];
            }
        }

        return root.selectedWallpaper;
    }

    function applyPreset(p : string) : void {
        if (p === "eco") {
            root.powerProfile = "lite";
            root.powerAcFps = 30;
            root.powerBattFps = 0;
            root.powerPauseOnBatt = true;
            root.powerFullscreenPause = true;
        } else if (p === "balanced") {
            root.powerProfile = "balanced";
            root.powerAcFps = 60;
            root.powerBattFps = 15;
            root.powerPauseOnBatt = false;
            root.powerFullscreenPause = true;
        } else if (p === "ultra") {
            root.powerProfile = "full";
            root.powerAcFps = 144;
            root.powerBattFps = 30;
            root.powerPauseOnBatt = false;
            root.powerFullscreenPause = false;
        }
        applyPower();
    }

    function setVolume(v : int) : void {
        root.audioVolume = v;
        volumeProcess.command = ["we-wallpaper", "volume", String(v)];
        volumeProcess.running = true;
    }

    function toggleMute() : void {
        root.audioEnabled = !root.audioEnabled;
        muteProcess.command = ["we-wallpaper", "mute", root.audioEnabled ? "off" : "on"];
        muteProcess.running = true;
    }

    function applyToScreen(screenName : string, wpId : string, mode : string) : void {
        if (!screenName || !wpId) return;
        applyScreenProcess.command = ["we-wallpaper", "apply-screen", screenName, wpId, mode || "lite"];
        applyScreenProcess.running = true;
    }

    function setMultimonitor(m : string) : void {
        root.multimonitorMode = m;
        multimonitorProcess.command = ["we-wallpaper", "multimonitor", m];
        multimonitorProcess.running = true;
    }

    function saveProp(name : string, val : string) : void {
        if (!root.selectedId || !name) return;
        savePropsProcess.command = ["we-wallpaper", "props", root.selectedId, name + "=" + val];
        savePropsProcess.running = true;
    }

    function resetProps() : void {
        if (!root.selectedId) return;
        resetPropsProcess.command = ["we-wallpaper", "props-clear", root.selectedId];
        resetPropsProcess.running = true;
    }

    function loadScreens() : void {
        screensProcess.command = ["we-wallpaper", "screens-json"];
        screensProcess.running = true;
    }

    function loadTelemetry() : void {
        telemetryProcess.running = true;
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

    function showNotice(msg : string, type : string) : void {
        root.noticeMessage = msg;
        root.noticeType = type || "info";
        root.status = msg;
        noticeTimer.restart();
    }

    function clearNotice() : void {
        root.noticeMessage = "";
        noticeTimer.stop();
    }

    function isWallpaperEngine(item : var) : bool {
        if (!item || !item.id) return false;
        if (item.source === "wallpaperengine") return true;
        if (item.source && item.source !== "wallpaperengine") return false;
        const id = String(item.id);
        if (id.indexOf("desktophut-") === 0 || id.indexOf("motionbgs-") === 0 ||
            id.indexOf("bing-") === 0 || id.indexOf("img-") === 0 ||
            id.indexOf("fetch-") === 0 || id.indexOf("import-") === 0) {
            return false;
        }
        return true;
    }

    function isStoreItem(id : string) : bool {
        if (!id) {
            return false;
        }
        return id.indexOf("motionbgs-") === 0 || id.indexOf("desktophut-") === 0 || id.indexOf("bing-") === 0 || id.indexOf("img-") === 0 || id.indexOf("fetch-") === 0 || id.indexOf("import-") === 0;
    }

    function requestDelete(item : var) : void {
        if (!item || !item.id) return;
        if (root.isWallpaperEngine(item)) {
            root.showNotice("Los fondos de Wallpaper Engine deben gestionarse y eliminarse desde Steam / Wallpaper Engine.", "blocked");
            return;
        }
        root.confirmDelete(item);
    }

    function confirmDelete(item : var) : void {
        const target = item || root.deleteTarget;
        if (!target || !target.id) return;
        if (root.isWallpaperEngine(target)) {
            root.showNotice("Los fondos de Wallpaper Engine deben gestionarse y eliminarse desde Steam / Wallpaper Engine.", "blocked");
            root.deleteTarget = null;
            return;
        }

        const tid = String(target.id);
        // Optimistically remove from items list immediately
        const updated = [];
        for (let i = 0; i < root.items.length; i++) {
            if (root.items[i] && String(root.items[i].id) !== tid) {
                updated.push(root.items[i]);
            }
        }
        root.items = updated;

        if (String(root.selectedId) === tid) {
            root.selectedId = "";
            root.selectedWallpaper = null;
            root.props = [];
        }
        root.deleteTarget = null;
        root.status = "Deleting " + (target.title || tid) + "...";
        removeProcess.command = ["we-wallpaper", "remove", tid];
        removeProcess.running = true;
    }

    function remove(item : var) : void {
        root.requestDelete(item);
    }

    function refresh() : void {
        refreshProcess.running = true;
        currentProcess.running = true;
        checkActiveProcess.running = true;
        powerProcess.command = ["we-wallpaper", "power"];
        powerProcess.running = true;
        backendProcess.command = ["we-wallpaper", "backend"];
        backendProcess.running = true;
        root.loadScreens();
        root.loadTelemetry();
    }

    function effBackend() : string {
        if (root.powerBackendOverride === "sway" || root.powerBackendOverride === "x11") {
            return root.powerBackendOverride;
        }
        return root.backendDetected;
    }

    readonly property var acFpsSteps: [0, 15, 30, 60, 120, 144]
    readonly property var battFpsSteps: [5, 10, 15, 30]
    readonly property var idleMinSteps: [0, 5, 10, 15, 30, 60]
    readonly property var hwdecSteps: ["auto", "vaapi-copy", "vaapi", "nvdec", "no"]
    readonly property var layerSteps: ["background", "bottom", "top", "overlay"]
    readonly property var toolkitThemeSteps: ["auto", "gtk", "qt", "brave"]
    readonly property var gpuTargetSteps: ["auto", "intel", "nvidia"]

    function acFpsLabel() : string {
        return root.powerAcFps <= 0 ? "auto" : String(root.powerAcFps);
    }

    function idleMinLabel() : string {
        return root.powerIdleMin <= 0 ? "off" : String(root.powerIdleMin) + " min";
    }

    function stepAcFps(dir : int) : void {
        let i = root.acFpsSteps.indexOf(root.powerAcFps);
        if (i === -1) {
            i = 0;
        }
        i = Math.max(0, Math.min(root.acFpsSteps.length - 1, i + dir));
        root.powerAcFps = root.acFpsSteps[i];
    }

    function stepBattFps(dir : int) : void {
        let i = root.battFpsSteps.indexOf(root.powerBattFps);
        if (i === -1) {
            i = 1;
        }
        i = Math.max(0, Math.min(root.battFpsSteps.length - 1, i + dir));
        root.powerBattFps = root.battFpsSteps[i];
    }

    function stepIdleMin(dir : int) : void {
        let i = root.idleMinSteps.indexOf(root.powerIdleMin);
        if (i === -1) {
            i = 0;
        }
        i = Math.max(0, Math.min(root.idleMinSteps.length - 1, i + dir));
        root.powerIdleMin = root.idleMinSteps[i];
    }

    function setPowerProfile(p : string) : void {
        root.powerProfile = (root.powerProfile === p) ? "" : p;
    }

    function cycleBackendOverride() : void {
        if (root.powerBackendOverride === "") {
            root.powerBackendOverride = "sway";
        } else if (root.powerBackendOverride === "sway") {
            root.powerBackendOverride = "x11";
        } else {
            root.powerBackendOverride = "";
        }
    }

    function backendOverrideLabel() : string {
        if (root.powerBackendOverride === "sway" || root.powerBackendOverride === "x11") {
            return root.powerBackendOverride;
        }
        return "auto";
    }

    function stepHwdec(dir : int) : void {
        let i = root.hwdecSteps.indexOf(root.powerHwdec);
        if (i === -1) {
            i = 0;
        }
        i = Math.max(0, Math.min(root.hwdecSteps.length - 1, i + dir));
        root.powerHwdec = root.hwdecSteps[i];
    }

    function cycleToolkitTheme() : void {
        let i = root.toolkitThemeSteps.indexOf(root.toolkitTheme);
        if (i === -1) {
            i = 0;
        }
        i = (i + 1) % root.toolkitThemeSteps.length;
        root.toolkitTheme = root.toolkitThemeSteps[i];
    }

    function toolkitThemeLabel() : string {
        switch (root.toolkitTheme) {
        case "gtk": return "GTK / Libadwaita";
        case "qt": return "Qt / Breeze";
        case "brave": return "Brave / Obsidian";
        default: return "Auto / System";
        }
    }

    function cycleGpuTarget() : void {
        let i = root.gpuTargetSteps.indexOf(root.gpuTarget);
        if (i === -1) {
            i = 0;
        }
        i = (i + 1) % root.gpuTargetSteps.length;
        root.gpuTarget = root.gpuTargetSteps[i];
    }

    function gpuTargetLabel() : string {
        switch (root.gpuTarget) {
        case "intel": return "Intel iGPU (Battery)";
        case "nvidia": return "dGPU Performance";
        default: return "Auto";
        }
    }

    function applyPower() : void {
        const args = [
            "we-wallpaper", "power-set",
            "WE_PROFILE=" + root.powerProfile,
            "WE_FPS=" + (root.powerAcFps > 0 ? String(root.powerAcFps) : ""),
            "WE_FPS_BATTERY=" + String(root.powerBattFps),
            "WE_PAUSE_ON_BATTERY=" + (root.powerPauseOnBatt ? "1" : "0"),
            "WE_IDLE_PAUSE=" + String(root.powerIdleMin),
            "WE_BACKEND=" + root.powerBackendOverride,
            "WE_LAYER=" + root.powerLayer,
            "WE_PAUSE_ACTIVE_ONLY=" + (root.powerPauseActiveOnly ? "1" : "0"),
            "WE_PAUSE_IGNORE_APPID=" + root.powerIgnoreAppid,
            "WE_NO_FULLSCREEN_PAUSE=" + (root.powerFullscreenPause ? "0" : "1"),
            "WE_HWDEC=" + root.powerHwdec,
            "WE_TOOLKIT_THEME=" + root.toolkitTheme,
            "WE_GPU=" + root.gpuTarget,
            "WE_AUDIO=" + (root.audioEnabled ? "1" : "0"),
            "WE_VOLUME=" + String(root.audioVolume),
            "WE_MULTIMONITOR=" + root.multimonitorMode
        ];
        root.status = "saving power profile...";
        powerSaveProcess.command = args;
        powerSaveProcess.running = true;
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
                root.isRunning = true;
                currentProcess.running = true;
                checkActiveProcess.running = true;
            }
        }
        stderr: StdioCollector {}
    }

    Process {
        id: checkActiveProcess
        command: ["we-wallpaper", "is-active"]
        running: false
        stdout: StdioCollector {
            onStreamFinished: root.isRunning = (this.text.trim() === "active")
        }
        stderr: StdioCollector {}
    }

    Process {
        id: controlProcess
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                root.status = this.text;
                checkActiveProcess.running = true;
                currentProcess.running = true;
            }
        }
        stderr: StdioCollector {}
    }

    Process {
        id: removeProcess
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                const out = this.text.trim();
                root.status = out !== "" ? out : "Operation completed";
                if (out.indexOf("Steam / Wallpaper Engine") !== -1) {
                    root.showNotice("Los fondos de Wallpaper Engine deben gestionarse y eliminarse desde Steam / Wallpaper Engine.", "blocked");
                } else if (out.indexOf("Borrado:") !== -1 || out.indexOf("Deleted") !== -1 || out.indexOf("deleted") !== -1) {
                    root.showNotice("Fondo eliminado correctamente", "success");
                }
                if (root.selectedId !== "") {
                    root.selectedId = "";
                    root.selectedWallpaper = null;
                    root.props = [];
                }
                refreshProcess.running = true;
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
        id: powerProcess
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                const lines = this.text.split("\n");

                for (let i = 0; i < lines.length; i++) {
                    const eq = lines[i].indexOf("=");

                    if (eq === -1) {
                        continue;
                    }

                    const k = lines[i].slice(0, eq).trim();
                    const v = lines[i].slice(eq + 1).trim();

                    if (k === "WE_PROFILE") {
                        root.powerProfile = (v === "lite" || v === "balanced" || v === "full") ? v : "";
                    } else if (k === "WE_FPS") {
                        const n = parseInt(v, 10);
                        root.powerAcFps = isNaN(n) ? 0 : n;
                    } else if (k === "WE_FPS_BATTERY") {
                        const n = parseInt(v, 10);
                        root.powerBattFps = isNaN(n) ? 10 : n;
                    } else if (k === "WE_PAUSE_ON_BATTERY") {
                        root.powerPauseOnBatt = (v === "1");
                    } else if (k === "WE_IDLE_PAUSE") {
                        const n = parseInt(v, 10);
                        root.powerIdleMin = isNaN(n) ? 0 : n;
                    } else if (k === "WE_BACKEND") {
                        root.powerBackendOverride = (v === "sway" || v === "x11") ? v : "";
                    } else if (k === "WE_LAYER") {
                        root.powerLayer = (v === "background" || v === "top" || v === "overlay") ? v : "bottom";
                    } else if (k === "WE_PAUSE_ACTIVE_ONLY") {
                        root.powerPauseActiveOnly = (v === "1");
                    } else if (k === "WE_PAUSE_IGNORE_APPID") {
                        root.powerIgnoreAppid = v;
                    } else if (k === "WE_NO_FULLSCREEN_PAUSE") {
                        root.powerFullscreenPause = (v !== "1");
                    } else if (k === "WE_HWDEC") {
                        root.powerHwdec = (v === "") ? "auto" : v;
                    } else if (k === "WE_TOOLKIT_THEME") {
                        root.toolkitTheme = (v === "gtk" || v === "qt" || v === "brave") ? v : "auto";
                    } else if (k === "WE_GPU") {
                        root.gpuTarget = (v === "intel" || v === "nvidia") ? v : "auto";
                    } else if (k === "WE_AUDIO") {
                        root.audioEnabled = (v === "1");
                    } else if (k === "WE_VOLUME") {
                        const n = parseInt(v, 10);
                        root.audioVolume = isNaN(n) ? 60 : n;
                    } else if (k === "WE_MULTIMONITOR") {
                        root.multimonitorMode = (v === "independent" || v === "single") ? v : "clone";
                    }
                }

                root.powerLoaded = true;
            }
        }
        stderr: StdioCollector {}
    }

    Process {
        id: backendProcess
        running: false
        stdout: StdioCollector {
            onStreamFinished: root.backendDetected = this.text.trim()
        }
        stderr: StdioCollector {}
    }

    Process {
        id: powerSaveProcess
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                root.status = this.text.split("\n")[0];
                // flags only take effect on (re)launch: reapply current
                if (root.currentId !== "") {
                    applyProcess.command = ["we-wallpaper", "apply-id", root.currentId];
                    applyProcess.running = true;
                } else {
                    refreshProcess.running = true;
                }
            }
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
                        saved: cols.length > 4 && cols[4] !== "",
                        min: cols.length > 5 && cols[5] !== "" ? parseFloat(cols[5]) : NaN,
                        max: cols.length > 6 && cols[6] !== "" ? parseFloat(cols[6]) : NaN,
                        step: cols.length > 7 && cols[7] !== "" ? parseFloat(cols[7]) : NaN
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
        id: resetPropsProcess
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
        id: screensProcess
        command: ["we-wallpaper", "screens-json"]
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                try {
                    const parsed = JSON.parse(this.text.trim());
                    if (Array.isArray(parsed)) {
                        root.screens = parsed;
                        if (!root.selectedScreen || root.selectedScreen === "") {
                            for (let i = 0; i < parsed.length; i++) {
                                if (parsed[i].primary) {
                                    root.selectedScreen = parsed[i].name;
                                    break;
                                }
                            }
                            if (!root.selectedScreen && parsed.length > 0) {
                                root.selectedScreen = parsed[0].name;
                            }
                        }
                    }
                } catch (e) {
                    // Ignorado si no hay JSON
                }
            }
        }
        stderr: StdioCollector {}
    }

    Process {
        id: multimonitorProcess
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                root.status = this.text;
                root.loadScreens();
            }
        }
        stderr: StdioCollector {}
    }

    Process {
        id: applyScreenProcess
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                root.status = this.text;
                root.isRunning = true;
                root.loadScreens();
            }
        }
        stderr: StdioCollector {}
    }

    Process {
        id: volumeProcess
        running: false
        stdout: StdioCollector {
            onStreamFinished: root.status = this.text.split("\n")[0]
        }
        stderr: StdioCollector {}
    }

    Process {
        id: muteProcess
        running: false
        stdout: StdioCollector {
            onStreamFinished: root.status = this.text.split("\n")[0]
        }
        stderr: StdioCollector {}
    }

    Process {
        id: telemetryProcess
        command: ["bash", "-c", "echo \"BAT:$(cat /sys/class/power_supply/BAT*/capacity 2>/dev/null || echo 100)% $(cat /sys/class/power_supply/BAT*/status 2>/dev/null || echo AC) | GPU:$(nvidia-smi --query-gpu=utilization.gpu,temperature.gpu --format=csv,noheader,nounits 2>/dev/null || echo auto)\""]
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                const raw = this.text.trim();
                const parts = raw.split(" | ");
                if (parts.length >= 1 && parts[0].indexOf("BAT:") === 0) {
                    root.batteryStatus = "🔋 " + parts[0].slice(4).trim();
                }
                if (parts.length >= 2 && parts[1].indexOf("GPU:") === 0) {
                    const gpuParts = parts[1].slice(4).split(",");
                    if (gpuParts.length >= 2) {
                        root.gpuInfo = "⚡ GPU " + gpuParts[0].trim() + "% · " + gpuParts[1].trim() + "°C";
                    } else {
                        root.gpuInfo = "⚡ GPU " + parts[1].slice(4).trim();
                    }
                }
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

                    let rawTitle = cols[1];
                    let res = "";
                    const m = rawTitle.match(/\[(4K|HD|2K|FHD)\]/i);
                    if (m) {
                        res = m[1].toUpperCase();
                        rawTitle = rawTitle.replace(/\[(4K|HD|2K|FHD)\]/i, "").trim();
                    }
                    let slug = cols[0];
                    if (slug.indexOf(":") !== -1) {
                        slug = slug.split(":")[1];
                    }

                    rows.push({
                        ref: cols[0],
                        slug: slug,
                        title: rawTitle,
                        resolution: res,
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
                    root.exploreError = "No results found or network error. Please retry.";
                    root.status = "explore: no results";
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
                const out = this.text.trim();
                root.status = out !== "" ? out : "Fondo instalado";
                root.showNotice("Fondo descargado e instalado correctamente", "success");
                refreshProcess.running = true;
            }
        }
        stderr: StdioCollector {}
    }

    Timer {
        id: noticeTimer
        interval: 4500
        running: false
        repeat: false
        onTriggered: root.noticeMessage = ""
    }

    Component.onCompleted: {
        checkActiveProcess.running = true;
        root.refresh();
    }
}
