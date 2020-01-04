(function() {

var socket = new WebSocket('ws://10.0.0.2:80');
socket.addEventListener('open', function (event) {
	console.log("opened socket");
});
socket.addEventListener('close', function (event) {
	console.log("closed socket");
});
socket.addEventListener('error', function (event) {
	console.log("error socket", event);
});
socket.addEventListener('message', function (event) {
	console.log("message socket");
});

}());
