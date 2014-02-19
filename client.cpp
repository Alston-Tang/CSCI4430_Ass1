#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include "protocol.h"
#include "ui.h"
#include "client.h"

#ifdef DEBUG
#include "debug.h"
#endif



//shared variable
int serverSocket;
char scrName[512];
recMsgBuf buf;
clientConnectedList cConList;
sendMsgBuf sdBuf;

pthread_rwlock_t bufLock,cConListLock;
pthread_mutex_t sdBufLock,printMutex;
pthread_cond_t bufNotEmpty,thdTerminate;
//

void closeHandle(int conSocket)
{
    close(conSocket);
    pthread_exit(0);
}

void closeHandle(int conSocket,char* scrName)
{
    close(conSocket);
    pthread_rwlock_wrlock(&cConListLock);
    cConList.disconnectWith(scrName);
    pthread_rwlock_unlock(&cConListLock);
    pthread_exit(0);
}

void* threadTypeA(void* in)
{
    int conSocket=*(((conInfwRec*)in)->conSocket);
    char ipName[20];
    #ifdef DEBUG
    int portName;
    portName=(htons(((conInf*)in)->conAddr->sin_port));
    #endif
    MYMSG msg[LMSGL];
    uint32_t length;

    strcpy(ipName,inet_ntoa(((conInf*)in)->conAddr->sin_addr));

    #ifdef DEBUG
        printHeader(ipName,portName);
        printf("thread type a is created\n");
    #endif

    pthread_mutex_lock(&sdBufLock);
    while(sdBuf.cutMsg(msg,&length)!=EMPTY)
    {
        #ifdef DEBUG
            printHeader(ipName,portName);
            printf("new message send to client is:\n");
            printMsg(ipName,portName,msg,length);
        #endif
        write(conSocket,msg,length);
    }
    pthread_mutex_unlock(&sdBufLock);

    while(1)
    {
        pthread_mutex_lock(&sdBufLock);
        while(sdBuf.cutMsg(msg,&length)!=EMPTY)
        {
            #ifdef DEBUG
                printHeader(ipName,portName);
                printf("new message send to client is:\n");
                printMsg(ipName,portName,msg,length);
            #endif
            write(conSocket,msg,length);
        }
        if(sdBuf.isTerminate())
        {
            pthread_mutex_unlock(&sdBufLock);
            pthread_cond_signal(&thdTerminate);
            #ifdef DEBUG
                printHeader(ipName,portName);
                printf("thread about terminate\n");
            #endif
            return NULL;
        }
        else pthread_cond_wait(&bufNotEmpty,&sdBufLock);
        pthread_mutex_unlock(&sdBufLock);
    }
    #ifdef DEBUG
        printHeader(ipName,portName);
        printf("thread about terminate\n");
    #endif
    return NULL;
}

void* threadTypeB(void* in)
{
    char ipName[20];
    #ifdef DEBUG
    int portName;
    portName=(htons(((conInfwRec*)in)->conAddr->sin_port));
    #endif
    strcpy(ipName,inet_ntoa(((conInfwRec*)in)->conAddr->sin_addr));
    #ifdef DEBUG
        printHeader(ipName,portName);
        printf("thread type B is created\n");
    #endif

    int conSocket=*(((conInfwRec*)in)->conSocket);

    msgReceiver msgRecSer(serverSocket);
    msgReceiver* msgRecCli=((conInfwRec*)in)->rec;

    MYMSG msg[LMSGL];
    char screenName[MSGL];
    bool validClient=false;

    screenName[0]=0;

    while(1)
    {
        uint32_t length;
        #ifdef DEBUG
            printHeader(ipName,portName);
            printf("thread type b: tries to get type b message\n");
        #endif
        if(msgRecCli->receiveTypeBMsg(msg,&length)==DISCONNECT)
        {
            #ifdef DEBUG
                printHeader(ipName,portName);
                printf("connection close\n");
            #endif
            if(screenName[0]==0) closeHandle(conSocket);
            else closeHandle(conSocket,scrName);

        }
        switch (msg[0])
        {
        case HELLO:
        {
            #ifdef DEBUG
                printHeader(ipName,portName);
                printf("receive a HELLO MSG, content is:\n");
                printMsg(ipName,portName,msg,length);
            #endif

            uint32_t length;
            int num;
            userInf uInf[MAXUSER];

            fetchFmsg(msg,&length,1);

            memcpy(screenName,&msg[5],length);
            screenName[length]=0;

            MYMSG sMsg[MSGL];
            length=createGetListMsg(sMsg);
            #ifdef DEBUG
                printHeader(ipName,portName);
                printf("get list message:\n");
                printMsg(ipName,portName,sMsg,length);
                printHeader(ipName,portName);
                printf("send to server\n");
            #endif

            write(serverSocket,sMsg,length);
            msgRecSer.receiveMsg(msg,&length);
            #ifdef DEBUG
                printHeader(ipName,portName);
                printf("receive a message from server, content is:\n");
                printMsg(ipName,portName,msg,length);
            #endif

            clientListParse(msg,uInf,&num);
            bool find=false;
            for(int i=0; i<num; i++)
            {
                if (strcmp(screenName,uInf[i].name)==0)
                {
                    find=true;
                    break;
                }
            }
            if (find)
            {
                #ifdef DEBUG
                    printHeader(ipName,portName);
                    printf("tries to send hello ok message\n");
                #endif
                pthread_rwlock_wrlock(&cConListLock);
                if (cConList.connectWith(screenName,conSocket,&msgRecCli)==TOOMANYCONNECTION)
                {
                    printf("Too many connections between clients\n");
                    delete msgRecCli;
                    close(conSocket);
                    err("Type B: makeConnection()\n",-9);
                }
                pthread_rwlock_unlock(&cConListLock);

                validClient=true;
                length=createHelloOkMsg(sMsg);
                #ifdef DEBUG
                    printHeader(ipName,portName);
                    printf("sending hello ok message\n");
                    printMsg(ipName,portName,sMsg,length);
                #endif
                write(conSocket,sMsg,length);
            }
            else
            {
                #ifdef DEBUG
                    printHeader(ipName,portName);
                    printf("unknown client\n");
                #endif
                length=createErrMsg(sMsg,0x04);
                #ifdef DEBUG
                    printHeader(ipName,portName);
                    printf("error msg is sent\n");
                    printMsg(ipName,portName,sMsg,length);
                #endif
                write(conSocket,sMsg,length);
            }
            break;
        }
        case MSG:
        {
            if (!validClient)
            {
                close(conSocket);
                err("Type B: validClient==false",NOSUCHUSER);
            }
            buf.addMsg(msg,screenName);
        }
        }
    }
    return NULL;
}

void* listenThread(void* in)
{
    #ifdef DEBUG
        printf("[listen] Listen thread created\n");
    #endif

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
    #ifdef DEBUG
        printf("[listen] Listening..\n");
    #endif
    while(1)
    {
        struct conInfwRec* passToChld=new struct conInfwRec;

        passToChld->conAddr=new struct sockaddr_in;
        passToChld->conSocket=new int;

        unsigned int addrLen=sizeof(sockaddr_in);

        *(passToChld->conSocket)=accept(listenSocket,(sockaddr*)passToChld->conAddr,&addrLen);
        if(*(passToChld->conSocket)==-1) err("accept() in listen thread");

        #ifdef DEBUG
            printf("[listen] A client is connected to listen thread\n");
            printf("[listen] IP: %s\n",inet_ntoa(passToChld->conAddr->sin_addr));
            printf("[listen] port %d\n",ntohs(passToChld->conAddr->sin_port));
        #endif

        passToChld->rec=new  msgReceiver(*(passToChld->conSocket));

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
    char ipAddr[20];
    char thrName[10]="main";


    pthread_rwlock_init(&bufLock,NULL);
    pthread_rwlock_init(&cConListLock,NULL);
    pthread_mutex_init(&sdBufLock,NULL);
    pthread_mutex_init(&printMutex,NULL);
    pthread_cond_init(&bufNotEmpty,NULL);
    pthread_cond_init(&thdTerminate,NULL);

    #ifdef LOCAL_SERVER
    strcpy(ipAddr,"127.0.0.1");
    strcpy(scrName,"tang");
    #elif LOCAL_SERVER2
    strcpy(ipAddr,"127.0.0.1");
    strcpy(scrName,"kkk");
    #else
    printf("IP Address:");
    scanf("%s",ipAddr);
    printf("Screen Name:");
    scanf("%s",scrName);
    #endif

    struct in_addr ip;
    if(inet_aton(ipAddr,&ip)==0) err("inet_aton()",0);

    serverSocket=socket(AF_INET,SOCK_STREAM,0);
    if(serverSocket==-1) err("socket() in main");

    struct sockaddr_in serverAddr;
    memset(&serverAddr,sizeof(struct sockaddr_in),0);
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_addr.s_addr=ip.s_addr;
    serverAddr.sin_port=htons(SERVERPORT);
    uint32_t addrLen=sizeof(struct sockaddr_in);

    if(connect(serverSocket,(struct sockaddr*)&serverAddr,addrLen)==-1) err("connect() in main");
    #ifdef DEBUG
        printf("Connect to server successful.\n");
        printf("Server ip: %s\n",inet_ntoa(serverAddr.sin_addr));
        printf("Port: %d\n",ntohs(serverAddr.sin_port));
    #endif

    srand(time(NULL));
    uint16_t portNum=rand()%16410+49125;

    #ifdef DEBUG
        printf("The random local client port is %d\n",portNum);
        printf("Try to login...\n");
    #endif

    MYMSG msg[MSGL];
    msgReceiver msgRec(serverSocket);

    int length=createLoginMsg(msg,scrName,portNum);
    #ifdef DEBUG
        printf("Login Msg is:\n");
        printMsg(thrName,msg,length);
        printf("Write to server...\n");
    #endif
    write(serverSocket,msg,length);
    #ifdef DEBUG
        printf("Write complete\n");
        printf("Tries to get message from server\n");
    #endif

    msgRec.receiveMsg(msg);
    if (msg[0]!=LOGIN_OK) err("Login failed",CANNOTLOGIN);
    else printf("Login successed\n");

    #ifdef DEBUG
        printf("[main] Creating listen thread\n");
    #endif

    pthread_attr_t threadAttr;
    pthread_t noUse;
    pthread_attr_init(&threadAttr);
    pthread_attr_setdetachstate(&threadAttr,PTHREAD_CREATE_DETACHED);
    pthread_create(&noUse,&threadAttr,listenThread,&portNum);

    #ifdef DEBUG
        printf("[main] Listen thread created\n");
        printf("[main] Getting user list\n");
        printf("[main] Start UI\n");
    #endif
    clientUI ui(thrName,&msgRec);
    ui.run();
}
