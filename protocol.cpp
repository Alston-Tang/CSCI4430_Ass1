#include "protocol.h"
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

void err(char const place[])
{
    perror(place);
    exit(-1);
}

void err(char const place[],int code)
{
    printf("fatal error happens at %s, error code is %d",place,code);
    exit(-1);
}

uint32_t copy2msg(MYMSG* destMsg,uint32_t length,uint8_t content[],uint32_t msgPos)
{
    uint32_t nlength=htonl(length);
    memcpy(&destMsg[msgPos],&nlength,4);
    msgPos+=4;
    memcpy(&destMsg[msgPos],content,length);
    msgPos+=length;
    return msgPos;
}
uint32_t copy2msg(MYMSG* destMsg,uint16_t content,uint32_t msgPos,bool hasLength)
{
    if (hasLength)
    {
        uint32_t nlength=htonl(2);
        memcpy(&destMsg[msgPos],&nlength,4);
        msgPos+=4;
    }
    uint16_t nContent=htons(content);
    memcpy(&destMsg[msgPos],&nContent,2);
    msgPos+=2;
    return msgPos;
}
uint32_t copy2msg(MYMSG* destMsg,uint32_t content,uint32_t msgPos,bool hasLength)
{
    if(hasLength)
    {
        uint32_t nlength=htonl(4);
        memcpy(&destMsg[msgPos],&nlength,4);
        msgPos+=4;
    }
    uint32_t nContent=htonl(content);
    memcpy(&destMsg[msgPos],&nContent,4);
    msgPos+=4;
    return msgPos;
}

uint32_t fetchFmsg(MYMSG* sourMsg,uint32_t* result,uint32_t msgPos)
{
    memcpy(&sourMsg[msgPos],result,4);
    *result=ntohl(*result);
    return msgPos+4;
}
uint32_t fetchFmsg(MYMSG* sourMsg,uint16_t* result,uint32_t msgPos)
{
    memcpy(&sourMsg[msgPos],result,2);
    *result=ntohs(*result);
    return msgPos+2;
}

int createLoginMsg(MYMSG* destMsg,char* userName, uint16_t portNum)
{
    uint16_t posMsg=1;
    destMsg[0]=LOGIN;
    uint32_t length=strlen(userName);
    posMsg=copy2msg(destMsg,length,(uint8_t*)userName,posMsg);
    posMsg=copy2msg(destMsg,portNum,posMsg,true);
    return length+11;
}


int createLoginOkMsg(MYMSG* destMsg)
{
    destMsg[0]=LOGIN_OK;
    return 1;
}

int createGetListMsg(MYMSG* destMsg)
{
    destMsg[0]=GET_LIST;
    return 1;
}

int createErrMsg(MYMSG* destMsg,uint8_t errCode)
{
    destMsg[0]=ERROR;
    copy2msg(destMsg,1,&errCode,1);
    return 6;
}

ERRCOD clientList::addUser(MYMSG* sourMsg, uint32_t ipAddr)
{
    if (sourMsg[0]!=LOGIN) return MSGINCOR;
    if (userNum>=10) return TOOMANYUSER;

    uint16_t posMsg=5;
    userInf* newUser=new userInf();
    uint32_t nlength=0;
    memcpy(&nlength,&sourMsg[1],4);
    uint32_t length=ntohl(nlength);
    newUser->nameLength=length;
    memcpy(&(newUser->name),&sourMsg[5],length);
    newUser->name[length+1]=0;
    posMsg+=(length+4);
    uint16_t nPortNum;
    memcpy(&nPortNum,&sourMsg[posMsg],2);
    uint16_t portNum=ntohs(nPortNum);
    newUser->portNum=portNum;
    newUser->ipAddr=ntohl(ipAddr);

    if (start==NULL)
    {
        start=newUser;
    }
    else
    {
        userInf* cur=start;
        userInf* last=NULL;
        while(cur!=NULL)
        {
            if(cur->ipAddr==newUser->ipAddr && cur->portNum==newUser->portNum)
            {
                delete newUser;
                return SMIPNPORT;
            }
            if (strcmp(newUser->name,cur->name)==0)
            {
                delete newUser;
                return SMNAME;
            }
            last=cur;
            cur=cur->next;
        }
        last->next=newUser;
    }
    userNum++;
    return 0;
}

ERRCOD clientList::delUser(char delName[])
{
    bool find=false;
    userInf* cur=start;
    userInf* last=NULL;
    while(cur!=NULL)
    {
        if (strcmp(cur->name,delName)==0)
        {
            if (last==NULL)
            {
                start=cur->next;
                delete cur;
                find=true;
            }
            else
            {
                last->next=cur->next;
                delete cur;
                find=true;
            }
        }
        last=cur;
        cur=cur->next;
    }
    if (find)
    {
        userNum--;
        return 0;
    }
    else return NOSUCHUSER;
}

int clientList::getList(MYMSG* destMsg)
{
    uint32_t msgPos=5;
    uint32_t totalByte=0;
    destMsg[0]=GET_LIST_OK;
    userInf* cur=start;
    while(cur!=NULL)
    {
        uint32_t length=cur->nameLength;
        msgPos=copy2msg(destMsg,length,(uint8_t*)cur->name,msgPos);
        msgPos=copy2msg(destMsg,cur->ipAddr,msgPos,false);
        msgPos=copy2msg(destMsg,cur->portNum,msgPos,false);
        totalByte+=(length+10);
        cur=cur->next;
    }
    copy2msg(destMsg,totalByte,1,false);
    return totalByte+1;
}

clientList::clientList()
{
    userNum=0;
    start=NULL;
}

clientList::~clientList()
{
    userInf* cur=start;
    userInf* last=NULL;
    while(cur!=NULL)
    {
        last=cur;
        cur=cur->next;
        delete last;
    }
}

userInf::userInf()
{
    next=NULL;
}

msgReceiver::msgReceiver(int conSo)
{
    inLength=0;
    conSocket=conSo;
}

ERRCOD msgReceiver::receiveMsg(MYMSG* destMsg)
{
    bool msgValid=false;
    int count,remArg=0,remByte=0;
    uint32_t msgPos=0,bufPos=0;
    uint8_t msgBuf[LMSGL];
    if (inLength!=0)
    {
        memcpy(msgBuf,inBuf,inLength);
        count=inLength;
        inLength=0;
    }
    else
    {
        count=read(conSocket,msgBuf,LMSGL);
        if (count==-1) err("read() in msgReceiver()");
        if (count>0)
        {
            remArg=getArgNum(msgBuf[0]);
            if (remArg==-1) err("msgReceiver",MSGINCOR);
            destMsg[0]=msgBuf[0];
            msgPos++; bufPos++; count--;
            msgValid=true;
        }
    }
    while((remArg>0 || remByte>0) && count>0)
    {
        if(remByte==0)
        {
            bufPos=fetchFmsg(msgBuf,(uint32_t*)&remByte,bufPos);
            remArg--;
        }
        if(remByte!=0)
        {
            if(count>=remByte)
            {
                memcpy(&msgBuf[bufPos],&destMsg[msgPos],remByte);
                count-=remByte;
                msgPos+=remByte;
                bufPos+=remByte;
                remByte=0;
            }
            else
            {
                memcpy(&msgBuf[bufPos],&destMsg[msgPos],count);
                remByte-=count;
                msgPos+=count;
                bufPos=0;
                count=0;
                count=read(conSocket,msgBuf,LMSGL);
                if (count==-1) err("read() in msgReceiver()");
            }
        }
    }
    if (!msgValid) return MSGINCOR;
    return 0;
}

int getArgNum(uint8_t msgType)
{
    switch (msgType)
    {
        case 0x01: return 2;
        case 0x02: return 0;
        case 0x03: return 0;
        case 0x04: return 1;
        case 0x10: return 1;
        case 0x20: return 0;
        case 0x30: return 1;
        case 0xff: return 1;
        default: return -1;
    }
}

