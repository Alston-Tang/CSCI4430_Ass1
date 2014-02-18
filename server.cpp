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
#include "debug.h"

//shared variable
clientList clList;
pthread_rwlock_t clListLock;
//


void* connection(void* in)
{
    int32_t ipAddr=((struct conInf*)in)->conAddr->sin_addr.s_addr;
    int conSocket=*(((struct conInf*) in)->conSocket);
    uint32_t length=0;
    bool valid=false;
    userInf* thisUser=NULL;
    ERRCOD rtVal;
    MYMSG msg[LMSGL],sendMsg[LMSGL];
    msgReceiver msgRec(conSocket);
    #ifdef DEBUG
        char* ipName=inet_ntoa(((struct conInf*)in)->conAddr->sin_addr);
        printf("[%s] child thread created\n",ipName);
    #endif
    while(1)
    {
        if(msgRec.receiveMsg(msg,&length)==DISCONNECT)
        {
            #ifdef DEBUG
                printf("[%s] Connection closed\n",ipName);
                printf("[%s] Thread terminate\n",ipName);
            #endif
            close(conSocket);
            if (thisUser!=NULL)
            {
                #ifdef MUTEX
                    printf("clList:Writer lock\n");
                #endif
                pthread_rwlock_wrlock(&clListLock);
                clList.delUser(thisUser->name);
                pthread_rwlock_unlock(&clListLock);
                #ifdef MUTEX
                    printf("clList:Writer unlock\n");
                #endif
            }
            pthread_exit(0);
        }
        #ifdef DEBUG
            printf("[%s] Received messagnbe:\n",ipName);
            printMsg(ipName,msg,length);
        #endif
        switch (msg[0])
        {
            case LOGIN:
                #ifdef MUTEX
                    printf("clList:Writer lock\n");
                #endif
                pthread_rwlock_wrlock(&clListLock);
                rtVal=clList.addUser(msg,ipAddr,&thisUser);
                pthread_rwlock_unlock(&clListLock);
                #ifdef MUTEX
                    printf("clList:Writer unlock\n");
                #endif
                switch (rtVal)
                {
                    case 0:
                        valid=true;
                        length=createLoginOkMsg(sendMsg);
                        #ifdef DEBUG
                            printf("[%s] User information has been added into list\n",ipName);
                            printf("[%s] LoginOk Message created, content:\n",ipName);
                            printMsg(ipName,sendMsg,length);
                        #endif
                        write(conSocket,sendMsg,length);
                        #ifdef DEBUG
                            printf("[%s] Send LoginOkMes to client\n",ipName);
                        #endif
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
                    if(!valid)
                    {
                        #ifdef DEBUG
                            printf("[%s] An unauthorized client tries to get list,connection will be closed\n",ipName);
                        #endif
                        close(conSocket);
                        pthread_exit(0);
                    }
                #ifdef DEBUG
                    printf("[%s] Client asks for the client list\n",ipName);
                #endif
                #ifdef MUTEX
                    printf("clist:Reader lock\n");
                #endif
                pthread_rwlock_rdlock(&clListLock);
                length=clList.getList(sendMsg);
                pthread_rwlock_unlock(&clListLock);
                #ifdef MUTEX
                    printf("clist:Reader unlock\n");
                #endif
                #ifdef DEBUG
                    printf("[%s] Client list ok message is:\n",ipName);
                    printMsg(ipName,sendMsg,length);
                    printf("[%s] Send msg to client...\n",ipName);
                #endif
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

    if (pthread_rwlock_init(&clListLock,NULL)!=0) err("Can not initialize rwlock!\n",-1);

    listenSocket=socket(AF_INET,SOCK_STREAM,0);
    if(listenSocket==-1) err("socket()");
    #ifdef DEBUG
        printf("[main] Server socket create sucess,fd is %d\n",listenSocket);
    #endif

    memset(&addr,sizeof(struct sockaddr_in),0);
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_port=htons(SERVERPORT);

    rtValue=bind(listenSocket,(struct sockaddr*) &addr, sizeof(addr));
    if(rtValue==-1) err("bind()");
    #ifdef DEBUG
        printf("[main] Socket bind successful.\n");
        printf("[main] Port: %d\n",ntohs(addr.sin_port));
    #endif

    rtValue=listen(listenSocket,1024);
    if (rtValue==-1) err("listen()");
    #ifdef DEBUG
        printf("[main] Socket listen successful.\n");
    #endif

    while(1)
    {
        #ifdef DEBUG
            printf("[main] Enter main loop\n");
        #endif
        struct conInf* passToChld=new struct conInf;
        passToChld->conAddr=new struct sockaddr_in;
        passToChld->conSocket=new int;

        unsigned int addrLen=sizeof(sockaddr_in);
        #ifdef DEBUG
            printf("[main] Waiting for client...\n");
        #endif
        *(passToChld->conSocket)=accept(listenSocket,(sockaddr*)passToChld->conAddr,&addrLen);
        if(*(passToChld->conSocket)==-1) err("accept()");
        #ifdef DEBUG
            printf("[main] A new client connect, connecting socket fd is %d\n",*(passToChld->conSocket));
            printf("[main] Client ip is %s\n",inet_ntoa(passToChld->conAddr->sin_addr));
            printf("[main] Creating new thread...\n");
        #endif

        pthread_attr_t threadAttr;
        pthread_t noUse;
        pthread_attr_init(&threadAttr);
        pthread_attr_setdetachstate(&threadAttr,PTHREAD_CREATE_DETACHED);
        pthread_create(&noUse,&threadAttr,connection,(void*)passToChld);

        #ifdef DEBUG
            printf("[main] New thread is created\n");
        #endif

    }
    #ifdef DEBUG
        printf("[main] Exit main loop, about to terante\n");
    #endif
    return 0;
}

