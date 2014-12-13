{
  "targets": [
    {
      "target_name": "stunserver",
      "defines" : ["NDEBUG"],
      "include_dirs" : ["../stuncore", "../server", "../common","../networkutils", "../resources", "/home/jselbie/boost_1_57_0"],
      "sources" : [
       "binding.cpp",
       "nodestun_args.cpp",
       "../common/logger.cpp",
       "../common/common.cpp",
       "../common/getmillisecondcounter.cpp",
       "../common/fasthash.cpp",
       "../common/stringhelper.cpp",
       "../common/prettyprint.cpp",
       "../common/refcountobject.cpp",
       "../common/atomichelpers.cpp",
       "../common/getconsolewidth.cpp",
       "../common/cmdlineparser.cpp",
       "../server/main.cpp",
       "../server/stunconnection.cpp",
       "../server/sampleauthprovider.cpp",
       "../server/server.cpp",
       "../server/tcpserver.cpp",
       "../server/stunsocketthread.cpp",
       "../networkutils/recvfromex.cpp",
       "../networkutils/resolvehostname.cpp",
       "../networkutils/stunsocket.cpp",
       "../networkutils/adapters.cpp",
       "../networkutils/polling.cpp",
       "../stuncore/buffer.cpp",
       "../stuncore/stunclienttests.cpp",
       "../stuncore/stunbuilder.cpp",
       "../stuncore/stunutils.cpp",
       "../stuncore/stunreader.cpp",
       "../stuncore/socketaddress.cpp",
       "../stuncore/datastream.cpp",
       "../stuncore/stunclientlogic.cpp",
       "../stuncore/messagehandler.cpp" ]
    }
  ]
}
