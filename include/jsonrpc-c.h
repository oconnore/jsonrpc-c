/*
 * jsonrpc-c.h
 *
 *  Created on: Oct 11, 2012
 *      Author: hmng
 */

#define _ISOC99_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _XOPEN_SOURCE_EXTENDED 700
#define _GNU_SOURCE 1

#ifndef JSONRCPC_H_
#define JSONRPCC_H_

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

#include <jansson.h>
#include <ev.h>

/*
 * http://www.jsonrpc.org/specification
 *
 * code	message	meaning
 * -32700 Parse error      - Invalid JSON was received by the server.
 *                           An error occurred on the server while
 *                           parsing the JSON text.
 * -32600 Invalid Request  - The JSON sent is not a valid Request object.
 * -32601 Method not found - The method does not exist / is not available.
 * -32602 Invalid params   - Invalid method parameter(s).
 * -32603 Internal error   - Internal JSON-RPC error.
 * -32000 Server error     - Reserved for implementation-defined
 *    |                      server errors
 * -32099
 */

#define JRPC_PARSE_ERROR -32700
#define JRPC_INVALID_REQUEST -32600
#define JRPC_METHOD_NOT_FOUND -32601
#define JRPC_INVALID_PARAMS -32603
#define JRPC_INTERNAL_ERROR -32693


#define JRPC_SUCCESS 0
#define JRPC_ERROR -1

//
// Macros
//

#define FIELD_SIZE(type,field) (sizeof((((type*)0))->field))

typedef struct {
  void *data;
  int error_code;
  char *error_msg;
  json_t *error_data;
} jrpc_context;

typedef json_t*
(*jrpc_function)(jrpc_context *context, json_t *params, json_t* id);

typedef struct{
  char * name;
  jrpc_function function;
  void *data;
} jrpc_procedure;

#ifdef DEBUG
typedef struct {
  int code;
  char cause[80], msg[255];
} jrpc_error;
#endif

typedef struct {
  char *hostname;
  int port_number;
  struct ev_loop *loop;
  struct ev_io listen_watcher;
  int procedure_count;
  jrpc_procedure *procedures;

#ifdef DEBUG
  jrpc_error error;
#endif

} jrpc_server;

typedef struct {

  struct ev_io io;

  int fd;
  int pos;
  unsigned int buffer_size;
  char *buffer;

  // server context
  jrpc_server *server;
  int debug_level;

} jrpc_connection;

//
// Functions
//

#ifdef DEBUG

static
char* jrpc_new_vsprintf(const char *fmt, va_list vargs);

static
char* jrpc_new_sprintf(const char *fmt, ...);

void jrpc_clear_error(jrpc_server *server);

static
void jrpc_set_error(jrpc_server *server,
                    int code,
                    const char *cause,
                    const char *msg);

#endif

static
void* get_in_addr(struct sockaddr* sock);

static
int send_response(jrpc_connection* connection,
                  char* response);

static inline
void send_static_error(jrpc_connection *conn);

static
int send_error(jrpc_connection *conn,
               json_int_t code,
               char *msg,
               json_t *error_object,
               json_t *id);

static
int send_result(jrpc_connection *conn,
                json_t *result_object,
                json_t *id);

static
int invoke_procedure(jrpc_server *server,
                     jrpc_connection *conn,
                     const char *name,
                     json_t *params,
                     json_t *id);

static
int eval_request(jrpc_server* server,
                 jrpc_connection* conn,
                 json_t* root);

static
void close_connection(struct ev_loop *loop,
                      struct ev_io *w);

static
void handle_buffer(jrpc_connection *conn);

static
void connection_cb(struct ev_loop *loop,
                   struct ev_io *w,
                   int revents);

static
void accept_cb(struct ev_loop *loop,
               struct ev_io *w,
               int revents);

int jrpc_server_init(jrpc_server *server,
                     const char *hostname,
                     int port);

int jrpc_server_init_with_ev_loop(jrpc_server *server,
                                  const char *hostname,
                                  int port,
                                  struct ev_loop *loop);

static
int __jrpc_server_start(jrpc_server *server);

void jrpc_server_run(jrpc_server *server);

int jrpc_server_stop(jrpc_server *server);

void jrpc_server_destroy(jrpc_server *server);

static
void jrpc_procedure_destroy(jrpc_procedure *procedure);

int jrpc_register_procedure(jrpc_server *server,
                            jrpc_function function_pointer,
                            char *name,
                            void *data);

int jrpc_deregister_procedure(jrpc_server *server,
                              char *name);

#endif
