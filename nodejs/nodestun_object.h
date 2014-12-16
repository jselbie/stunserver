#ifndef STUNSERVER_H
#define STUNSERVER_H

#include <node.h>
#include "server.h"

class NodeStun : public node::ObjectWrap {
public:
  static void Init(v8::Handle<v8::Object> exports);

private:
  NodeStun(CStunServerConfig config);
  ~NodeStun();

  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Start(const v8::Arguments& args);
  static v8::Handle<v8::Value> Stop(const v8::Arguments& args);
  static v8::Persistent<v8::Function> constructor;
  CStunServerConfig instance_config_;
  CStunServer* instance_server_;
};

#endif
