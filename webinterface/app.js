(function() {


var commandLength = 2;
var connected = false;


var socket = new WebSocket('ws://10.0.0.2:80');
socket.addEventListener('open', function (event) {
	/*
	var buf = new ArrayBuffer(commandLength);
	var bufView = new Uint16Array(buf);
	bufView[0] = 0;
	socket.send(buf);
	console.log("opened socket");
	*/
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


}());
