# Simple socket echo server

This is a non concurrent socket server where the server echos back what ever the client sends.  The client send 10 HELLOs to server and writes the responses back.

To start it as a server, run (without args)

```
 ./sock 
```

To invoke as a client, start with name of the server machine (localhost if server is running on same machine)

```
 ./sock localhost
```

