(function(window) {
'use strict';


function forEach(collection, fn) {
	Array.prototype.forEach.call(collection, fn);
}


function toArray(collection) {
	return Array.prototype.slice.call(collection, 0);
}


function decodeURLParameters(data) {
	var parameters = [];
	if (data === '') {
		return parameters;
	}
	var components = data.split('&');
	forEach(components, function(component) {
		var separatorPosition = component.indexOf('=');
		if (separatorPosition === -1) {
			parameters.push([decodeURIComponent(component), '']);
		}
		else {
			var name = decodeURIComponent(component.slice(0, separatorPosition));
			var value = decodeURIComponent(component.slice(separatorPosition + 1));
			parameters.push([name, value]);
		}
	});
	return parameters;
}


function getQueryParameters() {
	var url = window.location + '';
	var qmark = url.indexOf('?');
	if (qmark === -1) {
		return {};
	}
	var paramsList = decodeURLParameters(url.slice(qmark + 1));
	var params = {};
	forEach(paramsList, function(p) {
		params[p[0]] = p[1];
	});
	return params;
}


function closest(target, selector) {
	if (target.matches(selector)) {
		return target;
	}
	return target.closest(selector);
}


function debounce(fn, delay) {
	var timer = null;
	var context, args;
	var closure = function () {
		context = this;
		args = arguments;
		if (timer === null) {
			timer = setTimeout(function () {
				fn.apply(context, args);
				timer = null;
			}, delay);
		}
	};
	var instant = function() {
		var context = this, args = arguments;
		clearTimeout(timer);
		fn.apply(context, args);
	};
	closure.instant = instant;
	return closure;
}


window._ = {
	decodeURLParameters: decodeURLParameters,
	getQueryParameters: getQueryParameters,
	closest: closest,
	toArray: toArray,
	forEach: forEach,
	debounce: debounce
};

}(window));
