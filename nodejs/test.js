
var stun = require('./build/Release/stunserver');
var DEFAULT_STUN_PORT = 3478;

var possextra = [
  {protocol:"tcp"},
  "tcp",
  void(0)
];

var possstun = [
  {port:DEFAULT_STUN_PORT},
  DEFAULT_STUN_PORT,
  void(0),
];

var perms = [];

for(var i=0;i<4;i++){
  if(i == 3){
    perms.push([]);
    continue;
  }
  for(var ii=0;ii<4;ii++){
    if(ii == 3){
      perms.push([possstun[i]]);
      continue;
    }
    for(var iii=0;iii<4;iii++){
      if(iii == 3){
        perms.push([possstun[i],possstun[ii]]);
        continue;
      }
      perms.push([possstun[i],possstun[ii],possextra[iii]]);
    }
  }
}
for(var i =0;i<perms.length;i++){
  console.log("startserver"+JSON.stringify(perms[i])+"");
  var result = stun.startserver.apply(stun,perms[i]);
  stun.stopserver();
  console.log("returned: " + result)
  console.log("\n\n")
}

setInterval(function(){}, 10000);
