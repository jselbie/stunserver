
var stun = require('./build/Release/stunserver');


var DEFAULT_STUN_PORT = 3478;
var result = stun.startserver(3478);

console.log("startserver returned: " + result);

while (true)
{

    ;

}


