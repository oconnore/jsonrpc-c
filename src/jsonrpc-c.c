/*
 * jsonrpc-c.c
 *
 *  Created on: Oct 11, 2012
 *      Author: hmng
 */

#include "jsonrpc-c.h"

//
// Debugging
//

static
char* jrpc_new_vsprintf(const char *fmt, va_list vargs) {
  char *buf;
  va_list tmp_vargs;
  va_copy(tmp_vargs, tmp_vargs);
  int len=vsnprintf(NULL,0,fmt,tmp_vargs)+1;
  buf=(char*)malloc(len);
  vsnprintf(buf, len, fmt, vargs);
  return buf;
}

static
char* jrpc_new_sprintf(const char *fmt, ...) {
  char *buf;
  va_list args;
  va_start(args, fmt);
  buf=jrpc_new_vsprintf(fmt, args);
  va_end(args);
  return buf;
}

void jrpc_clear_error(jrpc_server *server) {
  memset(&server->error, 0, sizeof(jrpc_error));
}

static
void jrpc_set_error(jrpc_server *server,
                    int code,
                    const char *cause,
                    const char *msg) {
  jrpc_error *err=&server->error;
  memset(err, 0, sizeof(jrpc_error));
  err->code=code;
  if(cause!=NULL)
    strncpy(err->cause, cause, FIELD_SIZE(jrpc_error, cause));
  if(msg!=NULL)
    strncpy(err->msg, msg, FIELD_SIZE(jrpc_error, msg));
}

#endif

//
// INET4/6
//

// get sockaddr, IPv4 or IPv6:
static
void* get_in_addr(struct sockaddr *sa) {

  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*) sa)->sin_addr);
  } else {
    return &(((struct sockaddr_in6*) sa)->sin6_addr);
  }

}

static
int send_response(jrpc_connection *conn,
                  char *response) {
  return
    ( write(conn->fd, response, strlen(response) ) > 0 ) &&
    ( write(conn->fd, "\n", 1) > 0 );
}

//
// JSON RPC Functions
//

static inline
void send_static_error(jrpc_connection *conn){
  send_response(conn,
                "{\"jsonrpc\":\"2.0\",\"error\":"
                "{\"code\":-32603,\"message\":\"Internal Error\"}}");
}

static
json_t* safe_json_ptr(json_t *ptr){
  if(ptr==NULL) return json_null();
  else return ptr;
}

static
int send_error(jrpc_connection *conn,
               json_int_t code,
               char *msg,
               json_t *error_object,
               json_t *id) {
  int return_value = -1;
  json_t *json=json_pack("{s:s,s:{s:i,s:s,s:o},s:o}",
                         "jsonrpc","2.0",
                         "error",
                         "code", code,
                         "message", msg,
                         "data", safe_json_ptr(error_object),
                         "id", safe_json_ptr(id));
  if( json != NULL) {
    char *buf=json_dumps(json, JSON_COMPACT | JSON_PRESERVE_ORDER);
    if( buf != NULL ) {
      return_value=send_response(conn, buf);
      free(buf);
    } else {
      send_static_error(conn);
    }
    json_decref(json);
  } else {
    send_static_error(conn);
  }
  return return_value;
}

static
int send_result(jrpc_connection *conn,
                json_t *result_object,
                json_t *id) {
  int return_value = -1;
  json_t *json=json_pack("{s:s,s:o,s:o}",
                         "jsonrpc","2.0",
                         "result", safe_json_ptr(result_object),
                         "id", safe_json_ptr(id));
  if( json != NULL ) {
    char *buf=json_dumps(json, JSON_COMPACT|JSON_PRESERVE_ORDER);
    if( buf != NULL) {
      return_value=send_response(conn, buf);
      free(buf);
    } else {
      send_static_error(conn);
    }
    json_decref(json);
  } else {
    send_static_error(conn);
  }
  return return_value;
}

static
int invoke_procedure(jrpc_server *server,
                     jrpc_connection *conn,
                     const char *name,
                     json_t *params,
                     json_t *id) {
  json_t *returned = NULL;
  int procedure_found = 0, result;
  jrpc_context ctx;
  memset(&ctx, 0, sizeof(jrpc_context));

  for( int i=0; i<server->procedure_count; i++) {
    if( strcmp(server->procedures[i].name, name)==0 ) {
      procedure_found = 1;
      ctx.data = server->procedures[i].data;
      returned = server->procedures[i].function(&ctx, params, id);
      if( ctx.error_code == 0) {
        result=send_result(conn, returned, id);
      } else {
        result=send_error(conn,
                          ctx.error_code, ctx.error_msg,
                          ctx.error_data,
                          id);
      }
      if(ctx.error_msg != NULL){
        free(ctx.error_msg);
      }
      return result;
    }
  }
  return send_error(conn,
                    JRPC_METHOD_NOT_FOUND,
                    "Method not found.",
                    NULL,
                    id);
}

static
int eval_request(jrpc_server* server,
                 jrpc_connection* conn,
                 json_t* root) {
  char *version, *method;
  json_t *params, *id;
  if( json_unpack(root, "{s:s,s:s,s:o,s:o}",
                  "jsonrpc", &version,
                  "method", &method,
                  "params", &params,
                  "id", &id) == 0) {
    if( version!=NULL &&
        strcmp(version,"2.0")==0 &&
        method!=NULL ) {
      return invoke_procedure(server, conn,
                              method,
                              params, id);
    } else {
      send_error(conn, JRPC_INVALID_REQUEST,
                 "Missing version or method", NULL, NULL);
    }
  } else {
    send_static_error(conn);
  }
  return -1;
}

static
void close_connection(struct ev_loop *loop,
                      struct ev_io *w) {

  jrpc_connection *wptr = (jrpc_connection*) w;
  ev_io_stop(loop, w);
  close(wptr->fd);
  free(wptr->buffer);
  free(wptr);

}

static
void handle_buffer(jrpc_connection *conn) {
  json_error_t error;
  json_t *root;;
  
  if((root = json_loads(conn->buffer,
                        JSON_DISABLE_EOF_CHECK,
                        &error)) != NULL) {
    if(json_is_object(root)) {
      eval_request(conn->server, conn, root);
    }
    json_decref(root);
    
    /* Shift processed request, discarding it */
    memmove(conn->buffer, conn->buffer+conn->pos,
            error.position);
    conn->pos -= error.position;
    memset(conn->buffer + conn->pos, 0,
           conn->buffer_size - conn->pos);

  } else {

    // Did we parse the all buffer? If so, just wait for more.
    // else there was an error before the buffer's end
    if (error.position != conn->pos) {
#ifdef DEBUG
      char *msg=jrpc_new_sprintf("Parse error at %d", error.position);
      jrpc_set_error(conn->server, -1, "json_loads", msg);
      free(msg);
#endif
      send_error(conn,
                 JRPC_PARSE_ERROR,
                 strdup("Parse error. "
                        "Invalid JSON was received by the server."),
                 NULL, NULL);
      return close_connection(conn->server->loop, &conn->io);
    }
  }
}

static
void connection_cb(struct ev_loop *loop,
                   struct ev_io *w,
                   int revents) {

  jrpc_connection *conn;
  jrpc_server *server = (jrpc_server*) w->data;
  size_t bytes_read = 0;

  /* Get our 'subclassed' event watcher */
  conn = (jrpc_connection*) w;
  int fd = conn->fd;

  if (conn->pos >= conn->buffer_size ) {

    conn->buffer_size *= 2;
    char *new_buffer=realloc(conn->buffer, conn->buffer_size);

    if( new_buffer == NULL ) {
#ifdef DEBUG
      jrpc_set_error(server, -1, "realloc", "Memory error");
#endif
      return close_connection(loop, w);
    }

    conn->buffer = new_buffer;
    memset(conn->buffer + conn->pos, 0,
           conn->buffer_size - conn->pos);

  }

  // can not fill the entire buffer, string must be NULL terminated
  int max_read_size = conn->buffer_size - conn->pos - 1;

  bytes_read=read(fd, conn->buffer + conn->pos, max_read_size);

  if (bytes_read == -1) {

    /* We experienced a read error, close the connection */

#ifdef DEBUG
    char *msg=jrpc_new_sprintf("Read error in fd:%d", conn->fd);
    jrpc_set_error(server, -1, "read", msg);
    free(msg);
#endif

    close_connection(loop, w);

  } else if( bytes_read == 0 ) {

    /* We reached EOF, close the connection */
    close_connection(loop, w);

  } else {

    /* We read some bytes, attempt to parse */
    conn->pos += bytes_read;
    handle_buffer( conn );

  }

}

static
void accept_cb(struct ev_loop *loop,
               struct ev_io *w,
               int revents) {
  char s[INET6_ADDRSTRLEN];
  jrpc_connection *connection_watcher;
  connection_watcher = (jrpc_connection*) malloc(sizeof(jrpc_connection));
  struct sockaddr_storage their_addr; // connector's address information
  socklen_t sin_size;
  sin_size = sizeof(their_addr);
  connection_watcher->fd =
    accept(w->fd, (struct sockaddr *) &their_addr,
           &sin_size);
  if (connection_watcher->fd == -1) {
#ifdef DEBUG
    jrpc_set_error((jrpc_server*)w->data, errno, "accept", NULL);
#endif
    free(connection_watcher);
  } else {
    //copy pointer to struct jrpc_server
    connection_watcher->io.data = w->data;
    connection_watcher->buffer_size = 1500;
    connection_watcher->buffer = malloc(1500);

    if( connection_watcher->buffer == NULL ){
#ifdef DEBUG
      jrpc_set_error((jrpc_server*)w->data, -1, "accept_cb",
                     "malloc failed");
#endif
      close( connection_watcher->fd );
      free( connection_watcher );
      return;
    }

    memset(connection_watcher->buffer, 0, 1500);

    connection_watcher->pos = 0;
    connection_watcher->server = (jrpc_server*) w->data;

    ev_io_init( &connection_watcher->io,
                connection_cb,
                connection_watcher->fd,
                EV_READ );
    ev_io_start(loop, &connection_watcher->io);
  }
}

//
// Server Initialization
//

int jrpc_server_init(jrpc_server *server,
                     const char *hostname,
                     int port_number) {
  struct ev_loop* loop = EV_DEFAULT;
  return jrpc_server_init_with_ev_loop(server, hostname,
                                       port_number, loop);
}

int jrpc_server_init_with_ev_loop(jrpc_server* server, 
                                  const char *hostname,
                                  int port_number,
                                  struct ev_loop *loop) {
  memset(server, 0, sizeof(jrpc_server));
  server->loop = loop;
  server->hostname = strdup(hostname);
  server->port_number = port_number;

#ifdef DEBUG
  jrpc_error *err=&server->error;
  err->code=0;
  memset(err->cause, 0, FIELD_SIZE(jrpc_error,cause));
  memset(err->msg, 0, FIELD_SIZE(jrpc_error,msg));
#endif

  return __jrpc_server_start(server);
}

static
int __jrpc_get_addrinfo(jrpc_server *server,
                        struct addrinfo **addr){
  struct addrinfo hints;
  int rv;
  char str_port[6];
  snprintf(str_port, sizeof(str_port), "%d", server->port_number);

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((rv = getaddrinfo(server->hostname,
                        str_port, &hints, addr)) != 0) {

#ifdef DEBUG
    jrpc_set_error(server, rv, "getaddrinfo", NULL);
#endif

    return rv;
  }

  return 0;
}

static
int __jrpc_server_start(jrpc_server *server) {
  int sockfd, yes=1, rv;
  struct addrinfo *servinfo, *p;

  if( (rv = __jrpc_get_addrinfo(server, &servinfo)) != 0 ) {
    return rv;
  }

  /* Loop through all the results and bind to the first we can */
  for( p = servinfo; p != NULL; p = p->ai_next ) {

    /* Create the accepting socket */
    if( (sockfd = socket(p->ai_family,
                         p->ai_socktype,
                         p->ai_protocol)) == -1 ) {
#ifdef DEBUG
      jrpc_set_error(server, errno, "socket", NULL);
#endif
      return errno;
    }
    
    /* Configure the socket */
    if( setsockopt(sockfd,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &yes,
                   sizeof(int)) == -1 ) {
#ifdef DEBUG
      jrpc_set_error(server, errno, "setsockopt", NULL);
#endif
      return errno;
    }
    
    if( bind(sockfd, p->ai_addr, p->ai_addrlen) == -1 ) {
      close(sockfd);
      continue;
    }
    
    break;
  }

  /* We could not bind to any address */
  if (p == NULL) {
#ifdef DEBUG
    jrpc_set_error(server, -1, "__jrpc_server_start",
                   "Could not bind to an address");
#endif
    freeaddrinfo(servinfo);
    return JRPC_ERROR;
  }

  /* All done with this structure */
  freeaddrinfo(servinfo); 

  if (listen(sockfd, 5) == -1) {
#ifdef DEBUG    
    char *msg=jrpc_new_sprintf("Error listening on fd:%d port:%d\n",
                               sockfd, server->port_number);
    jrpc_set_error(server, errno, "listen", msg);
    free(msg);
#endif
    return JRPC_ERROR;
  }
  
  ev_io_init(&server->listen_watcher, accept_cb, sockfd, EV_READ);
  server->listen_watcher.data = server;
  ev_io_start(server->loop, &server->listen_watcher);
  return 0;
}

// Make the code work with both the old (ev_loop/ev_unloop)
// and new (ev_run/ev_break) versions of libev.
#ifdef EVUNLOOP_ALL
#define EV_RUN ev_loop
#define EV_BREAK ev_unloop
#define EVBREAK_ALL EVUNLOOP_ALL
#else
#define EV_RUN ev_run
#define EV_BREAK ev_break
#endif

void jrpc_server_run(jrpc_server *server){
  EV_RUN(server->loop, 0);
}

int jrpc_server_stop(jrpc_server *server) {
  EV_BREAK(server->loop, EVBREAK_ALL);
  return 0;
}

void jrpc_server_destroy(jrpc_server *server){
  int i;
  for (i = 0; i < server->procedure_count; i++){
    jrpc_procedure_destroy( &(server->procedures[i]) );
  }
  free(server->procedures);
  free(server->hostname);
}

static
void jrpc_procedure_destroy(jrpc_procedure *procedure){
  if (procedure->name){
    free(procedure->name);
    procedure->name = NULL;
  }
  if (procedure->data){
    free(procedure->data);
    procedure->data = NULL;
  }
}

int jrpc_register_procedure(jrpc_server *server,
                            jrpc_function function_pointer,
                            char *name,
                            void *data) {
  int i = server->procedure_count++;

  if ( server->procedures == NULL ) {

    server->procedures = malloc( sizeof(jrpc_procedure) );

  } else {

    jrpc_procedure *ptr = realloc( server->procedures,
                                   ( sizeof(jrpc_procedure) *
                                     server->procedure_count) );
    if ( ptr == NULL ) {
      return -1;
    }

    server->procedures = ptr;

  }

  if ( name != NULL && function_pointer != NULL ) {
    server->procedures[i].name = strdup(name);
    server->procedures[i].function = function_pointer;
    server->procedures[i].data = data;
    return 0;
  } else { 
    return -1;
  }
}

int jrpc_deregister_procedure(jrpc_server *server, char *name) {
  /* Search the procedure to deregister */
  int found = 0;

  if ( server->procedures != NULL ) {

    for ( int i = 0; i < server->procedure_count; i++ ){
      if ( found > 0 ) {
        server->procedures[i-1] = server->procedures[i];
      } else if( !strcmp(name, server->procedures[i].name) ) {
        found = 1;
        jrpc_procedure_destroy( &server->procedures[i] );
      }
    }

    if ( found > 0 ) {

      server->procedure_count--;

      if( server->procedure_count > 0 ) {
        jrpc_procedure *ptr =
          realloc( server->procedures,
                   ( sizeof(jrpc_procedure) *
                     server->procedure_count) );
        if ( ptr == NULL ){
#ifdef DEBUG
          jrpc_set_error(server, -1, "realloc", NULL);
#endif
          return -1;
        }
        server->procedures = ptr;
      }else{
        server->procedures = NULL;
      }
    }

  } else {

#ifdef DEBUG
    jrpc_set_error(server, -1, "jrpc_deregister_procedure",
                   "Procedure not found");
#endif

    return -1;

  }
  return 0;
}
