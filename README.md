# moch - Minimalist, Old school CHat

Simple TCP/IP chat server/client for UNIX systems.

## usage

```
sudo moch_srv -p <port>
```
```
moch_clnt -a <server_address> -p <server_port> -n <nick_name>
```

## compilation
ncurses lib is needed for the client.
```
make
```
The executable files will be in the build directory.
