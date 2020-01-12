(function() {


/*
var commandLength = 2;
var connected = false;


var socket = new WebSocket('ws://10.0.0.2:80');
socket.addEventListener('open', function (event) {
	var buf = new ArrayBuffer(commandLength);
	var bufView = new Uint16Array(buf);
	bufView[0] = 0;
	socket.send(buf);
	console.log("opened socket");
	connected = true;
});
socket.addEventListener('close', function (event) {
	connected = false;
});
socket.addEventListener('error', function (event) {
	connected = false;
});
socket.addEventListener('message', function (event) {
	console.log("message", event);
});
*/

function Signal() {
	var self = this;
	this.listeners = [];

	this.connect = function(callback) {
		if (callback === undefined) {
			wtf();
		}
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
			self.close();
		});
	};

	function sendMsg(data) {
		conn.send(data);
	}

	function close() {
		if (self.status == 'connected' && conn !== undefined) {
			conn.close();
			conn = undefined;
			self.status = 'disconnected';
			signals.statusChanged.send(self.status);
			signals.close.send();
		}
	}

	this.signals = signals;
	this.sendMsg = sendMsg;
	this.close = close;
}


function Api(socket_url) {
	var self = this;

	var pingTimer;
	var pongReceived = false;

	function checkPong() {
		if (pongReceived) {
			pongReceived = false;
			pingTimer = setTimeout(checkPong, 1000);
		}
		else {
			self.connection.close();
		}
	}

	self.connection = new Connection(socket_url);

	self.connection.signals.close.connect(function() {
		self.connection.connect();
	});

	self.connection.signals.statusChanged.connect(function(status) {
		console.log(status);
		if (status === 'connected' && pingTimer === undefined) {
			pingTimer = setTimeout(checkPong, 1000);
		}
		if (status !== 'connected' && pingTimer !== undefined) {
			clearTimeout(pingTimer);
		}
	});

	self.connection.connect();
}


var api = new Api('ws://10.0.0.2:80');


}());
