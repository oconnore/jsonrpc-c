jsonrpc-c
=========

JSON-RPC in C (server only for now)

What?
-----
A library for a C program to receive JSON-RPC requests on tcp sockets (no HTTP).

Free software, MIT license.

Why?
----
I needed a way for an application written in C, running on an embedded Linux system to be configured by
a Java/Swing configuration tool running on a connected laptop. Wanted something simple, human readable,
and saw no need for HTTP.

How?
----
It depends on libev and Jansson.

###Testing

Run `autoreconf -i`  before `./configure` and `make`

Test the example server by running it and typing: 

`echo '{"jsonrpc":"2.0","method":"sayHello","params":["Foo"],"id":340958}' | nc localhost 1234`
`{"jsonrpc":"2.0","result":"Hello Foo!\n","id":340958}`

or

`echo '{"jsonrpc":"2.0","method":"exit"}' | nc localhost 1234`

Who?
----

@hmngomes
@eric_oconnor
