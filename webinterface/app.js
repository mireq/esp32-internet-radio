(function() {
'use strict';

if (window.indexedDB === undefined) {
	alert("Browser does not suport indexedDB");
}


var wsAddress = _.getQueryParameters().device;
var storage;
var api;


var COMMAND_PING = 0;
var COMMAND_SET_PLAYLIST_ITEM = 1;
var COMMAND_SET_VOLUME = 2;

var RESPONSE_PONG = 0;
var RESPONSE_STATUS = 1;

var STATUS_VOLUME = 0;


function getSettings(key, onSuccess) {
	if (storage === undefined) {
		onSuccess(undefined);
	}
	var transaction = storage.transaction('settings', 'readonly');
	var settingsStore = transaction.objectStore('settings');
	var req = settingsStore.get(key);
	req.onsuccess = function(event) {
		var result = event.target.result;
		if (result === undefined) {
			onSuccess(undefined);
		}
		else {
			onSuccess(event.target.result.value);
		}
	};
}

function updateSettings(key, value) {
	if (storage === undefined) {
		return;
	}
	var transaction = storage.transaction('settings', 'readwrite');
	var settingsStore = transaction.objectStore('settings');
	settingsStore.put({id: key, value: value});
}


function Signal() {
	var self = this;
	this.listeners = [];

	this.connect = function(callback) {
		self.listeners.push(callback);
	};

	this.disconnect = function(callback) {
		if (callback === undefined) {
			self.listeners = [];
			return;
		}

		var pos = self.listeners.indexOf(callback);
		if (pos === -1) {
			console.log('Not connected', callback);
			return;
		}
		self.listeners.splice(pos, 1);
	};

	this.send = function() {
		var sendArguments = arguments;
		Array.prototype.forEach.call(self.listeners, function(listener) {
			listener.apply(this, sendArguments);
		});
	};
}


function Connection(socket_url) {
	var self = this;

	var conn;

	var signals = {
		open: new Signal(),
		close: new Signal(),
		messageReceived: new Signal(),
		statusChanged: new Signal()
	};

	var onOpen = function (event) {
		signals.open.send();
		if (self.status != 'connected') {
			self.status = 'connected';
			signals.statusChanged.send(self.status);
		}
	};

	var onMessage = function (event) {
		signals.messageReceived.send(event);
	};

	var onClose = function (event) {
		if (self.status != 'disconnected') {
			self.status = 'disconnected';
			signals.statusChanged.send(self.status);
		}
		signals.close.send();
		self.close();
	};

	var onError = function (event) {
		self.close();
	};

	this.status = 'disconnect';

	this.connect = function() {
		self.status = 'connecting';
		signals.statusChanged.send(self.status);

		if (conn !== undefined) {
			self.close();
		}
		conn = new WebSocket(socket_url);

		conn.addEventListener('open', onOpen);
		conn.addEventListener('message', onMessage);
		conn.addEventListener('close', onClose);
		conn.addEventListener('error', onError);
	};

	this.send = function(data) {
		conn.send(data);
	};

	this.close = function() {
		if (self.status == 'connected') {
			self.status = 'disconnected';
			signals.statusChanged.send(self.status);
			signals.close.send();
		}
		if (conn !== undefined) {
			if (conn.readyState === WebSocket.OPEN) {
				conn.close();
			}
			conn.removeEventListener('open', onOpen);
			conn.removeEventListener('message', onMessage);
			conn.removeEventListener('close', onClose);
			conn.removeEventListener('error', onError);
			conn = undefined;
		}
	};

	this.signals = signals;
}


function Api(socket_url) {
	var self = this;

	var pingTimer;
	var pongReceived = false;
	var oldStatus = 'disconnected';

	var volume = 0;

	var signals = {
		connected: new Signal(),
		disconnected: new Signal(),
		volumeChanged: new Signal(),
	};

	function checkPong() {
		if (pongReceived) {
			pongReceived = false;
			self.sendCommand(COMMAND_PING);
			pingTimer = setTimeout(checkPong, 1000);
		}
		else {
			self.connection.close();
			pingTimer = undefined;
		}
	}

	this.connection = new Connection(socket_url);

	this.sendCommand = function(commandNr, data) {
		if (data === undefined) {
			data = new Uint8Array(0);
		}
		else {
			data = new Uint8Array(data);
		}
		var buf = new ArrayBuffer(data.byteLength + 2);
		var commandView = new DataView(buf, 0, 2);
		commandView.setUint16(0, commandNr);
		var dataView = new Uint8Array(buf, 2, data.byteLength);
		dataView.set(new Uint8Array(data), 0);
		self.connection.send(buf);
	};

	this.receiveCommand = function(commandNr, data) {
		switch (commandNr) {
			case RESPONSE_PONG:
				pongReceived = true;
				break;
			case RESPONSE_STATUS:
				self.processStatus(data);
				break;
			default:
				console.log("Unknown command", commandNr);
				break;
		}
	};

	this.connection.signals.close.connect(function() {
		setTimeout(function() { self.connection.connect(); }, 1000);
	});

	this.connection.signals.statusChanged.connect(function(status) {
		if (status === 'connected' && pingTimer === undefined) {
			self.sendCommand(COMMAND_PING);
			if (pingTimer !== undefined) {
				clearTimeout(pingTimer);
				pingTimer = undefined;
			}
			pingTimer = setTimeout(checkPong, 1000);
		}
		if (status !== 'connected' && pingTimer !== undefined) {
			clearTimeout(pingTimer);
			pingTimer = undefined;
		}

		if (status === 'connected' && oldStatus !== 'connected') {
			oldStatus = 'connected';
			signals.connected.send();
		}
		else if (status !== 'connected' && oldStatus === 'connected') {
			oldStatus = 'disconnected';
			signals.disconnected.send();
		}
	});

	this.connection.signals.messageReceived.connect(function(event) {
		event.data.arrayBuffer().then(function(buf) {
			var commandView = new DataView(buf, 0, 2);
			var commandNr = commandView.getUint16(0);
			var data = buf.slice(2);
			self.receiveCommand(commandNr, data);
		});
	});

	this.setPlaylistItem = function(uri, text) {
		var uriArray = new TextEncoder().encode(uri);
		var textArray = new TextEncoder().encode(text);

		var buf = new ArrayBuffer(uriArray.byteLength + textArray.byteLength + 2);
		var bufferView = new Uint8Array(buf);

		bufferView.set(uriArray, 0);
		bufferView.set(textArray, uriArray.byteLength + 1);

		this.sendCommand(COMMAND_SET_PLAYLIST_ITEM, buf);
	};

	this.setVolume = function(volume) {
		var buf = new ArrayBuffer(2);
		var volumeView = new DataView(buf, 0, 2);
		volumeView.setUint16(0, volume);
		this.sendCommand(COMMAND_SET_VOLUME, buf);
	};

	this.processStatus = function(data) {
		while (data.byteLength) {
			var cmd = new Uint8Array(data)[0];
			data = data.slice(1);
			switch (cmd) {
				case STATUS_VOLUME:
					data = data.slice(self.processStatusVolume(data));
					break;
			}
		}
	};

	this.processStatusVolume = function(data) {
		var dataView = new DataView(data, 0, 2);
		var volume = dataView.getUint16(0);
		if (self.volume !== volume) {
			self.volume = volume;
			self.signals.volumeChanged.send(self.volume);
		}
		return 2;
	};

	this.connection.connect();

	this.signals = signals;
}


function onPlaylistClick(e) {
	var target = e.target;
	if (target.tagName.toLowerCase() !== "a") {
		return;
	}

	e.preventDefault();
	api.setPlaylistItem(target.getAttribute('href'), target.textContent);

}


function onDisconnectClicked(event) {
	updateSettings('address', '');
	if (api !== undefined) {
		api.connection.close();
		api = undefined;
	}
	document.body.className = 'login';
}


function onClick(e) {
	if (e.which !== 1) {
		return;
	}
	var target = e.target;
	if (_.closest(target, '.playlist') !== null) {
		onPlaylistClick(e);
	}
	if (_.closest(target, '.disconnect') !== null) {
		onDisconnectClicked(e);
	}
}


function initSliders() {
	_.toArray(document.getElementsByClassName('slider')).forEach(function(element) {
		var opts = {};
		var min = 0;
		var max = 100;
		var input;
		opts.start = 0;
		opts.range = {min: 0, max: 100};
		opts.pips = {
			mode: 'count',
			values: 5,
			density: 5
		};
		noUiSlider.create(element, opts);

		if (element.dataset.input !== undefined) {
			input = document.getElementById(element.dataset.input);
			input.style.display = 'none';
			if (input.hasAttribute('min')) {
				min = parseFloat(input.getAttribute('min'), 10);
			}
			if (input.hasAttribute('max')) {
				max = parseFloat(input.getAttribute('max'), 10);
			}
		}

		var ticks = element.querySelectorAll('.noUi-value');
		function onSliderClicked(event) {
			if (event.which !== 1) {
				return;
			}
			var tick = event.target;
			var value = parseFloat(tick.getAttribute('data-value'), 10);
			if (isNaN(value)) {
				return;
			}
			element.noUiSlider.set(value);
		}

		function onUpdate(values, handle) {
			var value = (parseFloat(values[0]) / 100) * (max - min) + min;
			if (input !== undefined) {
				input.value = parseInt(value, 10);
				var event = new Event('change');
				input.dispatchEvent(event);
			}
		}

		element.addEventListener('click', onSliderClicked);
		element.noUiSlider.on('update', _.debounce(onUpdate, 100));
		element.noUiSlider.on('start', function() {
			element.noUiSlider.dragging = true;
		});
		element.noUiSlider.on('end', function() {
			element.noUiSlider.dragging = undefined;
		});
	});
}


document.body.addEventListener('click', onClick);
initSliders();


if (window.location.protocol === 'https:' && 'serviceWorker' in navigator) {
	navigator.serviceWorker
		.register('./sw.js')
		.then(function(reg) { reg.update(); });
}


function startApp() {
	if (!wsAddress) {
		document.body.className = 'login';
		return;
	}
	if (window.location.protocol === 'https:') {
		wsAddress = 'wss://' + wsAddress;
	}
	else {
		wsAddress = 'ws://' + wsAddress;
	}
	document.body.className = 'connecting';
	api = new Api(wsAddress);
	var volumeInput = document.getElementById('input_volume');

	api.signals.connected.connect(function() {
		document.body.className = 'player';
	});
	api.signals.disconnected.connect(function() {
		document.body.className = 'connecting';
	});
	api.signals.volumeChanged.connect(function(volume) {
		var element = document.getElementById('volume_slider');
		if (!element.noUiSlider.dragging) {
			element.noUiSlider.set(100 * api.volume / 65535.0);
		}
	});
	volumeInput.addEventListener('change', function() {
		if (api !== undefined && api.connection.status === 'connected') {
			api.setVolume(volumeInput.value);
		}
	});
}


document.getElementById('page_login').setAttribute('action', window.location);


(function() {
	var dbRequest = indexedDB.open('esp32-radio', 1);
	dbRequest.onupgradeneeded = function(event) {
		var db = event.target.result;
		var store = db.createObjectStore('settings', {keyPath: 'id'});
	};
	dbRequest.onsuccess = function(event) {
		storage = event.target.result;
		if (wsAddress) {
			updateSettings('address', wsAddress);
			startApp();
		}
		else {
			getSettings('address', function(value) {
				wsAddress = value;
				startApp();
			});
		}
	};
	dbRequest.onerror = function(event) {
		startApp();
	};
}());


window.onbeforeunload = function() {
	if (api !== undefined) {
		api.connection.close();
	}
};


}());
