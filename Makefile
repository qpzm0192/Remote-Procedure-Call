
default: librpc.a binder

librpc.a: rpc.h rpcfunc.cpp
	g++ -c rpcfunc.cpp -o rpcfunc.o
	ar rc librpc.a rpcfunc.o
	
binder: binder.cpp
	g++ binder.cpp -o binder
	

