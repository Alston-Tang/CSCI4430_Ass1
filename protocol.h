#ifndef PROTOCOL_H_INCLUDED
#define PROTOCOL_H_INCLUDED

#include <stdint.h>

#define MSGINCOR -1
#define TOOMANYUSER -2
#define SMIPNPORT -3
#define SMNAME -4
#define NOSUCHUSER -5

#define MSGL 512
#define LMSGL 5120

#define MYMSG uint8_t
#define ERRCOD int8_t

#define LOGIN 0x01
#define LOGIN_OK 0x02
#define GET_LIST 0x03
#define GET_LIST_OK 0x04

uint32_t copy2msg(MYMSG* destMsg,uint32_t length,uint8_t content[],uint32_t msgPos);
uint32_t copy2msg(MYMSG* destMsg,uint16_t content,uint32_t msgPos,bool hasLength);
uint32_t copy2msg(MYMSG* destMsg,uint32_t content,uint32_t msgPos,bool hasLength);

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

class clientList
{
private:
    uint32_t userNum;
    userInf* start;
public:
    ERRCOD addUser(MYMSG* sourMsg, uint32_t ipAddr);
    ERRCOD delUser(char delName[]);
    ERRCOD getList(MYMSG* destMsg);

    clientList();
    ~clientList();
};

ERRCOD createLoginMsg(MYMSG* destMsg,char* userName, uint16_t portNum);
ERRCOD createLoginOkMsg(MYMSG* destMsg);
ERRCOD createGetListMsg(MYMSG* destMsg);
ERRCOD createGetListOkMsg(MYMSG* destMsg, clientList& ServerClientList);




#endif // PROTOCOL_H_INCLUDED
