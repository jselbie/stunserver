
var stun = require('./build/Release/stunserver');


var DEFAULT_STUN_PORT = 3478;
var result = stun.startserver({primaryport:DEFAULT_STUN_PORT});

console.log("startserver["+DEFAULT_STUN_PORT+"] returned: " + result);

setInterval(function(){}, 10000);
