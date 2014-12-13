#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif


#include "commonincludes.hpp"
#include <node.h>
#include "stuncore.h"
#include "server.h"
#include "nodestun_args.h"
#include "stdio.h"

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


bool startGlobalServer(CStunServerConfig config)
{
    HRESULT hr = S_OK;
    bool result = true;

    // stop any previous instance
    stopGlobalServer();

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

    return result;
}


using namespace v8;

Handle<Value> StartServer(const Arguments& args)
{
  HandleScope scope;

  Local<Object> option_map = Object::New();

  /*
  if(NodeStun_Args::ThreeArgs(args,option_map)){
    return scope.Close(Boolean::New(false));
  }
  */
  if(NodeStun_Args::OneArgs(args,option_map)){
    return scope.Close(Boolean::New(false));
  }
  if(option_map->Get(String::New("verbosity"))->IsNumber()){
    int temp = option_map->Get(String::New("verbosity"))->Int32Value();
    Logging::SetLogLevel(temp);
  }

  CStunServerConfig config;

  if(NodeStun_Args::Object2Config(option_map, config)){
    return scope.Close(Boolean::New(false));
  }

  //  printf("Port: %d \n",n);
  HRESULT hr = S_OK;

  // stop any previous instance
  stopGlobalServer();

  hr = CStunServer::CreateInstance(config, &g_pServer);

  if (FAILED(hr))
  {
    ThrowException(Exception::TypeError(String::New("server was not initialized")));
    stopGlobalServer();
    return scope.Close(Boolean::New(false));
  }

  hr = g_pServer->Start();

  if (FAILED(hr))
  {
    ThrowException(Exception::TypeError(String::New("server did not start")));
    stopGlobalServer();
    return scope.Close(Boolean::New(false));
  }

  return scope.Close(Boolean::New(true));
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
