(function() {


var commandLength = 2;


var socket = new WebSocket('ws://10.0.0.2:80');
socket.addEventListener('open', function (event) {
	var buf = new ArrayBuffer(commandLength);
	var bufView = new Uint16Array(buf);
	bufView[0] = 0;
	socket.send(buf);
	console.log("opened socket");
});
socket.addEventListener('close', function (event) {
	console.log("closed socket");
});
socket.addEventListener('error', function (event) {
	console.log("error socket", event);
});
socket.addEventListener('message', function (event) {
	console.log("message", event);
});

}());
