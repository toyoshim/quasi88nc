// Bump CACHE_NAME (or its suffix) whenever app.js / quasi88.js / quasi88.wasm
// change so old clients pick up the new build. Asset URLs are cached with
// the same ?v= query string used by index.html and app.js's locateFile().
const VERSION = '55';
// style.css is now part of the cached set; bumping VERSION evicts the old.
const CACHE_NAME = 'quasi88-v' + VERSION;
const ASSETS = [
    'index.html',
    'manifest.json',
    'style.css?v=' + VERSION,
    'app.js?v=' + VERSION,
    'quasi88.js?v=' + VERSION,
    'quasi88.wasm?v=' + VERSION
];

self.addEventListener('install', (e) => {
    e.waitUntil(
        caches.open(CACHE_NAME).then((cache) => cache.addAll(ASSETS))
    );
});

self.addEventListener('activate', (e) => {
    // Evict old quasi88-v* caches so the new build isn't shadowed.
    e.waitUntil(
        caches.keys().then((keys) => Promise.all(
            keys.filter((k) => k.startsWith('quasi88-v') && k !== CACHE_NAME)
                .map((k) => caches.delete(k))
        ))
    );
});

self.addEventListener('fetch', (e) => {
    e.respondWith(
        caches.match(e.request).then((response) => response || fetch(e.request))
    );
});
