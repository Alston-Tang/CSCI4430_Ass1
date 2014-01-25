#include "protocol.h"
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>

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

ERRCOD createLoginMsg(MYMSG* destMsg,char* userName, uint16_t portNum)
{
    uint16_t posMsg=1;
    destMsg[0]=LOGIN;
    uint32_t length=strlen(userName);
    posMsg=copy2msg(destMsg,length,(uint8_t*)userName,posMsg);
    posMsg=copy2msg(destMsg,portNum,posMsg,true);
    return 0;
}


ERRCOD createLoginOkMsg(MYMSG* destMsg)
{
    destMsg[0]=LOGIN_OK;
    return 0;
}

ERRCOD createGetListMsg(MYMSG* destMsg)
{
    destMsg[0]=GET_LIST;
    return 0;
}

ERRCOD createGetListOkMsg(MYMSG* destMsg, clientList& ServerClientList)
{
    return 0;
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

ERRCOD clientList::getList(MYMSG* destMsg)
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
    return 0;
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
    bool finish=false,msgValid=false;
    int msgType=-1,remArg=0,remByte=0,msgPos=0,count,bufPos=0;
    uint8_t msgBuf[LMSGL];
    if (inLength!=0)
    {
        memcpy(msgBuf,inBuf,inLength);
        count=inLength;
        inLength=0;
        remArg=getArgNum(msgBuf[0]);
        if (remArg==-1) err("getArgNum",-1);

        destMsg[0]=msgBuf[0];
        count--;
        bufPos++;
        msgPos++;

        while(remArg>0 && count>0)
        {


        }
    }
    while(!finish)
    {
        count=read(conSocket,msgBfuf,LMSGL);
        if (!msgValid)
        {

        }
        else
        {

        }
    }
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

