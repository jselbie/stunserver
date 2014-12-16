var poss = [
  [
/* protocol */
/* Acceptable: */  [ "tcp", "udp"],
/* Errors */       ["tls" ]
  ],[
//mode
/* Acceptable: */  [ "basic", "full"],
/* Errors */       ["none" ]
  ],[
//family
/* Acceptable: */  [ 4, 6],
/* Errors */       [ 7 ]
  ],[
//max_connections
/* Acceptable: */  [1, 100000],
/* Errors: */      [0, 100001]
  ],[
//primary_port
/* Acceptable: */  [3000,65535],
/* Errors: */      [1, 0, 65536 ]
  ],[
//primary_interface
/* Acceptable: */  [],
/* Errors: */      []
  ],[
//primary_advertised
/* Acceptable: */  [],
/* Errors: */      []
  ],[
//alternate_port
/* Acceptable: */  [3001],
/* Errors: */      [1, 3000, 0, 65536]
  ],[
//alternate_interface
/* Acceptable: */  [],
/* Errors: */      []
  ],[
//alternate_advertised
/* Acceptable: */  [],
/* Errors: */      []
  ]
];
var keys = [
  "protocol",
  "mode",
  "family",
  "max_connections",
  "primary_port",
  "primary_interface",
  "primary_advertised",
  "alternate_port",
  "alternate_interface",
  "alternate_advertised"
]
var tests = [];

function addtest(arg,item,msg){
  tests.push(function(){
    var result;
    if(msg instanceof Error){
      var result = 0;
      console.log(msg+": {"+item+":"+arg[item]+"}");
      try{
        stun.startGlobal(arg);
      }catch(e){
        result = 1
        console.log("recieved error: "+e);
        console.log("\n");
      }
      if(result === 0){
        throw new Error("did not throw an error");
      }
    }else{
      console.log(msg+": {"+item+":"+arg[item]+"}");
      var result = stun.startGlobal(arg);
      if(!result){
        throw new Error("uncaught problem");
      }
    }
    stun.stopGlobal();
    console.log("\n")
  })
}

function test_r(curtest,isi){
  if(curtest["mode"] == "basic" && /^alternate_/.test(keys[isi])){
    addtest(curtest,keys[isi-1],"skipping alternate in basic mode");
    return;
  }
  for(var i =0; i<poss[isi][0].length+1;i++){
    var toadd = JSON.parse(JSON.stringify(curtest));
    toadd[keys[isi]] = (poss[isi][0][i])?poss[isi][0][i]:void(0);
    if(isi < poss.length-1){
      test_r(toadd, isi+1);
    }else{
      addtest(toadd,keys[isi],"should work");
    }
  }
  for(var i =0; i<poss[isi][1].length;i++){
    var toadd = JSON.parse(JSON.stringify(curtest));
    toadd[keys[isi]] = poss[isi][1][i];
    addtest(toadd,keys[isi],new Error("should throw error"));
  }
  addtest(curtest,keys[isi],"missing argument");
}
test_r({verbosity:1},0);

var stun = require("./build/Release/stunserver");

for(var i=0;i<tests.length;i++){
  tests[i]();
}
