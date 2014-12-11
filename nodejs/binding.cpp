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


std::string v8str2stdstr(Handle<String> ori){
  String::Utf8Value param1(ori);
  std::string to = std::string(*param1);
  return to;
}

Handle<Value> StartServer(const Arguments& args)
{
  HandleScope scope;
  Local<Object> pri_and_alt_map [2];

  Local<Object> option_map = Object::New();

  int n;
  int argslen = args.Length();
  Local<Value> temp;

  Local<String> port_options [3] = {
    String::NewSymbol("port"),
    String::NewSymbol("interface"),
    String::NewSymbol("advertised")
  };

  Local<String> option_names [5] = {
    String::NewSymbol("protocol"),
    String::NewSymbol("mode"),
    String::NewSymbol("family"),
    String::NewSymbol("maxconn"),
    String::NewSymbol("verbosity")
  };

  if(argslen > 3){
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    return scope.Close(False());
  }

  for(n=0;n<2;n++)
  {
    int nn;
    pri_and_alt_map[n] = Object::New();
    if(!(argslen > n) || args[n]->IsUndefined())
    {
      printf("Stun[%d] will be defaults \n", n);
    }else if(args[n]->IsObject())
    {
      printf("Stun[%d] is detailed \n", n);
      Handle<Object> opt = args[n]->ToObject();
      for (nn=0; nn<3; ++nn)
      {
        temp = opt->Get(port_options[nn]);
        if(!temp->IsUndefined())
        {
          printf(
            "Stun[%d].detail[%s] will be set \n",
            n,
            v8str2stdstr(port_options[nn]).c_str()
          );
          pri_and_alt_map[n]->Set(port_options[nn], temp);
        }else{
          printf(
            "Stun[%d].detail[%s] will be default \n",
            n,
            v8str2stdstr(port_options[nn]).c_str()
          );
        }
      }
    }
    else if(args[n]->IsNumber())
    {
      printf("Only setting port for Stun[%d] \n", n);
      pri_and_alt_map[n]->Set(port_options[0], args[n]);
    }
    else
    {
      ThrowException(Exception::TypeError(
        String::New("Primary can only be an object, an integer or undefined")
      ));
      return scope.Close(False());
    }
    if(pri_and_alt_map[n]->Get(port_options[0])->IsUndefined())
    {
      if(n==0)
      {
        pri_and_alt_map[n]->Set(port_options[0], Number::New(3478));
      }
      else
      {
        nn = pri_and_alt_map[n-1]->Get(port_options[0])->Int32Value();
        pri_and_alt_map[n]->Set(port_options[0], Number::New((nn+1)%65536));
      }
    }
    else if(!pri_and_alt_map[n]->Get(port_options[0])->IsNumber())
    {
      ThrowException(Exception::TypeError(String::New("Port can only be a number")));
      return scope.Close(False());
    }
    nn = pri_and_alt_map[n]->Get(port_options[0])->Int32Value();
    if ((nn < 0) || (nn > 65535))
    {
      ThrowException(Exception::TypeError(String::New("Invalid value for port")));
      return scope.Close(False());
    }

  }
  if(args.Length() < 3 || args[2]->IsUndefined())
  {
    printf("Extra Options will be defaults \n");
  }
  else if(args[2]->IsObject())
  {
    Handle<Object> opt = args[2]->ToObject();
    printf("Extra Options is detailed \n");
    for (n=0; n<5; ++n)
    {
      temp = opt->Get(option_names[n]);
      if(!temp->IsUndefined())
      {
        printf(
          "Extra.detail[%s] \n",
          v8str2stdstr(option_names[n]).c_str()
        );
        option_map->Set(option_names[n], temp);
      }
    }
  }
  else if(args[2]->IsString())
  {
    printf("Only setting protocol in Extras \n");
    option_map->Set(option_names[0], args[2]);
  }
  else
  {
    ThrowException(Exception::TypeError(String::New("Extra Options can only be an object or a string")));
    return scope.Close(False());
  }


  n = pri_and_alt_map[0]->Get(port_options[0])->Int32Value();

  printf("Port: %d \n",n);

  bool result = startGlobalServer(n);

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
