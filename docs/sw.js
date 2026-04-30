// ⚠️ 每次你的 UI 或 JS 有大修改时，只要把这里的版本号 +1，就能强制所有用户的手机更新
const CACHE_NAME = 'pixel-box-v16';

const ASSETS = [
    './',
    './index.html',
    './manifest.json',
    './css/style.css',
    './js/ble.js',
    './js/ui.js',
    './js/controllers.js',
    './js/app.js'
];

self.addEventListener('install', (e) => {
    self.skipWaiting();
    e.waitUntil(
        caches.open(CACHE_NAME).then((cache) => cache.addAll(ASSETS))
    );
});

// 激活新版本时，自动清理旧版本的垃圾缓存
self.addEventListener('activate', (e) => {
    e.waitUntil(
        caches.keys().then((keyList) => {
            return Promise.all(keyList.map((key) => {
                if (key !== CACHE_NAME) return caches.delete(key);
            }));
        })
    );
    return self.clients.claim();
});

// 【核心修复】：改为“网络优先 (Network First)”策略
self.addEventListener('fetch', (e) => {
    e.respondWith(
        fetch(e.request)
            .then((response) => {
                // 1. 如果手机有网，直接从 Github 获取最新代码，并悄悄更新到本地缓存
                if (response && response.status === 200) {
                    const responseClone = response.clone();
                    caches.open(CACHE_NAME).then((cache) => cache.put(e.request, responseClone));
                }
                return response;
            })
            .catch(() => {
                // 2. 如果手机断网了 (fetch 失败)，则瞬间回退使用上一次存好的本地缓存！
                return caches.match(e.request);
            })
    );
});