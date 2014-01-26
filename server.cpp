#define SERVERPORT 12339

#include <stdio.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "protocol.h"

struct conInf
{
    int* conSocket;
    struct sockaddr_in* conAddr;
};

clientList clList;

void* connection(void* in)
{
    int32_t ipAddr=(struct conInf*) in->conAddr->sin_addr.s_addr;
    int conSocket=*((struct conInf*) in->conSocket)
    int length=0;
    ERRCOD rtVal;
    MYMSG msg[LMSGL],sendMsg[LMSGL];
    msgReceiver msgRec(*(inf->conSocket));
    while(1)
    {
        msgRec.receiveMsg(msg);
        switch (msg[0])
        {
            case LOGIN:
                rtVal=clList.addUser(msg,inf->conAddr->sin_addr.s_addr);
                switch (rtVal)
                {
                    case 0:
                        length=createLoginOkMsg(sendMsg);
                        write(conSocket,sendMsg,length);
                        break;
                    case SMNAME:
                        length=createErrMsg(sendMsg,0x01);
                        write(conSocket,sendMsg,length);
                        break;
                    case SMIPNPORT:
                        length=createErrMsg(sendMsg,0x02);
                        write(conSocket,sendMsg,length);
                        break;
                    case TOOMANYUSER:
                        length=createErrMsg(sendMsg,0x03);
                        write(conSocket,sendMsg,length);
                        break;
                }
                break;
            case GET_LIST:
                length=clList.getList(sendMsg);
                write(conSocket,sendMsg,length);
                break;
            default:
                err("connection(): receive an unknown msg",MSGINCOR);
        }
    }
    return NULL;
}

int main(void)
{

    int listenSocket,rtValue;
    struct sockaddr_in addr;

    listenSocket=socket(AF_INET,SOCK_STREAM,0);
    if(listenSocket==-1) err("socket()");

    memset(&addr,sizeof(struct sockaddr_in),0);
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_port=htons(SERVERPORT);

    rtValue=bind(listenSocket,(struct sockaddr*) &addr, sizeof(addr));
    if(rtValue==-1) err("bind()");

    rtValue=listen(listenSocket,1024);
    if (rtValue==-1) err("listen()");

    while(1)
    {
        struct conInf* passToChld=new struct conInf;
        passToChld->conAddr=new struct sockaddr_in;
        passToChld->conSocket=new int;

        unsigned int addrLen=sizeof(sockaddr_in);

        *(passToChld->conSocket)=accept(listenSocket,(sockaddr*)passToChld->conAddr,&addrLen);
        if(*(passToChld->conSocket)==-1) err("accept()");

        pthread_attr_t threadAttr;
        pthread_t noUse;
        pthread_attr_init(&threadAttr);
        pthread_attr_setdetachstate(&threadAttr,PTHREAD_CREATE_DETACHED);
        pthread_create(&noUse,&threadAttr,connection,(void*)passToChld);
    }
    return 0;
}
