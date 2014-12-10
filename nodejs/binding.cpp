#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif


#include "commonincludes.hpp"
#include <node.h>
#include "stuncore.h"
#include "server.h"
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
  Local<Object> option_map = Object::New();
  int n;
  int len = 11;
  Local<Value> temp;
  std::string option_names [11] = {
    "primaryport",
    "primaryinterface",
    "primaryadvertised",
    "altport",
    "altinterface",
    "altadvertised",
    "mode",
    "family",
    "protocol",
    "maxconn",
    "verbosity"
  };


  if (args.Length() > 11)
  {
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    return scope.Close(False());
  }

  if(args.Length() == 1 && args[0]->IsObject())
  {
    printf("Handling Options Argument \n");
    Handle<Object> opt = args[0]->ToObject();
    char* sym;
    for (n=0; n<len; ++n)
    {
      sym = (char*)option_names[n].c_str();
      temp = opt->Get( String::NewSymbol(sym));
      if(!temp->IsUndefined())
      {
        option_map->Set(String::NewSymbol(sym), temp);
      }
    }
  }
  else if(args.Length() > 0)
  {
    printf("Handling Arguments as Array \n");
    len = args.Length();
    for (n=0; n<len; ++n)
    {
      printf("%s\n",(char*)option_names[n].c_str());
      option_map->Set(String::NewSymbol((char*)option_names[n].c_str()), args[n]);
    }
  }
  else
  {
    printf("Handling Defaults \n");
    //defaults
  }

  int port = option_map->Get(String::NewSymbol("primaryport"))->Int32Value();
  if ((port < 0) || (port > 65535))
  {
    ThrowException(Exception::TypeError(String::New("Invalid value for port")));
    return scope.Close(False());
  }
  printf("Port: %d \n",port);

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
