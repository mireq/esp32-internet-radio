self.addEventListener('install', function(e) {
	//self.skipWaiting();
	e.waitUntil(
		caches.open('esp-radio').then(function(cache) {
			return cache.addAll([
				'/',
				'/index.html',
				'/app.js',
				'/utils.js',
				'/style.css',
			]);
		})
	);
});

self.addEventListener('activate', function(e) {
});


/*
self.addEventListener('fetch', function(e) {
	e.respondWith(
		caches.match(e.request).then(function(response) {
			return response || fetch(e.request);
		})
	);
});
*/
