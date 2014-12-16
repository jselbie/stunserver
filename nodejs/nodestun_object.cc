#include "commonincludes.hpp"
#include <node.h>
#include "stuncore.h"
#include "server.h"
#include "nodestun_args.h"
#include "nodestun_object.h"

using namespace v8;

Persistent<Function> NodeStun::constructor;

NodeStun::NodeStun (CStunServerConfig config)
  : instance_config_(config)
  , instance_server_(NULL)
{
}

NodeStun::~NodeStun() {
  if (instance_server_)
  {
    instance_server_->Stop();
    instance_server_->Shutdown();
    instance_server_->Release();
    instance_server_ = NULL;
  }
}

void NodeStun::Init(Handle<Object> exports) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(String::NewSymbol("StunServer"));
  tpl->InstanceTemplate()->SetInternalFieldCount(2);
  // Prototype
  tpl->PrototypeTemplate()->Set(String::NewSymbol("stop"),
  FunctionTemplate::New(Stop)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("start"),
  FunctionTemplate::New(Start)->GetFunction());
  constructor = Persistent<Function>::New(tpl->GetFunction());
  exports->Set(String::NewSymbol("StunServer"), constructor);
}

Handle<Value> NodeStun::New(const Arguments& args) {
  HandleScope scope;

  if (args.IsConstructCall()) {
    // Invoked as constructor: `new MyObject(...)`

    Local<Object> option_map = Object::New();
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

    NodeStun* obj = new NodeStun(config);
    obj->Wrap(args.This());
    return args.This();
  } else {
    // Invoked as plain function `MyObject(...)`, turn into construct call.
    const int argc = 1;
    Local<Value> argv[argc] = { args[0] };
    return scope.Close(constructor->NewInstance(argc, argv));
  }
}

Handle<Value> NodeStun::Start(const Arguments& args) {
  HandleScope scope;
  NodeStun* obj = ObjectWrap::Unwrap<NodeStun>(args.This());

  if(obj->instance_server_){
    ThrowException(Exception::TypeError(String::New("server is already running")));
    return scope.Close(Boolean::New(false));
  }

  HRESULT hr = S_OK;

  hr = CStunServer::CreateInstance(obj->instance_config_, &obj->instance_server_);
  if (FAILED(hr)){
    ThrowException(Exception::TypeError(String::New("server was not initialized")));
    return scope.Close(Boolean::New(false));
  }

  hr = obj->instance_server_->Start();
  if (FAILED(hr))
  {
    ThrowException(Exception::TypeError(String::New("server did not start")));
    return scope.Close(Boolean::New(false));
  }
  return scope.Close(Boolean::New(true));
}

Handle<Value> NodeStun::Stop(const Arguments& args) {
  HandleScope scope;
  NodeStun* obj = ObjectWrap::Unwrap<NodeStun>(args.This());
  if (obj->instance_server_)
  {
    obj->instance_server_->Stop();
    obj->instance_server_->Shutdown();
    obj->instance_server_->Release();
    obj->instance_server_ = NULL;
  }else{
    ThrowException(Exception::TypeError(String::New("server already stopped")));
    return scope.Close(Boolean::New(false));
  }

  return scope.Close(Boolean::New(true));
}
