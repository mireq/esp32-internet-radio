(function() {


var COMMAND_PING = 0;
var COMMAND_SET_PLAYLIST_ITEM = 1;

var RESPONSE_PONG = 0;


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

	this.status = 'disconnect';

	this.connect = function() {
		self.status = 'connecting';
		signals.statusChanged.send(self.status);

		conn = new WebSocket(socket_url);

		conn.addEventListener('open', function (event) {
			signals.open.send();
			if (self.status != 'connected') {
				self.status = 'connected';
				signals.statusChanged.send(self.status);
			}
		});
		conn.addEventListener('message', function (event) {
			signals.messageReceived.send(event);
		});
		conn.addEventListener('close', function (event) {
			if (self.status != 'disconnected') {
				self.status = 'disconnected';
				signals.statusChanged.send(self.status);
			}
			signals.close.send();
			self.close();
		});
		conn.addEventListener('error', function (event) {
			console.log("Error, closing connection", event);
			self.close();
		});
	};

	this.send = function(data) {
		console.log("send", data);
		conn.send(data);
	};

	this.close = function() {
		if (self.status == 'connected' && conn !== undefined) {
			conn.close();
			conn = undefined;
			self.status = 'disconnected';
			signals.statusChanged.send(self.status);
			signals.close.send();
		}
	};

	this.signals = signals;
}


function Api(socket_url) {
	var self = this;

	var pingTimer;
	var pongReceived = false;

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
		var commandView = new Uint16Array(buf, 0, 1);
		commandView[0] = commandNr;
		var dataView = new Uint8Array(buf, 2, data.byteLength);
		dataView.set(new Uint8Array(data), 0);
		self.connection.send(buf);
	};

	this.receiveCommand = function(commandNr, data) {
		switch (commandNr) {
			case RESPONSE_PONG:
				pongReceived = true;
				break;
			default:
				break;
		}
	};

	this.connection.signals.close.connect(function() {
		self.connection.connect();
	});

	this.connection.signals.statusChanged.connect(function(status) {
		console.log(status);
		if (status === 'connected' && pingTimer === undefined) {
			self.sendCommand(COMMAND_PING);
			pingTimer = setTimeout(checkPong, 1000);
		}
		if (status !== 'connected' && pingTimer !== undefined) {
			clearTimeout(pingTimer);
		}
	});

	this.connection.signals.messageReceived.connect(function(event) {
		event.data.arrayBuffer().then(function(buf) {
			var commandView = new Uint16Array(buf, 0, 1);
			var commandNr = commandView[0];
			var data = buf.slice(2);
			self.receiveCommand(commandNr, data);
		});
	});

	this.connection.connect();
}


var api = new Api('ws://10.0.0.2:80');


function onPlaylistClick(e) {
	var target = e.target;
	if (target.tagName.toLowerCase() !== "a") {
		return;
	}

	e.preventDefault();

	var uri = target.getAttribute('href');
	var text = target.textContent;

	var uriArray = new TextEncoder().encode(uri);
	var textArray = new TextEncoder().encode(text);

	var buf = new ArrayBuffer(uriArray.byteLength + textArray.byteLength + 2);
	var bufferView = new Uint8Array(buf);

	bufferView.set(uriArray, 0);
	bufferView.set(textArray, uriArray.byteLength + 1);

	api.sendCommand(COMMAND_SET_PLAYLIST_ITEM, buf);
	//api.sendCommand(COMMAND_PING, new ArrayBuffer(1));
}


function onClick(e) {
	if (e.which !== 1) {
		return;
	}
	if (e.target.closest('.playlist') !== null) {
		onPlaylistClick(e);
	}
}


document.body.addEventListener('click', onClick);


}());
