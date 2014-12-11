
var stun = require('./build/Release/stunserver');
var DEFAULT_STUN_PORT = 3478;
var argtest = [
  {port:DEFAULT_STUN_PORT},
  DEFAULT_STUN_PORT,
  void(0)
]
for(var i =0;i<argtest.length;i++){
  console.log("startserver("+JSON.stringify(argtest[i])+")");
  var result = stun.startserver(argtest[i]);
  stun.stopserver();
  console.log("returned: " + result)
  console.log("\n\n")
}
console.log("startserver() returned: " + result);
result = stun.startserver();
stun.stopserver();
console.log("returned: " + result)

setInterval(function(){}, 10000);
