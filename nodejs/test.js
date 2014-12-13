var poss = [
  [ 3000, 3001, 3002 ],//   primary_port
  [],//                     primary_interface
  [],//                     primary_advertised
  [3001,3001,3001],//       alternate_port
  [],//                     alternate_interface
  [],//                     alternate_advertised
  [ "tcp", "udp" ],//       protocol
  [ "basic", "full" ],//    mode
  [ 4, 6 ],//               family
  [0,1,99,0xffffffff],//    max_connections
  [true,false]//  verbosity
];
var keys = [
  "primary_port",
  "primary_interface",
  "primary_advertised",
  "alternate_port",
  "alternate_interface",
  "alternate_advertised",
  "protocol",
  "mode",
  "family",
  "max_commections",
  "verbosity"
]
var tests = [];

function test_r(curtest,isi){
  for(var i =0; i<poss[isi].length+1;i++){
    var toadd = JSON.parse(JSON.stringify(curtest));
    toadd[keys[isi]] = (poss[isi][i])?poss[isi][i]:void(0);
    if(isi < poss.length-1){
      test_r(toadd, isi+1);
    }else{
      tests.push(toadd);
    }
  }
  tests.push(curtest);
}
test_r({},0);

var stun = require("./build/Release/stunserver");

for(var i=0;i<tests.length;i++){
  result = stun.startserver(tests[i]);
  stun.stopserver();
}
