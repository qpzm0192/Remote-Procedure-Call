#include <stdio.h>      
#include <ifaddrs.h>
#include <netinet/in.h> 
#include <string.h> 
#include <arpa/inet.h>
#include <iostream>
#include <sys/socket.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/select.h>
#include <sstream>
#include <stdlib.h>
#include <limits.h>
#include <vector>
#include <map>
#include <deque>

using namespace std;

#define ARG_CHAR    1
#define ARG_SHORT   2
#define ARG_INT     3
#define ARG_LONG    4
#define ARG_DOUBLE  5
#define ARG_FLOAT   6

#define ARG_INPUT   31
#define ARG_OUTPUT  30

#define REGISTER "REGISTER"
#define REGISTER_SUCCESS "REGISTER_SUCCESS"
#define REGISTER_FAILURE "REGISTER_FAILURE"
#define LOC_REQUEST "LOC_REQUEST"
#define LOC_SUCCESS "LOC_SUCCESS"
#define LOC_FAILURE "LOC_FAILURE"
#define EXECUTE "EXECUTE"
#define EXECUTE_SUCCESS "EXECUTE_SUCCESS"
#define EXECUTE_FAILURE "EXECUTE_FAILURE"
#define TERMINATE "TERMINATE"

// warning code
#define NORMAL "00"
#define OVERWRITE "01"
#define DUP_REGISTRATION "02"

// error code
#define NOT_EXIST "-2"




void rm_array_length(char * arg_type){
    //unsigned char *arg_type_us = (unsigned char *) arg_type;
    //unsigned char newb[4];
    //newb[0] = arg_type_us[0]; //0-7
    //newb[1] = arg_type_us[1]; //7-15  
    ((unsigned char *)arg_type)[2] = (0 >> 8) & 0xFF;
    ((unsigned char *)arg_type)[3] = 1 & 0xFF;
}

int find_server(deque<pair <string, int> > RR_Queue, string sidwPort){
    int result = -1;
    for(int i = 0; i < RR_Queue.size(); i++){
        if(RR_Queue[i].first.compare(sidwPort) == 0)
            result = RR_Queue[i].second;
    }
    return result;
}

void conToByte(int i, unsigned char* bytes) {
    bytes[0] = (i >> 24);   //high
    bytes[1] = (i >> 16);   //  
    bytes[2] = (i >> 8);    //
    bytes[3] = i;           //low
}

/*void conToByte(int i, char* bytes) {
    unsigned char * new_bytes = (unsigned char *) bytes;
    new_bytes[0] = (i >> 24);   //high
    new_bytes[1] = (i >> 16);   //  
    new_bytes[2] = (i >> 8);    //
    new_bytes[3] = i;           //low
    bytes = (char*) new_bytes;
}*/

int conToInt(char * bytes) {
    unsigned char * new_bytes = (unsigned char *) bytes;
    int Int32 = 0;

    Int32 = (Int32 << 8) + new_bytes[0];
    Int32 = (Int32 << 8) + new_bytes[1];
    Int32 = (Int32 << 8) + new_bytes[2];
    Int32 = (Int32 << 8) + new_bytes[3];
    return Int32;
}

string append_length(string msg){
    string result;
    unsigned char byte[4];

    conToByte(msg.length(), byte);
    string hex_len(reinterpret_cast<char*>(byte), 4);
    
    result.append(hex_len);
    result.append(msg);
    cout << "append_length: " << msg << endl;
    return msg;
}


int main () {

    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number

    //int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char buf[USHRT_MAX];    // buffer for client data
    int nbytes;

    
    // get host name, get machine name
    char hostname[USHRT_MAX];
    //hostname[1023] = "0/";
    gethostname(hostname, USHRT_MAX-1);
    //cout << hostname << endl;
    printf("BINDER_ADDRESS %s\n", hostname); 



    // create socket
    struct hostent *hp;
    struct sockaddr_in mysockaddr;
    int mysocket, accept_socket;
    hp = gethostbyname(hostname);

    mysockaddr.sin_family= hp->h_addrtype;

    // initial portnum as 0
    unsigned short portnum = 0;
    mysockaddr.sin_port= htons(portnum);               
    mysocket= socket(AF_INET, SOCK_STREAM, 0);

    // bind
    bind(mysocket,(struct sockaddr *)&mysockaddr,sizeof(struct sockaddr_in));

    // listen
    listen(mysocket, 50); 
    socklen_t sockaddrlen = sizeof(struct sockaddr_in);
    getsockname(mysocket, (struct sockaddr *)&mysockaddr, &sockaddrlen);
    // print port num
    cout << "BINDER_PORT " << mysockaddr.sin_port << endl;



    // my registration data structure
    char buf_type[16];
    char buf_type_length[4];
   
    char buf_sid[65535];
    char buf_sid_length[4];
   
    char buf_port[65535];
    char buf_port_length[4];
   
    char buf_name[65535];
    char buf_name_length[4];
   
    char buf_arg_type[65535];
    char buf_arg_type_length[4];
    // procedure signature:
    //      name, argTypes
    // location:
    //      server_indentifier, port
    
    // mymap:
    // <signature, location> where signature is
    // name, num, type1, type2, type3...  ***without saying of array length**
    // my pair:
    // <server id, port>
    // vector of maps
    deque<pair <string, int> > RR_Queue;
    vector< map<string,pair<string, string> > >VoMap;
    
    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);
	int fd_count = 0;
	bool cangoin = true;
	bool firstTime = true;
    // add the listener to the master set
    FD_SET(mysocket, &master);

    // keep track of the biggest file descriptor
    fdmax = mysocket; // so far, it's this one

    // main loop
    while(true) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            //exit(4);
        }

        // run through the existing connections looking for data to read
        for(int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == mysocket && cangoin) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(mysocket, (struct sockaddr *)&mysockaddr, &sockaddrlen);
                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
						
                        //printf("selectserver: new connection from sth on "
                        //    "socket %d\n", newfd);
                    }
                }
                else {
                    // handle data from a client
					if(fd_count == 0 && !firstTime)
						return 0;
                    else if((nbytes = read(i, buf_type_length,4)) <= 0){
                        if (nbytes == 0) {
                            // connection closed
                            //printf("select server: socket %d hung up\n", i);
                        } else {
                            //perror("recv");
                            //cout << "lol" << endl;
                        }
                        //cout << "================================================" << endl;
						fd_count--;
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    }
                    else{
                        ////cout << "length is " << conToInt(buf_type_length) << endl;
                        read(i, buf_type, conToInt(buf_type_length));
                        buf_type[conToInt(buf_type_length)] = 0;
    
                        // we got some data from a client
                        string buf_str(buf_type);
                        ////cout << "socket is " << i <<endl;
    
                        if(buf_str.compare(REGISTER)==0){
                            // receive
                            //cout << "got REGISTER" << endl;
                            
                            read(i, buf_sid_length,4);
                            read(i, buf_sid, conToInt(buf_sid_length));
                            buf_sid[conToInt(buf_sid_length)] = 0;
                            //cout << "got sid: " << buf_sid << endl;
    
                            read(i, buf_port_length,4);
                            read(i, buf_port, conToInt(buf_port_length));
                            buf_port[conToInt(buf_port_length)] = 0;
                            //cout << "got port: " << buf_port << endl;
    
                            read(i, buf_name_length,4);
                            read(i, buf_name, conToInt(buf_name_length));
                            buf_name[conToInt(buf_name_length)] = 0;
                            //cout << "got name: " << buf_name << endl;
    
                            read(i, buf_arg_type_length,4);
                            string str_name(buf_name);
                            for(int j = 0; j < conToInt(buf_arg_type_length); j++){
                                char buf_arg_type[4];
                                read(i, buf_arg_type, 4);

                                rm_array_length(buf_arg_type);
                                string str_arg_type(buf_arg_type);
                                str_name.append(str_arg_type);

                                int myint = conToInt(buf_arg_type);
                                ////cout << "got arg type " << j << " " << myint << endl;
                            }
    
    
    
                            // insert
                            pair<string, string> temp_pair(buf_sid, buf_port);
                            string str_sid(buf_sid);
                            string str_port(buf_port);
                            string str_warning(NORMAL);
                            int index = find_server(RR_Queue, str_sid.append(str_port));
                            if(index == -1){
                                //cout << "first time found server" << endl;
                                map<string,pair<string, string> > thisMap;
                                thisMap[str_name] = temp_pair;
                                int size = VoMap.size();
                                VoMap.push_back(thisMap);
                                pair <string, int> mypair(str_sid, size);
                                RR_Queue.push_back(mypair);
								firstTime = false;
								fd_count++;
                            }
                            else{
                                //cout << "insert into exist server map" << endl;
                                if (VoMap[index][str_name].first.compare(temp_pair.first) == 0 &&
                                    VoMap[index][str_name].second.compare(temp_pair.second) == 0){
                                    str_warning = OVERWRITE;
                                }
                                VoMap[index][str_name] = temp_pair;
                            }

                            string str_reply(REGISTER_SUCCESS);
                            str_reply.append(str_warning);
    
                            
    
                            // display current map
                            //cout << "****************MyMap*******************" << endl;
                            for(int j = 0; j < VoMap.size(); j++){
                                //cout << "    server " << j << endl;
                                map<string,pair<string, string> > mymap = VoMap[j];
                                for (map<string,pair<string, string> >::iterator it = mymap.begin(); it != mymap.end(); ++it) {
                                    //cout << it->first << ": " << it->second.first  << ", " <<  it->second.second << endl;
                                }
                            }
                            //cout << "****************************************" << endl;

                            // read null terminator
                            read(i, buf_name, 1024);
    
                            // send reply
                            send(i, str_reply.c_str(), str_reply.length(), 0);
                        }
                        else if(buf_str.compare(LOC_REQUEST)==0){
                            //cout << "got LOC_REQUEST" << endl;

                            read(i, buf_name_length,4);
                            read(i, buf_name, conToInt(buf_name_length));
                            buf_name[conToInt(buf_name_length)] = 0;
                            //cout << "got func name: " << buf_name << endl;
    
                            read(i, buf_arg_type_length,4);
                            string str_name(buf_name);
                            for(int j = 0; j < conToInt(buf_arg_type_length); j++){
                                char buf_arg_type[4];
                                read(i, buf_arg_type, 4);

                                rm_array_length(buf_arg_type);
                                string str_arg_type(buf_arg_type);
                                str_name.append(str_arg_type);

                                int myint = conToInt(buf_arg_type);
                            }

                            // fetch
                            string sid;
                            string port;
                            for(int j = 0; j < RR_Queue.size(); j++){
                                pair <string, int> mypair = RR_Queue.front();
                                int index = mypair.second;
                                RR_Queue.pop_front();
                                RR_Queue.push_back(mypair);
                                map<string,pair<string, string> >::iterator it = VoMap[index].find(str_name);
                                if (it != VoMap[index].end()){    // if found
                                    sid = VoMap[index][str_name].first;
                                    port = VoMap[index][str_name].second;
                                    break;
                                }
                            }

                            // send reply
                            string str_reply;
                            if(!sid.length() && !port.length()){
                                // set reply type
                                string str_type(LOC_FAILURE);

                                unsigned char byte[4];
                                conToByte(strlen(LOC_FAILURE), byte);
                                string hex_len(reinterpret_cast<char*>(byte), 4);

                                string str_err(NOT_EXIST);
                                
                                str_reply.append(hex_len);
                                str_reply.append(str_type);
                                str_reply.append(str_err);
                            }
                            else{
                                // set reply type
                                string str_type(LOC_SUCCESS);

                                unsigned char byte[4];
                                conToByte(strlen(LOC_SUCCESS), byte);
                                string hex_len(reinterpret_cast<char*>(byte), 4);
                                
                                str_reply.append(hex_len);
                                str_reply.append(str_type);

                                // sid
                                unsigned char byte_sid[4];
                                conToByte(sid.length(), byte_sid);
                                string hex_len_sid(reinterpret_cast<char*>(byte_sid), 4);
                                str_reply.append(hex_len_sid);
                                str_reply.append(sid);

                                // port
                                unsigned char byte_port[4];
                                conToByte(port.length(), byte_port);
                                string hex_len_post(reinterpret_cast<char*>(byte_port), 4);
                                str_reply.append(hex_len_post);
                                str_reply.append(port);
                            }
                            // read null terminator
                            read(i, buf_name, 1024);
                            send(i, str_reply.c_str(), str_reply.length(), 0);

                        }
                        else if(buf_str.compare(TERMINATE)==0){
                            //cout << "got TERMINATE" << endl;

                            // set reply type
                            string str_reply;
                            string str_type(TERMINATE);
                            unsigned char byte[4];
                            conToByte(strlen(TERMINATE), byte);
                            string hex_len(reinterpret_cast<char*>(byte), 4);
                            str_reply.append(hex_len);
                            str_reply.append(str_type);
							cangoin = false;
                            for( int fd = 0; fd < fdmax; fd++ ){
                                if ( FD_ISSET(fd, &master) && fd != mysocket ){
                                    if (send(fd, str_reply.c_str(), str_reply.length(), 0) < 0){
                                        //cout << "send terminate failed" << endl;
                                    }
                                }
                            }
							

                        }
                        else{
                            //cout << "unknown type, got: <" << buf_str << ">" << endl;
                        }
                        
                        // clean read buffer
                        //if (recv(i, buf_type, sizeof buf_type, 0) < 0)
                        //    //cout << "recv failed" << endl;
                        ////cout << "recv done" << endl;
                        
                        ////cout << "send done" << endl;
    
                        // clean buffer
                        memset(buf_type, 0, sizeof buf_type);
                        memset(buf_type_length, 0, sizeof buf_type_length);
    
                        memset(buf_sid, 0, sizeof buf_sid);
                        memset(buf_sid_length, 0, sizeof buf_sid_length);
    
                        memset(buf_port, 0, sizeof buf_port);
                        memset(buf_port_length, 0, sizeof buf_port_length);
    
                        memset(buf_name, 0, sizeof buf_name);
                        memset(buf_name_length, 0, sizeof buf_name_length);
    
                        memset(buf_arg_type, 0, sizeof buf_arg_type);
                        memset(buf_arg_type_length, 0, sizeof buf_arg_type_length);
    
    
                        //cout << endl;
                        //sleep(1);
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    }




}