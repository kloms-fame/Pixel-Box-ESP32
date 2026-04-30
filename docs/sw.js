const CACHE_NAME = 'pixel-box-v8-offline';
// 这里写死你需要离线缓存的所有文件路径
const ASSETS = [
    './',
    './index.html',
    './manifest.json',
    './css/style.css',
    './js/ble.js',
    './js/ui.js',
    './js/app.js'
];

// 安装阶段：强制预缓存所有文件
self.addEventListener('install', (e) => {
    self.skipWaiting();
    e.waitUntil(
        caches.open(CACHE_NAME).then((cache) => {
            console.log('[Service Worker] Caching all assets');
            return cache.addAll(ASSETS);
        })
    );
});

// 激活阶段：清理旧版本缓存
self.addEventListener('activate', (e) => {
    e.waitUntil(
        caches.keys().then((keyList) => {
            return Promise.all(keyList.map((key) => {
                if (key !== CACHE_NAME) return caches.delete(key);
            }));
        })
    );
});

// 抓取阶段：断网时优先从缓存读取
self.addEventListener('fetch', (e) => {
    e.respondWith(
        caches.match(e.request).then((cachedResponse) => {
            // 如果缓存里有，直接给缓存（断网也能秒开）；如果没有，再走网络请求
            return cachedResponse || fetch(e.request);
        })
    );
});