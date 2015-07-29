#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string>
#include <arpa/inet.h>
#include <iostream>
#include <limits.h>
#include <sstream>
#include <iomanip>
#include <vector>
#include <map>
#include <math.h>

#include "rpc.h"


#define MAX_NAME 64
#define MAX_BUFFER 1024
#define PORT 0
#define MAX_CLIENT 5

using namespace std;

int binderSocket;
int listenSocket;
char address[MAX_BUFFER];
int port;
char *binderAddress;
char *binderPort;


map<string, skeleton> functions;


char* f2(float a, double b) {

  float ai;
  double bi;
  char *str1;
  char *str2;

  a = modff(a, &ai);
  b = modf(b, &bi);

  str1 = (char *)malloc(100);

  sprintf(str1, "%lld%lld", (long long)ai, (long long)bi);

  return str1;
}

int f2_Skel(int *argTypes, void **args) {

  // (char *)*args = f2( *((float *)(*(args + 1))), *((double *)(*(args + 2))) ); 
  *args = f2( *((float *)(*(args + 1))), *((double *)(*(args + 2))) );

  return 0;
}




void conToByte(int i, unsigned char* bytes) {
	bytes[0] = (i >> 24);	//high
	bytes[1] = (i >> 16);	//	
	bytes[2] = (i >> 8);	//
	bytes[3] = i;			//low
}

int conToInt(char * bytes) {
    unsigned char * new_bytes = (unsigned char *) bytes;
    int Int32 = 0;

    Int32 = (Int32 << 8) + new_bytes[0];
    Int32 = (Int32 << 8) + new_bytes[1];
    Int32 = (Int32 << 8) + new_bytes[2];
    Int32 = (Int32 << 8) + new_bytes[3];
    return Int32;
}

int conToInt(unsigned char * bytes) {
    int Int32 = 0;

    Int32 = (Int32 << 8) + bytes[0];
    Int32 = (Int32 << 8) + bytes[1];
    Int32 = (Int32 << 8) + bytes[2];
    Int32 = (Int32 << 8) + bytes[3];
    return Int32;
}

//------------------------------------------------------------------------------------------------------
//										RPC INIT
//------------------------------------------------------------------------------------------------------

int rpcInit() {
	
	//--------------------------------------------
	//	Create a Binder Socket
	//--------------------------------------------
	
	// get address and portnum
	char *binderAddress = getenv("BINDER_ADDRESS");
	char *binderPort = getenv("BINDER_PORT");

	struct sockaddr_in mysockaddr;
	struct hostent *hp;
	hp = gethostbyname(binderAddress);

	memset(&mysockaddr,0,sizeof(mysockaddr));
	memcpy((char *)&mysockaddr.sin_addr,hp->h_addr,hp->h_length);
	mysockaddr.sin_port = atoi(binderPort);
	mysockaddr.sin_family = AF_INET;

	// create socket
	binderSocket = socket(AF_INET,SOCK_STREAM,0);

	// connect
	if (connect(binderSocket,(struct sockaddr *)&mysockaddr,sizeof(mysockaddr)) == -1) { //connect
		close(binderSocket);
		perror("rpcInit connect to binder");
		return -1;
	}
	

	//--------------------------------------------
	//	Create a Client Socket to Listen
	//--------------------------------------------
	
	struct sockaddr_in server_addr;
	char hostname[MAX_BUFFER];
	
	listenSocket = socket(AF_INET, SOCK_STREAM, 0); //socket
	
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(PORT);
	if (bind(listenSocket, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
		perror("rpcInit bind to listenSocket");
		return -2;
	}; //bind
	
	//print SERVER_ADDRESS
	gethostname(address,MAX_BUFFER);
	//cout << "SERVER_ADDRESS " << address  << endl;
	
	//print SERVER_PORT
	socklen_t addrlen = sizeof(server_addr);
	if (getsockname(listenSocket, (struct sockaddr *)&server_addr, &(addrlen)) == -1) perror("getsockname");
	port = 	ntohs(server_addr.sin_port);
	//cout << "SERVER_PORT " << port << endl;
	
	return 0;
}

//------------------------------------------------------------------------------------------------------
//										RPC CALL
//------------------------------------------------------------------------------------------------------
int getArrayLength(int argType) {
	unsigned char byte[4];
	conToByte(argType, byte);
	byte[0] = 0;
	byte[1] = 0;
	return conToInt(byte);
}

int getType(int argType) {
	unsigned char byte[4];
	conToByte(argType, byte);
	byte[3] = byte[1];
	byte[2] = 0;
	byte[1] = 0;
	byte[0] = 0;
	return conToInt(byte);
}

int constructExcuteMsg(char* name, int* argTypes, void** args, int socket) {
	
	//[length]EXECUTE[length]name[length]argTypes[length]args
	string msg;
	
	//[length]EXECUTE	
	string request("EXECUTE");
	int len = request.length();
	unsigned char byte[4];
	conToByte(len, byte);
	string hex_len(reinterpret_cast<char*>(byte), 4);
	
	msg.append(hex_len);
	msg.append(request);
	//[length]name
	string strName(name);
	len = strName.length();
	conToByte(len, byte);
	string hex_len2(reinterpret_cast<char*>(byte), 4);
	
	msg.append(hex_len2);
	msg.append(strName);
	
	//argTypes
	int size=0;
	while(argTypes[size] != 0){
		size++;
	}
	
	conToByte(size, byte);
	string hex_len3(reinterpret_cast<char*>(byte), 4);
	msg.append(hex_len3);
	
	for(int i=0; i < size; i++) {
		conToByte(argTypes[i], byte);		
		string fourByteChar(reinterpret_cast<char*>(byte), 4);
		msg.append(fourByteChar);
	}
	
	//cout << "msg length: " << msg.length() << endl;
	
	int byteSend;
	if((byteSend = send(socket, msg.c_str(), msg.length(), 0)) == -1) {
		perror("constructExcuteMsg send to sever:");
	}
	//cout << "byteSend: " << byteSend << endl;
	
	//args
	for(int i=0; i < size; i++) {
		int arrayLength = getArrayLength(argTypes[i]);
		//cout << "arrayLength: " << arrayLength << endl;
		
		int type = getType(argTypes[i]);
		//cout << "type: " << type << endl; 
		
		if(arrayLength == 0) { //scalar
			//cout << "is a scalar" << endl;
			
			unsigned char* newbyte = reinterpret_cast<unsigned char *>(args[i]);
			switch (type) {
				case 1:
					//cout << "value is: " << *((char *)args[i]) << endl;
					if(write(socket, newbyte, sizeof(char)) == -1) {
						perror("s1-constructExcuteMsg-write-arg:");
					}
					break;
				case 2:
					//cout << "value is: " << *((short *)args[i]) << endl;
					if(write(socket, newbyte, sizeof(short)) == -1) {
						perror("s2-constructExcuteMsg-write-arg:");
					}
					break;				
				case 3:
					//cout << "value is: " << *((int *)args[i]) << endl;
					if(write(socket, newbyte, sizeof(int)) == -1) {
						perror("constructExcuteMsg-write-arg:");
					}
					break;
				case 4:
					//cout << "value is: " << *((long *)args[i]) << endl;
					if(write(socket, newbyte, sizeof(long)) == -1) {
						perror("constructExcuteMsg-write-arg:");
					}
					break;
				case 5:
					//cout << "value is: " << *((double *)args[i]) << endl;
					if(write(socket, newbyte, sizeof(double)) == -1) {
						perror("constructExcuteMsg-write-arg:");
					}
					break;
				case 6:
					//cout << "value is: " << *((float *)args[i]) << endl;
					if(write(socket, newbyte, sizeof(float)) == -1) {
						perror("constructExcuteMsg-write-arg:");
					}
					break;
				default:
					//cout << "Wrong type!!" << endl;
					break;
			}

		} else {
			//cout << "is an array" << endl;
			if(type == 1) {
				char *argArray = ((char *)args[i]);
				for(int j=0; j<arrayLength; j++) {
					unsigned char* newbyte = reinterpret_cast<unsigned char *>(&argArray[j]);
					if(write(socket, newbyte, sizeof(char)) == -1) {
						perror("constructResultMsg-write-arg:");
					}
				}
			} else if(type == 2) {
				short *argArray = ((short *)args[i]);
				for(int j=0; j<arrayLength; j++) {
					unsigned char* newbyte = reinterpret_cast<unsigned char *>(&argArray[j]);
					if(write(socket, newbyte, sizeof(short)) == -1) {
						perror("constructResultMsg-write-arg:");
					}
				}					
			} else if(type == 3) {
				int *argArray = ((int *)args[i]);
				for(int j=0; j<arrayLength; j++) {
					unsigned char* newbyte = reinterpret_cast<unsigned char *>(&argArray[j]);
					if(write(socket, newbyte, sizeof(int)) == -1) {
						perror("constructResultMsg-write-arg:");
					}
				}					
			}
			 else if(type == 4) {
				long *argArray = ((long *)args[i]);
				for(int j=0; j<arrayLength; j++) {
					unsigned char* newbyte = reinterpret_cast<unsigned char *>(&argArray[j]);
					if(write(socket, newbyte, sizeof(long)) == -1) {
						perror("constructResultMsg-write-arg:");
					}
				}					
			}
			 else if(type == 5) {
				double *argArray = ((double *)args[i]);
				for(int j=0; j<arrayLength; j++) {
					unsigned char* newbyte = reinterpret_cast<unsigned char *>(&argArray[j]);
					if(write(socket, newbyte, sizeof(double)) == -1) {
						perror("constructResultMsg-write-arg:");
					}
				}					
			}
			 else if(type == 6) {
				float *argArray = ((float *)args[i]);
				for(int j=0; j<arrayLength; j++) {
					unsigned char* newbyte = reinterpret_cast<unsigned char *>(&argArray[j]);
					if(write(socket, newbyte, sizeof(float)) == -1) {
						perror("constructResultMsg-write-arg:");
					}
				}					
			} else {
				//cout << "Wrong type!!" << endl;
			}
		}			
			
			
			/*
			long *argArray = ((long *)args[i]);
			for(int i=0; i<arrayLength; i++) {
				unsigned char* newbyte = reinterpret_cast<unsigned char *>(&argArray[i]);
				switch (type) {
					case 1:
						//cout << "value is: " << ((char )argArray[i]) << endl;
						if(write(socket, newbyte, sizeof(char)) == -1) {
							perror("Call-write-arg:");
						}
						break;
					case 2:
						//cout << "value is: " << *((short *)argArray[i]) << endl;
						if(write(socket, newbyte, sizeof(short)) == -1) {
							perror("Call-write-arg:");
						}
						break;				
					case 3:
						//cout << "value is: " << *((int *)argArray[i]) << endl;
						if(write(socket, newbyte, sizeof(int)) == -1) {
							perror("Call-write-arg:");
						}
						break;
					case 4:
						//cout << "value is: " << ((long )argArray[i]) << endl;
						if(write(socket, newbyte, sizeof(long)) == -1) {
							perror("Call-write-arg:");
						}
						break;
					case 5:
						//cout << "value is: " << *((double *)argArray[i]) << endl;
						if(write(socket, newbyte, sizeof(double)) == -1) {
							perror("Call-write-arg:");
						}
						break;
					case 6:
						//cout << "value is: " << *((float *)argArray[i]) << endl;
						if(write(socket, newbyte, sizeof(float)) == -1) {
							perror("Call-write-arg:");
						}
						break;
					default:
						//cout << "Wrong type!!" << endl;
						break;
				}
	
			}
			*/
		
	}
	return 0;
}

int rpcCall(char* name, int* argTypes, void** args) {
	int byteSend, byteRecv;  
	string msg;
	char buffer[MAX_BUFFER];
	string serverAddress;
	string serverPort;
	
	//---------------------------------------------------------------------
	//make a connection to binder
	//---------------------------------------------------------------------
	char *binderAddress = getenv("BINDER_ADDRESS");
	char *binderPort = getenv("BINDER_PORT");

	struct sockaddr_in mysockaddr;
	struct hostent *hp;
	hp = gethostbyname(binderAddress);

	memset(&mysockaddr,0,sizeof(mysockaddr));
	memcpy((char *)&mysockaddr.sin_addr,hp->h_addr,hp->h_length);
	mysockaddr.sin_port = atoi(binderPort);
	mysockaddr.sin_family = AF_INET;

	// create socket
	binderSocket = socket(AF_INET,SOCK_STREAM,0);

	// connect
	if (connect(binderSocket,(struct sockaddr *)&mysockaddr,sizeof(mysockaddr)) == -1) { //connect
		close(binderSocket);
		perror("rpcCall connect to binder");
		return -1;
	}
	//
	
	//construct a message
	//[length]LOC_REQUEST[length]name[length]argTypes
	
	//[length]LOC_REQUEST	
	string request("LOC_REQUEST");
	int leng = request.length();
	unsigned char byte[4];
	conToByte(leng, byte);
	string hex_len(reinterpret_cast<char*>(byte), 4);
	
	msg.append(hex_len);
	msg.append(request);
	//[length]name
	string strName(name);
	leng = strName.length();
	conToByte(leng, byte);
	string hex_len4(reinterpret_cast<char*>(byte), 4);
	
	msg.append(hex_len4);
	msg.append(strName);
	
	//[length]argTypes
	int size = 0;
	while(argTypes[size] != 0) {
		size++;
	}

	conToByte(size, byte);
	string hex_len5(reinterpret_cast<char*>(byte), 4);
	msg.append(hex_len5);
	
	for(int i=0; i < size; i++) {
		conToByte(argTypes[i], byte);
		string fourByteChar(reinterpret_cast<char*>(byte), 4);
		msg.append(fourByteChar);
	}
	
	//send msg
	if(send(binderSocket, msg.c_str(), msg.length()+1, 0) == -1) {
		perror("Call-send:");
		return -1;
	}

	
	//recv feedback
	char lenBuf[4];
	if(read(binderSocket, lenBuf, 4) == -1) {
		perror("Call-read-1:");
		return -1;
	}
	
	if(read(binderSocket, buffer, conToInt(lenBuf)) == -1) {
		perror("Call-read-2:");
		return -1;
	}

	buffer[conToInt(lenBuf)] = 0;


	string reply(buffer);
	if(reply.compare("LOC_SUCCESS") == 0) {
		//cout << "LOC_SUCCESS" << endl;
		
		//server-id
		if(read(binderSocket, lenBuf, 4) == -1) {
			perror("Call-read-1:");
			return -1;
		}
		
		//cout << "server id len: " << conToInt(lenBuf) << endl;
		
		if(read(binderSocket, buffer, conToInt(lenBuf)) == -1) {
			perror("Call-read-2:");
			return -1;
		}
		buffer[conToInt(lenBuf)] = 0;
		serverAddress.assign(buffer);
		//cout << "Address: " << serverAddress << endl;
		
		//server port
		if(read(binderSocket, lenBuf, 4) == -1) {
			perror("Call-read-1:");
			return -1;
		}

		//cout << "port len: " << conToInt(lenBuf) << endl;
		
		if(read(binderSocket, buffer, conToInt(lenBuf)) == -1) {
			perror("Call-read-2:");
			return -1;
		}
		buffer[conToInt(lenBuf)] = 0;
		serverPort.assign(buffer);
		//cout << "Port: " << serverPort << endl;
		
	} else {
		//cout << "LOC_FAILURE" << endl;
		return -2;
	}
	//////////////////////////////////////////////////////////////////////////////////////
	//make a connection to server
	//////////////////////////////////////////////////////////////////////////////////////
	

	int bytes_sent, bytes_recv, serverSocket;  
    struct addrinfo hints, *servinfo, *p;
    int rv;

	
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(serverAddress.c_str(), serverPort.c_str(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -3;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((serverSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) { //socket
            perror("client: socket");
            continue;
        }
	
        if (connect(serverSocket, p->ai_addr, p->ai_addrlen) == -1) { //connect
            close(serverSocket);
            perror("client: connect");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return -3;
    }

	//construct  and send msg
	int result = constructExcuteMsg(name, argTypes, args, serverSocket);
	
	//-----------------------------------------------------------
	//receive reply
	//-----------------------------------------------------------
	char len[4];
	
	if(read(serverSocket, len, 4) == -1) {
		perror("Call-read-2:");
		return -3;
	}
	
	//cout << "read length: " << conToInt(len) << endl;
	if(read(serverSocket, buffer, conToInt(len))  <= 0) {
		perror("Execute-read-1:");
		return -3;
	}
	buffer[conToInt(len)] = 0;
	string replyMsg(buffer);
	//cout << replyMsg << endl;
	if(replyMsg.compare("EXECUTE_SUCCESS") == 0) {
		if(read(serverSocket, len, 4)  <= 0) {
			perror("Call-read-2");
			return -3;
		}
		char namebuf[65];
		if(read(serverSocket, namebuf, conToInt(len))  <= 0) {
			perror("Call-read-3");
			return -3;
		}
		namebuf[conToInt(len)] = 0;
		string recvName(namebuf);
		//cout << "name: " << recvName << endl;
		if(recvName.compare(name) != 0) {
			//cout << "function name different!!" << endl;
		}
		
		if(read(serverSocket, len, 4)  <= 0) {
			perror("Call-read-4");
			return -3;
		} 

		int numArg = conToInt(len);

		
		//cout << "size of args: " << numArg << endl;

		for (int j=0; j < numArg; j++) {
			if(read(serverSocket, len, 4)  <= 0) {
				perror("Call-read-5");
				return -3;
			}
		}

		for (int j=0; j < numArg; j++) {
			int argtype = argTypes[j];
			
			int arrayLength = getArrayLength(argtype);
			//cout << "arrayLength: " << arrayLength << endl;
			
			int type = getType(argtype);
			//cout << "type: " << type << endl;
			
			unsigned char usArg[4];
			if(arrayLength == 0) {
				
				if(type == 1) {
					if(read(serverSocket, usArg, sizeof(char))  <= 0) {
						perror("Call-read-5");
						return -3;
					}
					
					char *arg;
					arg = (char *)malloc(sizeof(char));
					*arg = *((char*)(usArg));
					args[j] = (void *) arg;
					//cout << "value is: " << *arg << endl;
				} else if(type == 2) {
					if(read(serverSocket, usArg, sizeof(short))  <= 0) {
						perror("Call-read-5");
						return -3;
					}
					
					short *arg;
					arg = (short *)malloc(sizeof(short));
					*arg = *((short*)(usArg));
					args[j] = (void *) arg;
					//cout << "value is: " << *arg << endl;
				} else if(type == 3) {
					if(read(serverSocket, usArg, sizeof(int))  <= 0) {
						perror("Call-read-5");
						return -3;
					}
					
					int *arg;
					arg = (int *)malloc(sizeof(int));
					*arg = *((int*)(usArg));
					args[j] = (void *) arg;
					//cout << "value is: " << *arg << endl;
				} else if(type == 4) {
					if(read(serverSocket, usArg, sizeof(long))  <= 0) {
						perror("Call-read-5");
						return -3;
					}
					
					long *arg;
					arg = (long *)malloc(sizeof(long));
					*arg = *((long*)(usArg));
					args[j] = (void *) arg;
					//cout << "value is: " << *arg << endl;
				} else if(type == 5) {
					if(read(serverSocket, usArg, sizeof(double))  <= 0) {
						perror("Call-read-5");
						return -3;
					}
					
					double *arg;
					arg = (double *)malloc(sizeof(double));
					*arg = *((double*)(usArg));
					args[j] = (void *) arg;
					//cout << "value is: " << *arg << endl;										
				} else if(type == 6) {
					if(read(serverSocket, usArg, sizeof(float))  <= 0) {
						perror("Call-read-5");
						return -3;
					}
					
					float *arg;
					arg = (float *)malloc(sizeof(float));
					*arg = *((float*)(usArg));
					args[j] = (void *) arg;
					//cout << "value is: " << *arg << endl;										
				} else {
					//cout << "unknown arg type!!" << endl;										
				}								
				
			} else {
				if(type == 1) {
					char *arg;
					arg = (char *) malloc(arrayLength * sizeof(char ));
					for(int k=0; k < arrayLength; k++) {
						if(read(serverSocket, usArg, sizeof(char))  <= 0) {
							perror("Call-read-5");
							return -3;
						}
						
						arg[k] = *((char *)(usArg));
						//cout << "value is: " << arg[k] << endl;
					}
					args[j] = (void *) arg;	
				} else if(type == 2) {
					short *arg;
					arg = (short *) malloc(arrayLength * sizeof(short ));
					for(int k=0; k < arrayLength; k++) {
						if(read(serverSocket, usArg, sizeof(short))  <= 0) {
							perror("Call-read-5");
							return -3;
						}
						
						arg[k] = *((short *)(usArg));
						//cout << "value is: " << arg[k] << endl;
					}
					args[j] = (void *) arg;	
				} else if(type == 3) {
					int *arg;
					arg = (int *) malloc(arrayLength * sizeof(int ));
					for(int k=0; k < arrayLength; k++) {
						if(read(serverSocket, usArg, sizeof(int))  <= 0) {
							perror("Call-read-5");
							return -3;
						}
						
						arg[k] = *((int *)(usArg));
						//cout << "value is: " << arg[k] << endl;
					}
					args[j] = (void *) arg;
					
				} else if(type == 4) {
					long *arg;
					arg = (long *) malloc(arrayLength * sizeof(long ));
					for(int k=0; k < arrayLength; k++) {
						if(read(serverSocket, usArg, sizeof(long))  <= 0) {
							perror("Call-read-5");
							return -3;
						}
						
						arg[k] = *((long *)(usArg));
						//cout << "value is: " << arg[k] << endl;
					}
					args[j] = (void *) arg;										
				} else if(type == 5) {
					double *arg;
					arg = (double *) malloc(arrayLength * sizeof(double ));
					for(int k=0; k < arrayLength; k++) {
						if(read(serverSocket, usArg, sizeof(double))  <= 0) {
							perror("Call-read-5");
							return -3;
						}
						
						arg[k] = *((double *)(usArg));
						//cout << "value is: " << arg[k] << endl;
					}
					args[j] = (void *) arg;
					
				} else if(type == 6) {
					float *arg;
					arg = (float *) malloc(arrayLength * sizeof(float ));
					for(int k=0; k < arrayLength; k++) {
						if(read(serverSocket, usArg, sizeof(float))  <= 0) {
							perror("Call-read-5");
							return -3;
						}
						
						arg[k] = *((float *)(usArg));
						//cout << "value is: " << arg[k] << endl;
					}
					args[j] = (void *) arg;
					
				} else {
					//cout << "unknown arg type!!" << endl;
				}
			}
		}
		
		return 0;
	} else {
		unsigned char errorBuf[4];
		if(read(serverSocket, errorBuf, 4)  <= 0) {
			perror("Call-read-5");
			return -3;
		}
		int error = conToInt(errorBuf);
		return error;
	}
	
}



int rpcCacheCall(char* name, int* argTypes, void** args);

//------------------------------------------------------------------------------------------------------
//										RPC REGISTER
//------------------------------------------------------------------------------------------------------

int rpcRegister(char* name, int* argTypes, skeleton f) {
	char buffer[MAX_BUFFER];
	string msg;
	int byteRecv;
	//[length]REGISTER[length]server_identifier[length]port[length]name[length]argTypes
	
	//[length]REGISTER	
	string request("REGISTER");
	int len = request.length();
	unsigned char byte[4];
	conToByte(len, byte);
	string hex_len(reinterpret_cast<char*>(byte), 4);
	
	msg.append(hex_len);
	msg.append(request);
	
	//[length]server_identifier
	string serverID(address);
	len = serverID.length();
	conToByte(len, byte);
	string hex_len2(reinterpret_cast<char*>(byte), 4);
	
	msg.append(hex_len2);
	msg.append(serverID);

	//[length]port
	stringstream ss;
	ss << port;
	string strNum;
	ss >> strNum;
	len = strNum.length();
	conToByte(len, byte);
	string hex_len3(reinterpret_cast<char*>(byte), 4);
	
	msg.append(hex_len3);
	msg.append(strNum);
	
	//[length]name
	string strName(name);
	len = strName.length();
	conToByte(len, byte);
	string hex_len4(reinterpret_cast<char*>(byte), 4);
	
	msg.append(hex_len4);
	msg.append(strName);
	
	//[length]argTypes
	int size = 0;
	while(argTypes[size] != 0) {
		size++;
	}
	//cout << "argType size: " << size << endl;
	conToByte(size, byte);
	string hex_len5(reinterpret_cast<char*>(byte), 4);
	msg.append(hex_len5);
	
	for(int i=0; i < size; i++) {
		conToByte(argTypes[i], byte);
		string fourByteChar(reinterpret_cast<char*>(byte), 4);
		msg.append(fourByteChar);
	}
	
	//cout << "msg length: " << msg.length() << endl;
	
	//send msg
	if(send(binderSocket, msg.c_str(), msg.length()+1, 0) == -1) {
		perror("Register-send:");
		return -1;
	}
	
	//recv feedback
	if((byteRecv = read(binderSocket, buffer, 16)) < 0) {
		perror("Register-recv:");
		return -2;
	}
	buffer[16] = 0;
	
	
	string reply(buffer);
	//cout << reply << endl;
	if(reply.compare("REGISTER_SUCCESS") == 0) {
		functions[strName] = f;
		//cout << "REGISTER_SUCCESS" << endl;
		char error[2];
		if(read(binderSocket, error, 2) < 0) {
			perror("registration success error");
			return -2;
		}
		error[2] = 0;
		string errorCode(error);
		if(errorCode.compare("00") == 0) {
			return 0;
		} else if(errorCode.compare("01") == 0) {
			//cout << "01" << endl;
			return 1;
		} else if(errorCode.compare("02") == 0) {
			return 2;
		}
		
	} else {
		//cout << "REGISTER_FAILURE" << endl;
		return -3;
	}
	msg.clear();	
}

//------------------------------------------------------------------------------------------------------
//										RPC EXECUTE
//------------------------------------------------------------------------------------------------------

int constructResultMsg(char* name, int* argTypes, void** args, int socket) {
	
	//cout << "-------------start constructing EXECUTE Reply------------------" << endl;
	
	//[length]EXECUTE[length]name[length]argTypes[length]args
	string msg;
	
	//[length]EXECUTE_SUCCESS	
	string request("EXECUTE_SUCCESS");
	int len = request.length();
	unsigned char byte[4];
	conToByte(len, byte);
	string hex_len(reinterpret_cast<char*>(byte), 4);
	
	msg.append(hex_len);
	msg.append(request);
	//[length]name
	string strName(name);
	len = strName.length();
	conToByte(len, byte);
	string hex_len2(reinterpret_cast<char*>(byte), 4);
	
	msg.append(hex_len2);
	msg.append(strName);
	
	//argTypes
	int size=0;
	while(argTypes[size] != 0){
		size++;
	}
	
	conToByte(size, byte);
	string hex_len3(reinterpret_cast<char*>(byte), 4);
	msg.append(hex_len3);
	
	for(int i=0; i < size; i++) {
		conToByte(argTypes[i], byte);		
		string fourByteChar(reinterpret_cast<char*>(byte), 4);
		msg.append(fourByteChar);
	}
	
	//cout << "msg length: " << msg.length() << endl;
	
	int byteSend;
	if((byteSend = send(socket, msg.c_str(), msg.length(), 0)) == -1) {
		perror("Execute send to client:");
	}
	//cout << "byteSend: " << byteSend << endl;
	
	//args
	for(int i=0; i < size; i++) {
		int arrayLength = getArrayLength(argTypes[i]);
		//cout << "arrayLength: " << arrayLength << endl;
		
		int type = getType(argTypes[i]);
		//cout << "type: " << type << endl; 
		
		if(arrayLength == 0) { //scalar
			//cout << "is a scalar" << endl;
			
			unsigned char* newbyte = reinterpret_cast<unsigned char *>(args[i]);
			switch (type) {
				case 1:
					//cout << "value is: " << *((char *)args[i]) << endl;
					if(write(socket, newbyte, sizeof(char)) == -1) {
						perror("constructResultMsg-write-arg");
					}
					break;
				case 2:
					//cout << "value is: " << *((short *)args[i]) << endl;
					if(write(socket, newbyte, sizeof(short)) == -1) {
						perror("constructResultMsg-write-arg");
					}
					break;				
				case 3:
					//cout << "value is: " << *((int *)args[i]) << endl;
					if(write(socket, newbyte, sizeof(int)) == -1) {
						perror("constructResultMsg-write-arg");
					}
					break;
				case 4:
					//cout << "value is: " << *((long *)args[i]) << endl;
					if(write(socket, newbyte, sizeof(long)) == -1) {
						perror("constructResultMsg-write-arg");
					}
					break;
				case 5:
					//cout << "value is: " << *((double *)args[i]) << endl;
					if(write(socket, newbyte, sizeof(double)) == -1) {
						perror("constructResultMsg-write-arg");
					}
					break;
				case 6:
					//cout << "value is: " << *((float *)args[i]) << endl;
					if(write(socket, newbyte, sizeof(float)) == -1) {
						perror("constructResultMsg-write-arg");
					}
					break;
				default:
					//cout << "Wrong type!!" << endl;
					break;
			}

		} else {
			if(type == 1) {
				char *argArray = ((char *)args[i]);
				for(int j=0; j<arrayLength; j++) {
					unsigned char* newbyte = reinterpret_cast<unsigned char *>(&argArray[j]);
					if(write(socket, newbyte, sizeof(char)) == -1) {
						perror("constructResultMsg-write-arg:");
					}
				}
			}else if(type == 2) {
				short *argArray = ((short *)args[i]);
				for(int j=0; j<arrayLength; j++) {
					unsigned char* newbyte = reinterpret_cast<unsigned char *>(&argArray[j]);
					if(write(socket, newbyte, sizeof(short)) == -1) {
						perror("constructResultMsg-write-arg:");
					}
				}					
			} else if(type == 3) {
				int *argArray = ((int *)args[i]);
				for(int j=0; j<arrayLength; j++) {
					unsigned char* newbyte = reinterpret_cast<unsigned char *>(&argArray[j]);
					if(write(socket, newbyte, sizeof(int)) == -1) {
						perror("constructResultMsg-write-arg:");
					}
				}					
			}else if(type == 4) {
				long *argArray = ((long *)args[i]);
				for(int j=0; j<arrayLength; j++) {
					unsigned char* newbyte = reinterpret_cast<unsigned char *>(&argArray[j]);
					if(write(socket, newbyte, sizeof(long)) == -1) {
						perror("constructResultMsg-write-arg:");
					}
				}					
			}else if(type == 5) {
				double *argArray = ((double *)args[i]);
				for(int j=0; j<arrayLength; j++) {
					unsigned char* newbyte = reinterpret_cast<unsigned char *>(&argArray[j]);
					if(write(socket, newbyte, sizeof(double)) == -1) {
						perror("constructResultMsg-write-arg:");
					}
				}					
			}else if(type == 6) {
				float *argArray = ((float *)args[i]);
				for(int j=0; j<arrayLength; j++) {
					unsigned char* newbyte = reinterpret_cast<unsigned char *>(&argArray[j]);
					if(write(socket, newbyte, sizeof(float)) == -1) {
						perror("constructResultMsg-write-arg:");
					}
				}					
			} else {
				//cout << "Wrong type!!" << endl;
			}
			
		}
	}
	return 0;
}


int rpcExecute(){
	int error;
	
	//print map
	for(map<string, skeleton>::const_iterator it = functions.begin();
		it != functions.end(); ++it)
	{
		//cout << it->first << " " << it->second << "\n";
	}
	///////////////////////////////////////////////////////////////////////
	
	fd_set master, read_fds;
	int fdmax, newfd, i;
	struct sockaddr_storage remoteaddr;
	socklen_t addrlen;
	
	if(listen(listenSocket, MAX_CLIENT) < 0) {
		perror("execute-listen:");
	}; //listen
	// add the listener to the master set
    FD_SET(listenSocket, &master);

    // keep track of the biggest file descriptor
    fdmax = listenSocket; // so far, it's this one
	
	FD_SET(binderSocket, &master); // add binderSocket to master set
	if (fdmax < binderSocket) {    
		fdmax = binderSocket;
	}
	while(1) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listenSocket) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
					
                    newfd = accept(listenSocket, (struct sockaddr *)&remoteaddr, &addrlen); //accept
                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (fdmax < newfd) {    
                            fdmax = newfd;
                        }
                    }
                } else {
					char buffer[MAX_BUFFER];
					char len[4];
					int bytes_recv;
					if((bytes_recv = read(i, len, 4)) <= 0){
                        if (bytes_recv == 0) {
                            printf("selectserver: socket %d hung up\n", i);
                        } else {
                            perror("read");
                        }
                        close(i);
                        FD_CLR(i, &master);
                    }					
					else {
						//process data
						//cout << "read length: " << conToInt(len) << endl;
						if(read(i, buffer, conToInt(len))  <= 0) {
							perror("Execute-read-1:");
						}
						buffer[conToInt(len)] = 0;
						string msg(buffer);
						//cout << msg << endl;
						if(msg.compare("EXECUTE") == 0) {
							
						
								if(read(i, len, 4)  <= 0) {
									perror("Execute-read-2");
								}
								char namebuf[65];
								if(read(i, namebuf, conToInt(len))  <= 0) {
									perror("Execute-read-3");
								}
								namebuf[conToInt(len)] = 0;
								string name(namebuf);
								//cout << "name: " << name << endl;
								
								if(read(i, len, 4)  <= 0) {
									perror("Execute-read-4");
								} 

								int numArg = conToInt(len);
								int argTypes[numArg+1];
								void **args;
								args = (void **)malloc(numArg * sizeof(void *));
								
								//cout << "size of args: " << numArg << endl;
		
								for (int j=0; j < numArg; j++) {
									if(read(i, len, 4)  <= 0) {
										perror("Execute-read-5");
									}

									argTypes[j] = conToInt(len);
								}
								argTypes[numArg] = 0;

								for (int j=0; j < numArg; j++) {
									int argtype = argTypes[j];
									
									int arrayLength = getArrayLength(argtype);
									//cout << "arrayLength: " << arrayLength << endl;
									
									int type = getType(argtype);
									//cout << "type: " << type << endl;
									
									unsigned char usArg[4];
									if(arrayLength == 0) {
										
										if(type == 1) {
											if(read(i, usArg, sizeof(char))  <= 0) {
												perror("Execute-read-5");
											}
											
											char *arg;
											arg = (char *)malloc(sizeof(char));
											*arg = *((char*)(usArg));
											args[j] = (void *) arg;
											//cout << "value is: " << *arg << endl;
										} else if(type == 2) {
											if(read(i, usArg, sizeof(short))  <= 0) {
												perror("Execute-read-5");
											}
											
											short *arg;
											arg = (short *)malloc(sizeof(short));
											*arg = *((short*)(usArg));
											args[j] = (void *) arg;
											//cout << "value is: " << *arg << endl;
										} else if(type == 3) {
											if(read(i, usArg, sizeof(int))  <= 0) {
												perror("Execute-read-5");
											}
											
											int *arg;
											arg = (int *)malloc(sizeof(int));
											*arg = *((int*)(usArg));
											args[j] = (void *) arg;
											//cout << "value is: " << *arg << endl;
										} else if(type == 4) {
											if(read(i, usArg, sizeof(long))  <= 0) {
												perror("Execute-read-5");
											}
											
											long *arg;
											arg = (long *)malloc(sizeof(long));
											*arg = *((long*)(usArg));
											args[j] = (void *) arg;
											//cout << "value is: " << *arg << endl;
										} else if(type == 5) {
											if(read(i, usArg, sizeof(double))  <= 0) {
												perror("Execute-read-5");
											}
											
											double *arg;
											arg = (double *)malloc(sizeof(double));
											*arg = *((double*)(usArg));
											args[j] = (void *) arg;
											//cout << "value is: " << *arg << endl;										
										} else if(type == 6) {
											if(read(i, usArg, sizeof(float))  <= 0) {
												perror("Execute-read-5");
											}
											
											float *arg;
											arg = (float *)malloc(sizeof(float));
											*arg = *((float*)(usArg));
											args[j] = (void *) arg;
											//cout << "value is: " << *arg << endl;										
										} else {
											error = 1;
											//cout << "unknown arg type!!" << endl;										
										}								
										
									} else {
										if(type == 1) {
											char *arg;
											arg = (char *) malloc(arrayLength * sizeof(char ));
											for(int k=0; k < arrayLength; k++) {
												if(read(i, usArg, sizeof(char))  <= 0) {
													perror("Execute-read-5");
												}
												
												arg[k] = *((char *)(usArg));
												//cout << "value is: " << arg[k] << endl;
											}
											args[j] = (void *) arg;	
										} else if(type == 2) {
											short *arg;
											arg = (short *) malloc(arrayLength * sizeof(short ));
											for(int k=0; k < arrayLength; k++) {
												if(read(i, usArg, sizeof(short))  <= 0) {
													perror("Execute-read-5");
												}
												
												arg[k] = *((short *)(usArg));
												//cout << "value is: " << arg[k] << endl;
											}
											args[j] = (void *) arg;	
										} else if(type == 3) {
											int *arg;
											arg = (int *) malloc(arrayLength * sizeof(int ));
											for(int k=0; k < arrayLength; k++) {
												if(read(i, usArg, sizeof(int))  <= 0) {
													perror("Execute-read-5");
												}
												
												arg[k] = *((int *)(usArg));
												//cout << "value is: " << arg[k] << endl;
											}
											args[j] = (void *) arg;
											
										} else if(type == 4) {
											long *arg;
											arg = (long *) malloc(arrayLength * sizeof(long ));
											for(int k=0; k < arrayLength; k++) {
												if(read(i, usArg, sizeof(long))  <= 0) {
													perror("Execute-read-5");
												}
												
												arg[k] = *((long *)(usArg));
												//cout << "value is: " << arg[k] << endl;
											}
											args[j] = (void *) arg;										
										} else if(type == 5) {
											double *arg;
											arg = (double *) malloc(arrayLength * sizeof(double ));
											for(int k=0; k < arrayLength; k++) {
												if(read(i, usArg, sizeof(double))  <= 0) {
													perror("Execute-read-5");
												}
												
												arg[k] = *((double *)(usArg));
												//cout << "value is: " << arg[k] << endl;
											}
											args[j] = (void *) arg;
											
										} else if(type == 6) {
											float *arg;
											arg = (float *) malloc(arrayLength * sizeof(float ));
											for(int k=0; k < arrayLength; k++) {
												if(read(i, usArg, sizeof(float))  <= 0) {
													perror("Execute-read-5");
												}
												
												arg[k] = *((float *)(usArg));
												//cout << "value is: " << arg[k] << endl;
											}
											args[j] = (void *) arg;
											
										} else {
											error = 1;
											//cout << "unknown arg type!!" << endl;
										}
									}
								}
								
								//cout << "parse done" << endl;
								skeleton f = functions[name];
								//cout << "get from map" << endl;
								
								pid_t pid = fork();
								//cout << "fork" << endl;
								
								if (pid == 0) {
									//cout << "child proc" << endl;
									int result = f(argTypes, args);
									//cout << result << endl;
									//printf("ACTUAL return of f2 is: %s\n", (char *)args[0]);
									
									if(result == 0) {
										constructResultMsg(namebuf, argTypes, args, i);
										//cout << "done" << endl;
									} else {
										error = -4;
										//[length]EXECUTE_FAILURE reasonCode
										string msg;
										
										//[length]EXECUTE_FAILURE	
										string request("EXECUTE_FAILURE");
										int len = request.length();
										unsigned char byte[4];
										conToByte(len, byte);
										string hex_len(reinterpret_cast<char*>(byte), 4);
										
										msg.append(hex_len);
										msg.append(request);
										//reasonCode
										conToByte(error, byte);
										string hex_len2(reinterpret_cast<char*>(byte), 4);
										
										msg.append(hex_len2);
										
										int byteSend;
										if((byteSend = send(i, msg.c_str(), msg.length(), 0)) == -1) {
											perror("Execute send to client:");
										}	
									}
									exit(0);
								}
							
							
						} else if(msg.compare("TERMINATE") == 0) {
							if (i == binderSocket) {
								//cout << "verified" << endl;
								return 0;
							} else {
								//cout << "not from binder" << endl;
								error = 2;
							}
						}
					}
                } 
            } 
        } 
    }
}

//------------------------------------------------------------------------------------------------------
//										RPC TERMINATE
//------------------------------------------------------------------------------------------------------

int rpcTerminate() {
	string str_reply;
	string str_type("TERMINATE");
	unsigned char byte[4];
	conToByte(strlen("TERMINATE"), byte);
	string hex_len(reinterpret_cast<char*>(byte), 4);
	str_reply.append(hex_len);
    str_reply.append(str_type);
	send(binderSocket, str_reply.c_str(), str_reply.length(), 0);
}
