#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <netinet/in.h>
#include <string.h>
#include "protocol.h"

struct conInf
{
    int* conSocket;
    struct sockaddr_in* conAddr;
};

recMsgBuf buf;

void* threadTypeA(void* in)
{
    return NULL;
}

void* threadTypeB(void* in)
{
    int conSocket=*(((conInf*)in)->conSocket);

    msgReceiver msgRec(conSocket);
    MYMSG msg[LMSGL];

    while(1)
    {
        msgRec.receiveMsg(msg);
        switch (msg[0])
        {
        case HELLO:
            char screenName[512];
            uint32_t length=fetchFmsg(msg,&length,1);
            memcpy(screenName,&msg[5],length);
            MYMSG sMsg[MSGL];
            length=createGetListMsg(sMsg);
            write(conSocket,sMsg,length);
            msgRec.receiveMsg(msg);

            break;
        }
    }
    return NULL;
}

void* listenThread(void* in)
{
    uint16_t portNum=*((uint16_t*)in);
    int listenSocket;
    struct sockaddr_in addr;

    listenSocket=socket(AF_INET,SOCK_STREAM,0);
    if(listenSocket==-1) err("socket() in listen thread");

    memset(&addr,sizeof(struct sockaddr_in),0);
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_port=htons(portNum);
    if(bind(listenSocket,(struct sockaddr*)&addr,sizeof(addr))==-1) err("bind() in listen thread");

    if(listen(listenSocket,1024)==-1) err("listen() in listen thread");

    while(1)
    {
        struct conInf* passToChld=new struct conInf;
        passToChld->conAddr=new struct sockaddr_in;
        passToChld->conSocket=new int;

        unsigned int addrLen=sizeof(sockaddr_in);

        *(passToChld->conSocket)=accept(listenSocket,(sockaddr*)passToChld->conAddr,&addrLen);
        if(*(passToChld->conSocket)==-1) err("accept() in listen thread");

        pthread_attr_t threadAttr;
        pthread_t noUse;
        pthread_attr_init(&threadAttr);
        pthread_attr_setdetachstate(&threadAttr,PTHREAD_CREATE_DETACHED);
        pthread_create(&noUse,&threadAttr,threadTypeB,passToChld);
    }
    return NULL;
}

int main(void)
{
    srand(time(NULL));
    uint16_t portNum=rand()%16410+49125;

    pthread_attr_t threadAttr;
    pthread_t noUse;
    pthread_attr_init(&threadAttr);
    pthread_attr_setdetachstate(&threadAttr,PTHREAD_CREATE_DETACHED);
    pthread_create(&noUse,&threadAttr,listenThread,&portNum);

    sleep(5);
}
