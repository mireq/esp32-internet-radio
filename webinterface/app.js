(function() {

var socket = new WebSocket('ws://10.0.0.2:80');
socket.addEventListener('open', function (event) {
	console.log("opened socket");
});

}());
