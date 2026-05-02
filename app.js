/**
 * QUASI88 WASM App Logic.
 *
 * Filesystem persistence is handled by pre.js (mounts IDBFS at /quasi88,
 * loads on startup, autosyncs every 10s). This file only handles UI:
 * ROM/disk uploads, the ready/menu/rom buttons, and the start sequence.
 */

(function() {
    var VERSION = '55';

    // Standalone (installed PWA) detection.
    //
    // Two reasons we can't just check this once at startup:
    //   - Linux Chrome shortcut launches: display-mode resolves to
    //     standalone reliably.
    //   - Omnibox "Open in app": the same page is *transferred* from
    //     a browser tab into a PWA window without reloading, so the
    //     display-mode flips after our script ran.
    //
    // We listen for display-mode media-query changes (across all the
    // app-ish modes) and re-evaluate. Detection itself is "not browser"
    // since Linux Chrome occasionally serves PWAs as minimal-ui /
    // window-controls-overlay rather than standalone.
    function detectStandalone() {
        try {
            return !window.matchMedia('(display-mode: browser)').matches
                ||  window.navigator.standalone === true;
        } catch (e) { return false; }
    }
    var standaloneInitDone = false;
    function updateStandalone() {
        var s = detectStandalone();
        var was = document.body.classList.contains('standalone');
        document.body.classList.toggle('standalone', s);
        // Snap-to-scale on a real transition into standalone (e.g.
        // omnibox "Open in app"). The initial detection is skipped
        // because setScale() at the end of this IIFE handles it.
        if (s && !was && standaloneInitDone) fitWindow();
        standaloneInitDone = true;
    }
    updateStandalone();
    ['browser', 'standalone', 'minimal-ui', 'fullscreen', 'window-controls-overlay']
        .forEach(function(mode) {
            try {
                window.matchMedia('(display-mode: ' + mode + ')')
                      .addEventListener('change', updateStandalone);
            } catch (e) {}
        });

    var settingsElement = document.getElementById('settings');
    var readyButton = document.getElementById('ready');
    var romBtn = document.getElementById('rom');
    var diskBtn = document.getElementById('file');
    var menuBtn = document.getElementById('menu');
    var pbarElement = document.getElementById('progress-bar');
    var progressContainer = document.getElementById('progress');
    var romInput = document.getElementById('rom_input');
    var diskInput = document.getElementById('disk_input');

    var runtimeReady = false;
    var currentRomTargetIndex = -1;

    var roms = [
        "N88.ROM", "N88EXT0.ROM", "N88EXT1.ROM", "N88EXT2.ROM", "N88EXT3.ROM",
        "N88N.ROM", "N88SUB.ROM", "N88KNJ1.ROM", "N88KNJ2.ROM", "N88JISHO.ROM",
        "FONT.ROM"
    ];

    // ---- Module definition ----------------------------------------------

    var Module = {
        noInitialRun: true,
        locateFile: function(path) {
            if (path.endsWith('.wasm')) return path + '?v=' + VERSION;
            return path;
        },
        print: function(text) {
            if (arguments.length > 1) text = Array.prototype.slice.call(arguments).join(' ');
            console.log(text);
        },
        canvas: (function() {
            var canvas = document.getElementById('canvas');
            if (canvas) {
                canvas.addEventListener("webglcontextlost", function(e) {
                    alert('WebGL context lost. You will need to reload the page.');
                    e.preventDefault();
                }, false);
            }
            return canvas;
        })(),
        setStatus: function(text) {
            if (!text) return;
            var m = text.match(/([^(]+)\((\d+(\.\d+)?)\/(\d+)\)/);
            if (m) updateProgress(parseFloat(m[2]) / parseFloat(m[4]));
        },
        totalDependencies: 0,
        monitorRunDependencies: function(left) {
            this.totalDependencies = Math.max(this.totalDependencies, left);
            updateProgress(left ? (this.totalDependencies - left) / this.totalDependencies : 1);
        },
        onRuntimeInitialized: function() {
            // pre.js's addRunDependency('idbfs-load') ensures IDBFS has
            // already populated /quasi88 before this fires.
            console.log("Runtime initialized; FS ready.");
            runtimeReady = true;
            checkAllRoms();
        }
    };
    window.Module = Module;

    // ---- UI helpers ------------------------------------------------------

    function updateProgress(rate) {
        if (!pbarElement) return;
        var n = Math.round(rate * 100);
        pbarElement.style.width = n + "%";
        pbarElement.setAttribute('aria-valuenow', n);
        if (rate === 1 && progressContainer)
            progressContainer.classList.remove('active', 'progress-striped');
    }

    function checkAllRoms() {
        if (!Module.FS) return false;
        var fs = Module.FS;
        var allFound = true;
        for (var i = 0; i < roms.length; i++) {
            var name = roms[i];
            var label = document.getElementById('rom' + (i + 1) + 's');
            var exists = false;
            try { exists = !!fs.findObject('/quasi88/ROM/' + name); } catch (e) {}

            if (exists) {
                if (label) label.innerText = 'found';
            } else {
                var opt = label && (label.innerText === 'optional' || label.innerText === 'found (opt)');
                if (!opt) allFound = false;
            }
        }
        if (readyButton) readyButton.disabled = !allFound;

        // Auto-skip the picker if every required ROM is already present
        // (typically when IDBFS restored them on a returning visit). Once
        // the emulator has been started this is a no-op.
        if (allFound && !emulatorStarted) {
            if (settingsElement) settingsElement.style.display = 'none';
            startEmulator();
        }
        return allFound;
    }

    // FS.writeFile triggers FS.trackingDelegate.onWriteToFile, which
    // pre.js wires to a debounced IDBFS save. Nothing else needed here.
    function writeFile(path, data) {
        if (!Module.FS) return;
        Module.FS.writeFile(path, data);
    }

    // ---- ROM buttons -----------------------------------------------------

    for (var i = 1; i <= 11; i++) {
        (function(idx) {
            var btn = document.getElementById('rom' + idx);
            if (btn) btn.onclick = function() {
                currentRomTargetIndex = idx - 1;
                romInput.click();
            };
        })(i);
    }

    if (romInput) {
        romInput.onchange = async function(e) {
            var file = e.target.files[0];
            if (!file || currentRomTargetIndex === -1) return;
            var targetName = roms[currentRomTargetIndex];
            var data = new Uint8Array(await file.arrayBuffer());
            writeFile('/quasi88/ROM/' + targetName, data);
            console.log('Saved ' + file.name + ' as ' + targetName);
            checkAllRoms();
        };
    }

    // ---- Disk button -----------------------------------------------------

    if (diskBtn) diskBtn.onclick = function() { diskInput.click(); };

    if (diskInput) {
        diskInput.onchange = async function(e) {
            var files = e.target.files;
            for (var j = 0; j < files.length; j++) {
                var file = files[j];
                var data = new Uint8Array(await file.arrayBuffer());
                writeFile('/quasi88/DISK/' + file.name, data);
                console.log('Saved disk image ' + file.name);
            }
        };
    }

    // ---- Start / ROM picker / Menu --------------------------------------

    if (readyButton) {
        readyButton.onclick = function() {
            if (settingsElement) settingsElement.style.display = 'none';
            if (runtimeReady) startEmulator();
        };
    }

    function nativeAudioRate() {
        // Match the emulator's sample rate to the browser AudioContext to
        // bypass SDL_AudioCVT resampling (which causes crackle). The valid
        // -samplefreq range is 8000..48000.
        try {
            var Ctx = window.AudioContext || window.webkitAudioContext;
            var ctx = new Ctx();
            var rate = ctx.sampleRate;
            ctx.close && ctx.close();
            if (rate >= 8000 && rate <= 48000) return rate | 0;
        } catch (e) {}
        return 48000; // most browsers default here
    }

    var emulatorStarted = false;
    function startEmulator() {
        if (emulatorStarted) return;
        if (!runtimeReady) return;
        emulatorStarted = true;
        var rate = nativeAudioRate();
        console.log("Starting emulator at " + rate + " Hz");
        var args = [
            '-romdir', '/quasi88/ROM',
            '-diskdir', '/quasi88/DISK',
            '-tapedir', '/quasi88/TAPE',
            '-samplefreq', String(rate)
        ];
        if (typeof Module.callMain === 'function') {
            Module.callMain(args);
        } else {
            console.error("callMain not available; cannot start emulator.");
            emulatorStarted = false;
        }
    }

    if (romBtn) {
        romBtn.onclick = function() {
            // Clearing the inline display lets the .modal CSS rule
            // (display: flex; align-items/justify-content: center) take
            // effect again. Setting it to 'block' would override flex
            // and pin the card to the top-left.
            if (settingsElement) settingsElement.style.display = '';
        };
    }

    if (menuBtn) {
        menuBtn.onclick = function() {
            if (!runtimeReady) return;
            // C-side helper posts the SDL_USEREVENT for us, so we don't
            // need to know SDL_Event's memory layout from JS.
            Module.ccall('quasi88_post_menu_event', null, [], []);
        };
    }

    // ---- Right-click context menu --------------------------------------

    var stageEl  = document.getElementById('stage');
    var SCALE_KEY = 'quasi88.scale';
    var currentScale = parseInt(localStorage.getItem(SCALE_KEY), 10) || 1;

    function setScale(s) {
        console.log('[scale] setScale(' + s + ')');
        currentScale = s;
        localStorage.setItem(SCALE_KEY, String(s));
        document.documentElement.style.setProperty('--scale', String(s));
        fitWindow();
    }

    // In standalone PWA, try to size the window to native canvas +
    // toolbar at the current scale. Best-effort: many browsers
    // silently deny window.resizeTo for PWA windows. The canvas CSS
    // (letterbox-fit in standalone) keeps the layout sensible either
    // way. Also called from the resize event listener for "snap back"
    // when the user/OS changes the window size.
    function fitWindow() {
        var standalone = document.body.classList.contains('standalone');
        var fs = !!document.fullscreenElement;
        console.log('[fit] standalone=' + standalone
                  + ' fullscreen=' + fs
                  + ' scale=' + currentScale
                  + ' outer=' + window.outerWidth + 'x' + window.outerHeight
                  + ' inner=' + window.innerWidth + 'x' + window.innerHeight);
        if (!standalone) { console.log('[fit] skip: not standalone'); return; }
        if (fs)          { console.log('[fit] skip: fullscreen'); return; }
        var wantInnerW = 640 * currentScale;
        var wantInnerH = 420 * currentScale + 32;
        var chromeW = window.outerWidth  - window.innerWidth;
        var chromeH = window.outerHeight - window.innerHeight;
        if (chromeW < 0) chromeW = 0;
        if (chromeH < 0) chromeH = 0;
        var wantOuterW = wantInnerW + chromeW;
        var wantOuterH = wantInnerH + chromeH;
        if (window.outerWidth === wantOuterW && window.outerHeight === wantOuterH) {
            console.log('[fit] skip: already at ' + wantOuterW + 'x' + wantOuterH);
            return;
        }
        var beforeW = window.outerWidth, beforeH = window.outerHeight;
        console.log('[fit] resizeTo(' + wantOuterW + ', ' + wantOuterH + ')');
        try { window.resizeTo(wantOuterW, wantOuterH); }
        catch (e) { console.warn('[fit] threw:', e); return; }
        setTimeout(function() {
            console.log('[fit] after 50ms: outer=' + window.outerWidth + 'x' + window.outerHeight);
            if (window.outerWidth === beforeW && window.outerHeight === beforeH) {
                console.warn('[fit] resizeTo BLOCKED by browser (no change)');
            }
        }, 50);
    }

    // Window/chrome dimensions aren't always known on first paint.
    // Fire one round at load and another after layout settles.
    if (document.readyState === 'complete') fitWindow();
    else window.addEventListener('load', fitWindow, { once: true });
    setTimeout(fitWindow, 200);

    // Snap back if the OS / user resizes. Debounced so we don't fight
    // every intermediate event during a drag-resize.
    var resizeTimer = 0;
    window.addEventListener('resize', function() {
        console.log('[resize] event fired; outer=' + window.outerWidth + 'x' + window.outerHeight);
        clearTimeout(resizeTimer);
        resizeTimer = setTimeout(fitWindow, 100);
    });

    function toggleFullscreen() {
        if (document.fullscreenElement) {
            document.exitFullscreen && document.exitFullscreen();
        } else if (stageEl.requestFullscreen) {
            stageEl.requestFullscreen().catch(function(err) {
                console.warn('Fullscreen rejected:', err);
            });
        }
    }

    var ctxMenu = null;
    function buildContextMenu() {
        ctxMenu = document.createElement('div');
        ctxMenu.id = 'context-menu';
        document.body.appendChild(ctxMenu);
        ctxMenu.addEventListener('click', function(e) {
            var t = e.target.closest('.item');
            console.log('[menu] click target=', e.target, 'matched=', t && t.dataset.act);
            if (!t) return;
            switch (t.dataset.act) {
                case 'scale1': setScale(1); break;
                case 'scale2': setScale(2); break;
                case 'fs':     toggleFullscreen(); break;
            }
            hideContextMenu();
        });
    }
    function rebuildContextMenuItems() {
        var fs = !!document.fullscreenElement;
        ctxMenu.innerHTML =
            '<div class="item' + (currentScale === 1 ? ' checked' : '') + '" data-act="scale1">画面サイズ 1x</div>' +
            '<div class="item' + (currentScale === 2 ? ' checked' : '') + '" data-act="scale2">画面サイズ 2x</div>' +
            '<div class="sep"></div>' +
            '<div class="item' + (fs ? ' checked' : '') + '" data-act="fs">フルスクリーン</div>';
    }
    function showContextMenu(x, y) {
        if (!ctxMenu) buildContextMenu();
        rebuildContextMenuItems();
        // Tentatively place at click point, then nudge if it overflows.
        ctxMenu.style.left = x + 'px';
        ctxMenu.style.top  = y + 'px';
        ctxMenu.classList.add('open');
        var r = ctxMenu.getBoundingClientRect();
        if (r.right > window.innerWidth)  ctxMenu.style.left = (window.innerWidth  - r.width  - 4) + 'px';
        if (r.bottom > window.innerHeight) ctxMenu.style.top  = (window.innerHeight - r.height - 4) + 'px';
    }
    function hideContextMenu() {
        if (ctxMenu) ctxMenu.classList.remove('open');
    }

    if (stageEl) {
        // Eat the right-mouse-down before SDL sees it; otherwise
        // pc88_mouse(KEY88_MOUSE_R, 1) fires with no matching release.
        stageEl.addEventListener('mousedown', function(e) {
            if (e.button === 2) e.stopImmediatePropagation();
        }, true);
        stageEl.addEventListener('contextmenu', function(e) {
            e.preventDefault();
            showContextMenu(e.clientX, e.clientY);
        });
    }
    document.addEventListener('mousedown', function(e) {
        if (ctxMenu && !ctxMenu.contains(e.target)) hideContextMenu();
    });
    document.addEventListener('keydown', function(e) {
        if (e.key === 'Escape') hideContextMenu();
    });

    // Apply persisted scale on load. Always — this sets the CSS
    // variable and triggers the initial standalone fitWindow.
    setScale(currentScale);

    window.onerror = function(msg, url, line, col, error) {
        console.error("Window Error:", msg, error);
        updateProgress(0);
    };
})();
