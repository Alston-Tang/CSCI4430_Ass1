#ifndef PROTOCOL_H_INCLUDED
#define PROTOCOL_H_INCLUDED

#include "stdint.h"
#include <netinet/in.h>
#include <pthread.h>

#define MSGINCOR -1
#define TOOMANYUSER -2
#define SMIPNPORT -3
#define SMNAME -4
#define NOSUCHUSER -5
#define BUFTOOSM -6
#define DISCONNECT -7
#define CANNOTLOGIN -8
#define TOOMANYCONNECTION -9
#define EMPTY -10
#define UNKNOWN -11
#define NOTFIND -12

#define MSGL 512
#define LMSGL 5120

#define MSGTYPE int8_t
#define TYPEA 1
#define TYPEB 0

#define MYMSG uint8_t
#define ERRCOD int8_t
#define STA int8_t

#define LOGIN 0x01
#define LOGIN_OK 0x02
#define GET_LIST 0x03
#define GET_LIST_OK 0x04
#define HELLO 0x10
#define HELLO_OK 0x20
#define MSG 0x30
#define ERROR 0xff

#define DIS 0
#define CONNECTING 1
#define CONNECTED 2

#define BUFMAX 100

#define MAXUSER 10

void err(char const place[]);
void err(char const place[],int code);
uint32_t copy2msg(MYMSG* destMsg,uint32_t length,uint8_t content[],uint32_t msgPos);
uint32_t copy2msg(MYMSG* destMsg,uint16_t content,uint32_t msgPos,bool hasLength);
uint32_t copy2msg(MYMSG* destMsg,uint32_t content,uint32_t msgPos,bool hasLength);
uint32_t fetchFmsg(MYMSG* sourMsg,uint32_t* result,uint32_t msgPos);
uint32_t fetchFmsg(MYMSG* sourMsg,uint16_t* result,uint32_t msgPos);



class restMsgBuf
{
public:
    restMsgBuf* next;
    MYMSG content[MSGL];
    int msgLength;

    restMsgBuf();
    restMsgBuf(MYMSG* sourMsg, uint32_t length);
    void insert(MYMSG* sourMsg,uint32_t length);
    ERRCOD get(MYMSG* destMsg,uint32_t* length);
    ERRCOD getTypeA(MYMSG* destMsg,uint32_t* length);
    ERRCOD getTypeB(MYMSG* destMsg,uint32_t* length);

    void print();
};

class msgReceiver
{
private:
    uint8_t inBuf[LMSGL];
    uint32_t inLength;
    int conSocket;
    restMsgBuf* rStart;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
public:
    msgReceiver(int conSocket);
    ~msgReceiver();
    ERRCOD receiveTypeAMsg(MYMSG* destMsg, uint32_t* msgLength);
    ERRCOD receiveTypeBMsg(MYMSG* destMsg, uint32_t* msgLength);
    ERRCOD receiveMsg(MYMSG* destMsg,uint32_t* msgLength);
    ERRCOD receiveMsg(MYMSG* destMsg);

};


class userInf
{
public:
    char name[512];
    uint32_t ipAddr;
    uint16_t portNum;
    uint32_t nameLength;
    userInf* next;

    userInf();
};


class recMsg
{
public:
    char msgContent[512];
    char fromName[512];
    recMsg* next;

    recMsg();
};

class clientList
{
private:
    uint32_t userNum;
    userInf* start;
public:
    ERRCOD addUser(MYMSG* sourMsg, uint32_t ipAddr,userInf** thisUser);
    ERRCOD delUser(char delName[]);
    int getList(MYMSG* destMsg);

    clientList();
    ~clientList();
};

class recMsgBuf
{
private:
    uint32_t unreadNum;
    recMsg* start;
public:
    recMsgBuf();
    ~recMsgBuf();
    ERRCOD addMsg(MYMSG* sourMsg,char name[]);
    int getAllMsg(recMsg* destArr);
    ERRCOD delAllMsg();
    int getUnreadNum();
};

class clientConnected
{
public:
    char name[512];
    int socketFd;
    bool connected;
    msgReceiver* rec;
    clientConnected* next;

    clientConnected();

};

class clientConnectedList
{
private:
    clientConnected* start;
public:
    clientConnectedList();
    ~clientConnectedList();
    ERRCOD makeConnection(char* screenName);
    ERRCOD connectWith(char* screenName,int socket,msgReceiver** rec);
    ERRCOD disconnectWith(char* screenName);
    int connectionStatus(char* screenName);
    int connectionStatus(char* screenName,int* destSocket, msgReceiver** rec);
};

class sendMsg
{
public:
    char content[MSGL];
    uint32_t msgLength;
    sendMsg* next;
    sendMsg();
};

class sendMsgBuf
{
public:
    sendMsg* start;
    bool terminate;
    ERRCOD insertMsg(char* soutStr);
    ERRCOD cutMsg(MYMSG* destMsg,uint32_t* length);
    void thdStart();
    void thdFinish();
    bool isEmpty();
    bool isTerminate();
    sendMsgBuf();
    ~sendMsgBuf();
};

struct conInf
{
    int* conSocket;
    struct sockaddr_in* conAddr;
};

struct conInfwRec
{
    int* conSocket;
    struct sockaddr_in* conAddr;
    msgReceiver* rec;
};

ERRCOD clientListParse(MYMSG* sourMsg, userInf* destStr, int* num);

int createLoginMsg(MYMSG* destMsg,char* userName, uint16_t portNum);
int createLoginOkMsg(MYMSG* destMsg);
int createGetListMsg(MYMSG* destMsg);
int createErrMsg(MYMSG* destMsg,uint8_t errCode);
int createHelloMsg(MYMSG* destMsg,char* sName);
int createHelloOkMsg(MYMSG* destMsg);
int createMsg(MYMSG* destMsg, char* msgContent);

int getArgNum(uint8_t);
MSGTYPE getMsgType(MYMSG* msg);


#endif // PROTOCOL_H_INCLUDED
