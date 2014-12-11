
var stun = require('./build/Release/stunserver');


var DEFAULT_STUN_PORT = 3478;
var result = stun.startserver({port:DEFAULT_STUN_PORT});
var result = stun.startserver(DEFAULT_STUN_PORT);

console.log("startserver["+DEFAULT_STUN_PORT+"] returned: " + result);

setInterval(function(){}, 10000);
