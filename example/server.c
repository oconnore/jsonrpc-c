/*
 * server.c
 *
 *  Created on: Oct 9, 2012
 *      Author: hmng
 */

#define DEBUG 1

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include "jsonrpc-c.h"

#define PORT 1234  // the port users will be connecting to

jrpc_server my_server;

json_t* say_hello(jrpc_context *ctx, json_t *params, json_t *id) {
  char buf[255];
  json_t *param;
  memset(&buf, 0, 255);
  if( json_unpack(params, "[o]", &param) == 0 &&
      json_string_value(param)!=NULL ) {
    snprintf(buf, 254, "Hello %s!\n", json_string_value(param));
    json_decref(param);
  } else {
    ctx->error_code=-1;
    char *tmp=strdup("Missing name parameter!");
    ctx->error_msg=tmp;
  }
  return json_string(buf);
}

json_t* exit_server(jrpc_context *ctx, json_t *params, json_t *id) {
  jrpc_server_stop(&my_server);
  return json_string("Bye!");
}

int main(void) {
  jrpc_server_init(&my_server, "127.0.0.1", PORT);
  jrpc_register_procedure(&my_server, say_hello, "sayHello", NULL );
  jrpc_register_procedure(&my_server, exit_server, "exit", NULL );
  jrpc_server_run(&my_server);
  jrpc_error *err=&my_server.error;
  jrpc_server_destroy(&my_server);
  return 0;
}
