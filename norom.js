/**
 * QUASI88 WASM Minimal Compatible ROM Generator (rom=no mode).
 *
 * Synthesizes clean-room PC-8801mkIISR ROM images in MEMFS without requiring
 * original NEC ROM dumps:
 *   - N88.ROM     (32KB): Minimal IPL boot stub (T0/S1 -> 0xC000, ROM_VERSION='8')
 *   - N88SUB.ROM  (2KB) : Minimal FDD Sub-CPU uPD765A + 8255 PPI firmware (0x00/0x02/0x03/0x06)
 *   - N88KNJ1.ROM (128KB) & N88KNJ2.ROM (128KB): JIS X 0208 16x16 rendered from WebFont
 *   - FONT.ROM    (2KB) : 8x8 ANK rendered from WebFont
 */
(function() {
    // Assembled from src/WASM/rom/n88_stub.s (223 bytes)
    var N88_STUB_B64 =
        '8zHwvz4x0zA+OdMxPqDTMq/TNdNT018+kdP/r9P+r9NRPs7TUD6Y01A+b9NQPljTUD5T01A+gNNRr9NQ' +
        '01DTaD7I02Q+89NkPrfTZT5L02U+5NNoPiDTUT5D01GvzYsAPgLNiwA+Ac2dAK/NnQCvzZ0APgHNnQA+' +
        'A82LACEAwAYAzb4AdyMQ+cMAwPU+D9P/2/7mBv4CIPg+DtP/8fXb/uYG/gIg+PHT/T4J0//b/uYEKPo+' +
        'CNP/2/7mBCD6yT4L0//b/uYBKPo+CtP/2/z1Pg3T/9v+5gEg+j4M0//xyQ==';

    // Assembled from src/WASM/rom/n88sub_stub.s (447 bytes)
    var N88SUB_STUB_B64 =
        '8+1WMf9/wzkA///////////////////////////////////////////////////////////////JPpHT' +
        '/6/T/jIAUDIEUM1WAdv+y18o+s19AbcoD/4CKBz+A8o8Af4GKAsY5c1WAa8yBFAY3DoEUM2eARjUzX0B' +
        'MgBQzX0B5gMyAVDNfQEyAlDNfQEyA1AhAGA6AFC3KLFHxeU6AlBPyz9XeeYBXwcHTzoBULFPPg/NZQF5' +
        'zWUBes1lAT4IzWUBzXIB/oAo9M1yAT5GzWUBec1lAXrNZQF7zWUBOgNQzWUBPgHNZQE6A1DNZQE+G81l' +
        'AT7/zWUB4Q4A2/rLfyj6y28oCdv7dyMNIO/b+M1yAebAMgRQBgbNcgEQ+zoDUDz+ETgOPgEyA1A6AlA8' +
        'MgJQGAMyA1DBBcKZAMNJACEAYDoAULfKSQBHDgB+I82eAQ0g+BD0w0kAPgPNZQE+381lAT4Dw2UB9dv6' +
        '5sD+gCD48dP7ydv65sD+wCD42/vJPgvT/9v+5gEo+j4K0//b/PU+DdP/2/7mASD6PgzT//HJ9dv+5gb+' +
        'AiD48dP9PgnT/9v+5gQo+j4I0//b/uYEIPrJ';

    var WEBFONT_FAMILY = 'DotGothic16';
    var WEBFONT_CSS_URL = 'https://fonts.googleapis.com/css2?family=DotGothic16&display=swap';
    var FONT_STACK_16 = '"' + WEBFONT_FAMILY + '", "MS Gothic", "BIZ UDGothic", "Noto Sans Mono CJK JP", monospace';

    function decodeBase64(b64) {
        var bin = atob(b64);
        var out = new Uint8Array(bin.length);
        for (var i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i);
        return out;
    }

    function ensureWebFont() {
        return new Promise(function(resolve) {
            if (!document.fonts) { resolve(false); return; }
            var id = 'quasi88-norom-webfont';
            if (!document.getElementById(id)) {
                var link = document.createElement('link');
                link.id = id;
                link.rel = 'stylesheet';
                link.href = WEBFONT_CSS_URL;
                document.head.appendChild(link);
            }
            var done = false;
            var finish = function(ok) {
                if (!done) { done = true; resolve(ok); }
            };
            // Wait up to 2.5s for WebFont; fall back cleanly if offline.
            setTimeout(function() { finish(false); }, 2500);
            Promise.all([
                document.fonts.load('16px "' + WEBFONT_FAMILY + '"', '亜あアＡ0'),
                document.fonts.ready
            ]).then(function() { finish(true); }, function() { finish(false); });
        });
    }

    function buildN88Rom() {
        var rom = new Uint8Array(0x8000);
        rom.fill(0xFF);
        rom.set(decodeBase64(N88_STUB_B64), 0x0000);
        // ROM_VERSION at 0x79D7: '8' (PC-8801mkIISR/FH/MH V2)
        rom[0x79D7] = 0x38;
        return rom;
    }

    function buildN88SubRom() {
        var rom = new Uint8Array(0x0800);
        rom.fill(0xFF);
        rom.set(decodeBase64(N88SUB_STUB_B64), 0x0000);
        return rom;
    }

    // Map JIS X 0208 (hi, lo) in 0x21..0x7E to PC-88 Kanji ROM (bank, wordAddr).
    function jisToPc88(hi, lo) {
        if (hi >= 0x21 && hi <= 0x27) {
            return { bank: 0, w: ((lo & 0x60) << 7) | ((hi & 0x07) << 9) | ((lo & 0x1F) << 4) };
        }
        if (hi >= 0x30 && hi <= 0x4F) {
            return { bank: 0, w: ((lo & 0x60) << 9) | ((hi & 0x1F) << 9) | ((lo & 0x1F) << 4) };
        }
        if (hi >= 0x50 && hi <= 0x6F) {
            return { bank: 1, w: ((lo & 0x60) << 9) | (((hi - 0x50) & 0x1F) << 9) | ((lo & 0x1F) << 4) };
        }
        if (hi >= 0x70 && hi <= 0x74) {
            return { bank: 1, w: ((lo & 0x60) << 7) | ((hi & 0x07) << 9) | ((lo & 0x1F) << 4) };
        }
        return null;
    }

    function buildKanjiRoms() {
        var knj1 = new Uint8Array(0x20000);
        var knj2 = new Uint8Array(0x20000);
        var banks = [knj1, knj2];

        var dec;
        try {
            dec = new TextDecoder('euc-jp');
        } catch (e) {
            console.warn('[norom] TextDecoder(euc-jp) unavailable:', e);
            return { knj1: knj1, knj2: knj2 };
        }

        // Collect all valid JIS X 0208 glyphs covered by PC-88 Kanji ROM 1 & 2.
        var glyphs = [];
        var pair = new Uint8Array(2);
        var ranges = [[0x21, 0x27], [0x30, 0x4F], [0x50, 0x6F], [0x70, 0x74]];
        for (var r = 0; r < ranges.length; r++) {
            for (var hi = ranges[r][0]; hi <= ranges[r][1]; hi++) {
                pair[0] = hi | 0x80;
                for (var lo = 0x21; lo <= 0x7E; lo++) {
                    pair[1] = lo | 0x80;
                    var ch = dec.decode(pair);
                    if (!ch || ch === '\uFFFD') continue;
                    var map = jisToPc88(hi, lo);
                    if (map) glyphs.push({ ch: ch, bank: map.bank, w: map.w });
                }
            }
        }

        // Batch-render onto a 1024x1024 offscreen canvas (64x64 = 4096 glyphs per pass).
        var COLS = 64;
        var ROWS = 64;
        var CELL = 16;
        var BATCH = COLS * ROWS;
        var canvas = document.createElement('canvas');
        canvas.width = COLS * CELL;
        canvas.height = ROWS * CELL;
        var ctx = canvas.getContext('2d', { willReadFrequently: true });

        for (var start = 0; start < glyphs.length; start += BATCH) {
            var end = Math.min(start + BATCH, glyphs.length);
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            ctx.fillStyle = '#ffffff';
            ctx.font = '16px ' + FONT_STACK_16;
            ctx.textAlign = 'left';
            ctx.textBaseline = 'top';

            for (var idx = start; idx < end; idx++) {
                var slot = idx - start;
                var gx = (slot % COLS) * CELL;
                var gy = ((slot / COLS) | 0) * CELL;
                ctx.fillText(glyphs[idx].ch, gx, gy);
            }

            var img = ctx.getImageData(0, 0, canvas.width, canvas.height).data;
            var stride = canvas.width * 4;

            for (var j = start; j < end; j++) {
                var g = glyphs[j];
                var s = j - start;
                var ox = (s % COLS) * CELL;
                var oy = ((s / COLS) | 0) * CELL;
                var rom = banks[g.bank];
                var baseByte = g.w << 1;

                for (var y = 0; y < 16; y++) {
                    var rowOff = (oy + y) * stride + ox * 4;
                    var bLeft = 0;
                    var bRight = 0;
                    for (var x = 0; x < 8; x++) {
                        if (img[rowOff + x * 4 + 3] >= 96) bLeft |= (0x80 >> x);
                    }
                    for (var x2 = 0; x2 < 8; x2++) {
                        if (img[rowOff + (x2 + 8) * 4 + 3] >= 96) bRight |= (0x80 >> x2);
                    }
                    rom[baseByte + (y << 1)]     = bLeft;
                    rom[baseByte + (y << 1) + 1] = bRight;
                }
            }
        }

        return { knj1: knj1, knj2: knj2 };
    }

    // Generate 2KB ANK FONT.ROM (256 x 8 bytes) from WebFont.
    // DotGothic16 half-width glyphs sit natively on an 8x16 grid; sampling
    // 8 representative scanlines yields crisp, readable 8x8 TVRAM glyphs.
    function buildAnkFontRom() {
        var rom = new Uint8Array(256 * 8);
        var canvas = document.createElement('canvas');
        canvas.width = 256 * 8;
        canvas.height = 16;
        var ctx = canvas.getContext('2d', { willReadFrequently: true });
        ctx.clearRect(0, 0, canvas.width, canvas.height);
        ctx.fillStyle = '#ffffff';
        ctx.font = '16px ' + FONT_STACK_16;
        ctx.textAlign = 'left';
        ctx.textBaseline = 'top';

        var sjisDec = null;
        try { sjisDec = new TextDecoder('shift_jis'); } catch (e) {}

        for (var code = 0x20; code <= 0x7E; code++) {
            ctx.fillText(String.fromCharCode(code), code * 8, 0);
        }
        if (sjisDec) {
            var b = new Uint8Array(1);
            for (var k = 0xA1; k <= 0xDF; k++) {
                b[0] = k;
                var ch = sjisDec.decode(b);
                if (ch && ch !== '\uFFFD') ctx.fillText(ch, k * 8, 0);
            }
        }

        var img = ctx.getImageData(0, 0, canvas.width, canvas.height).data;
        var stride = canvas.width * 4;
        // Sample 8 scanlines out of the 16px tall half-width cell (rows 1..14)
        var SAMPLE_Y = [1, 3, 5, 7, 9, 11, 13, 14];

        for (var c = 0x20; c < 0xE0; c++) {
            if (c >= 0x7F && c <= 0xA0) continue;
            var ox = c * 8;
            for (var r = 0; r < 8; r++) {
                var rowOff = SAMPLE_Y[r] * stride + ox * 4;
                var bits = 0;
                for (var x = 0; x < 8; x++) {
                    if (img[rowOff + x * 4 + 3] >= 96) bits |= (0x80 >> x);
                }
                rom[c * 8 + r] = bits;
            }
        }
        return rom;
    }

    function mkdirp(FS, path) {
        var parts = path.split('/').filter(Boolean);
        var cur = '';
        for (var i = 0; i < parts.length; i++) {
            cur += '/' + parts[i];
            try { FS.mkdir(cur); }
            catch (e) { if (e.errno !== 20) throw e; }
        }
    }

    async function install(FS, romDir) {
        var t0 = performance.now();
        var loadedWebFont = await ensureWebFont();
        mkdirp(FS, romDir);

        var n88 = buildN88Rom();
        var sub = buildN88SubRom();
        var kanji = buildKanjiRoms();
        var ank = buildAnkFontRom();

        // Also mirror ANK into N88KNJ1.ROM offset 0x1000 (kanji_rom[0][0x0800])
        kanji.knj1.set(ank, 0x1000);

        FS.writeFile(romDir + '/N88.ROM', n88);
        FS.writeFile(romDir + '/N88SUB.ROM', sub);
        FS.writeFile(romDir + '/N88KNJ1.ROM', kanji.knj1);
        FS.writeFile(romDir + '/N88KNJ2.ROM', kanji.knj2);
        FS.writeFile(romDir + '/FONT.ROM', ank);

        console.log('[norom] Synthesized compatible ROMs in ' +
            Math.round(performance.now() - t0) + 'ms (WebFont=' + loadedWebFont + ')');
    }

    window.Quasi88NoRom = {
        install: install,
        mkdirp: mkdirp
    };
})();
