#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif


#include "commonincludes.hpp"
#include <node.h>
#include "stuncore.h"
#include "server.h"

CStunServer* g_pServer = NULL;


void stopGlobalServer()
{
  if (g_pServer)
  {
      g_pServer->Stop();
      g_pServer->Shutdown();
      g_pServer->Release();
      g_pServer = NULL;
  }
}

bool startGlobalServer(int port)
{
    HRESULT hr = S_OK;
    bool result = true;

    // stop any previous instance    
    stopGlobalServer();
    
    
    // single port mode
    CStunServerConfig config;
    config.fHasPP = true;
    config.addrPP.SetPort(port);
    
    hr = CStunServer::CreateInstance(config, &g_pServer);
    
    if (SUCCEEDED(hr))
    {
        hr = g_pServer->Start();
    }

    if (FAILED(hr))
    {
        stopGlobalServer();
        result = false;
    }
    
    return true;
}


using namespace v8;

Handle<Value> StartServer(const Arguments& args)
{
  HandleScope scope;
  int port = 0;

  if (args.Length() != 1)
  {
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    return scope.Close(False());
  }
  
  port = args[0]->Int32Value();
  if ((port < 0) || (port > 65535))
  {
    ThrowException(Exception::TypeError(String::New("Invalid value for port")));
    return scope.Close(False());
  }
  
  bool result = startGlobalServer(port);
  
  return scope.Close(Boolean::New(result));

}

Handle<Value> StopServer(const Arguments& args) {
  HandleScope scope;

  stopGlobalServer();
  return scope.Close(True());
}



void Init(Handle<Object> exports) {
  exports->Set(String::NewSymbol("startserver"),
      FunctionTemplate::New(StartServer)->GetFunction());

  
    exports->Set(String::NewSymbol("stopserver"),
      FunctionTemplate::New(StopServer)->GetFunction());

    

}

NODE_MODULE(stunserver, Init)

