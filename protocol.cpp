#include "debug.h"
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
    memcpy(result,&sourMsg[msgPos],4);
    *result=ntohl(*result);
    return msgPos+4;
}
uint32_t fetchFmsg(MYMSG* sourMsg,uint16_t* result,uint32_t msgPos)
{
    memcpy(result,&sourMsg[msgPos],2);
    *result=ntohs(*result);
    return msgPos+2;
}

ERRCOD clientListParse(MYMSG* sourMsg, userInf* destStr, int* num)
{
    if (sourMsg[0]!=GET_LIST_OK) return MSGINCOR;
    uint32_t length;
    fetchFmsg(sourMsg,&length,1);
    int count=0;
    uint32_t msgPos=5;
    while(length!=0)
    {
        uint32_t sLength,iniPos=msgPos;
        msgPos=fetchFmsg(sourMsg,&sLength,msgPos);
        destStr[count].nameLength=sLength;
        memcpy(destStr[count].name,&sourMsg[msgPos],sLength);
        msgPos+=sLength;
        msgPos=fetchFmsg(sourMsg,&(destStr[count].ipAddr),msgPos);
        msgPos=fetchFmsg(sourMsg,&(destStr[count].portNum),msgPos);
        length-=(msgPos-iniPos);
        count++;
    }
    *num=count;
    return 0;
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

int createHelloMsg(MYMSG* destMsg,char* sName)
{
    destMsg[0]=HELLO;
    int length=strlen(sName);
    copy2msg(destMsg,length,(uint8_t*)sName,1);
    return length+5;
}
int createHelloOkMsg(MYMSG* destMsg)
{
    destMsg[0]=HELLO_OK;
    return 1;
}
int createMsg(MYMSG* destMsg, char* msgContent)
{
    destMsg[0]=MSG;
    int length=strlen(msgContent);
    copy2msg(destMsg,length,(uint8_t*)msgContent,1);
    return length+5;
}

ERRCOD clientList::addUser(MYMSG* sourMsg, uint32_t ipAddr, userInf** thisUser)
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
    *thisUser=newUser;
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
    return totalByte+5;
}

clientList::clientList()
{
    userNum=0;
    start=NULL;
}

clientList::~clientList()
{
    #ifdef DEBUG
        printf("clientList destroyed\n");
    #endif
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

recMsg::recMsg()
{
    next=NULL;
}

recMsgBuf::recMsgBuf()
{
    unreadNum=0;
    start=NULL;
}

recMsgBuf::~recMsgBuf()
{
    #ifdef DEBUG
        printf("recMsgBuf destroyed\n");
    #endif
    recMsg* cur=start;
    recMsg* last=NULL;
    while(cur!=NULL)
    {
        last=cur;
        cur=cur->next;
        delete(last);
    }
}

ERRCOD recMsgBuf::addMsg(MYMSG* sourMsg,char name[])
{
    if(sourMsg[0]!=MSG) return MSGINCOR;
    recMsg* curMsg=new recMsg();

    strcpy(curMsg->fromName,name);
    uint32_t length;
    fetchFmsg(sourMsg,&length,1);
    memcpy(curMsg->msgContent,&sourMsg[5],length);
    curMsg->msgContent[length]=0;

    if (start==NULL)
    {
        start=curMsg;
        unreadNum++;
        return 0;
    }
    recMsg* cur=start;
    recMsg* last=NULL;
    while(cur!=NULL)
    {
        last=cur;
        cur=cur->next;
    }
    last->next=curMsg;
    unreadNum++;
    return 0;
}

int recMsgBuf::getAllMsg(recMsg* destArr)
{
    if (unreadNum>BUFMAX) return BUFTOOSM;
    recMsg* cur=start;
    int count=0;
    while(cur!=NULL)
    {
        strcpy(destArr[count].msgContent,cur->msgContent);
        strcpy(destArr[count].fromName,cur->fromName);
        cur=cur->next;
        count++;
    }
    return unreadNum;
}

ERRCOD recMsgBuf::delAllMsg()
{
    recMsg* cur=start;
    recMsg* last=NULL;
    while(cur!=NULL)
    {
        last=cur;
        cur=cur->next;
        delete last;
    }
    start=NULL;
    unreadNum=0;
    return 0;
}

int recMsgBuf::getUnreadNum()
{
    return unreadNum;
}

restMsgBuf::restMsgBuf()
{
    next=NULL;
    msgLength=-1;
}

restMsgBuf::restMsgBuf(MYMSG* sourMsg, uint32_t length)
{
    next=NULL;
    memcpy(content,sourMsg,length);
    msgLength=length;
}

void restMsgBuf::insert(MYMSG* sourMsg, uint32_t length)
{
    restMsgBuf* cur=this;
    while(cur->next!=NULL) cur=cur->next;
    cur->next=new restMsgBuf(sourMsg,length);
}

ERRCOD restMsgBuf::get(MYMSG* destMsg,uint32_t* length)
{
    restMsgBuf* cur=this;
    while(cur->next!=NULL) cur=cur->next;
    if(cur->msgLength<0) return NOTFIND;
    else
    {
        *length=cur->msgLength;
        memcpy(destMsg,content,*length);
        return 0;
    }
}

ERRCOD restMsgBuf::getTypeA(MYMSG* destMsg,uint32_t* length)
{
    restMsgBuf* cur=this;
    while(cur->next!=NULL)
    {
        if (getMsgType(cur->next->content)==TYPEA)
        {
            *length=cur->next->msgLength;
            memcpy(destMsg,cur->next->content,*length);
            restMsgBuf* temp=cur->next;
            cur->next=cur->next->next;
            delete temp;
            return 0;
        }
        cur=cur->next;
    }
    return NOTFIND;
}

ERRCOD restMsgBuf::getTypeB(MYMSG* destMsg,uint32_t* length)
{
    restMsgBuf* cur=this;
    while(cur->next!=NULL)
    {
        if (getMsgType(cur->next->content)==TYPEB)
        {
            *length=cur->next->msgLength;
            memcpy(destMsg,cur->next->content,*length);
            restMsgBuf* temp=cur->next;
            cur->next=cur->next->next;
            delete temp;
            return 0;
        }
        cur=cur->next;
    }
    return NOTFIND;
}

msgReceiver::msgReceiver(int conSo)
{
    inLength=0;
    conSocket=conSo;
    rStart=new restMsgBuf;
    pthread_mutex_init(&mutex,NULL);
    pthread_cond_init(&cond,NULL);
}

msgReceiver::~msgReceiver()
{
    #ifdef DEBUG
        printf("msgReceiver destroyed\n");
    #endif
    restMsgBuf* cur=rStart;
    restMsgBuf* temp=NULL;
    while(cur!=NULL)
    {
        temp=cur;
        cur=cur->next;
        delete temp;
    }
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
}
ERRCOD msgReceiver::receiveTypeAMsg(MYMSG* destMsg, uint32_t* msgLength)
{
    pthread_mutex_lock(&mutex);
    while(rStart->getTypeA(destMsg,msgLength)==NOTFIND)
    {
        pthread_cond_wait(&cond,&mutex);
    }
    pthread_mutex_unlock(&mutex);
    return 0;
}


ERRCOD msgReceiver::receiveTypeBMsg(MYMSG* destMsg, uint32_t* msgLength)
{
    #ifdef DEBUG
        printf("[receiverB] receive type B message\n");
    #endif
    if(rStart->getTypeB(destMsg,msgLength)!=NOTFIND) return 0;
    #ifdef DEBUG
        printf("[receiverB] not find in buffer\n");
    #endif
    if(receiveMsg(destMsg,msgLength)==DISCONNECT)
    {
        return DISCONNECT;
    }
    while(getMsgType(destMsg)!=TYPEB)
    {
        pthread_mutex_lock(&mutex);
        rStart->insert(destMsg,*msgLength);
        pthread_mutex_unlock(&mutex);
        pthread_cond_signal(&cond);
        receiveMsg(destMsg,msgLength);
    }
    return 0;
}

ERRCOD msgReceiver::receiveMsg(MYMSG* destMsg,uint32_t* msgLength)
{
    bool msgValid=false;
    int count,remArg=0,remByte=0;
    uint32_t msgPos=0,bufPos=0;
    uint8_t msgBuf[LMSGL];
    (*msgLength)=0;
    if (inLength!=0)
    {
        memcpy(msgBuf,inBuf,inLength);
        count=inLength;
        (*msgLength)+=inLength;
        inLength=0;
    }
    else
    {
        count=read(conSocket,msgBuf,LMSGL);
        if (count==0) return DISCONNECT;
        if (count==-1) err("read() in msgReceiver()");
        if (count>0)
        {
            remArg=getArgNum(msgBuf[0]);
            if (remArg==-1) err("msgReceiver",MSGINCOR);
            destMsg[0]=msgBuf[0];
            msgPos++; bufPos++; count--; (*msgLength)++;
            msgValid=true;
        }
    }
    while(remArg>0 || remByte>0)
    {
        if(remByte==0)
        {
            if(count>=4)
            {
                uint32_t remByte_n;
                memcpy(&remByte_n,&msgBuf[bufPos],4);
                remByte=ntohl(remByte_n);
                memcpy(&destMsg[msgPos],&remByte_n,4);
                bufPos+=4; msgPos+=4; count-=4;
                (*msgLength)+=4;
                remArg--;
            }
            else
            {
                int rem=count;
                count=read(conSocket,&msgBuf[bufPos+rem],LMSGL-bufPos-rem);
                if(count==0) return DISCONNECT;
                else if (count==-1) err("read() in msgReceiver()");
                count+=rem;
            }
        }
        if(remByte!=0)
        {
            if(count>=remByte)
            {
                memcpy(&destMsg[msgPos],&msgBuf[bufPos],remByte);
                count-=remByte;
                msgPos+=remByte;
                bufPos+=remByte;
                (*msgLength)+=remByte;
                remByte=0;
            }
            else
            {
                memcpy(&destMsg[msgPos],&msgBuf[bufPos],count);
                remByte-=count;
                msgPos+=count;
                (*msgLength)+=count;
                bufPos=0;
                count=0;
                count=read(conSocket,msgBuf,LMSGL);
                if (count==0) return DISCONNECT;
                else if (count==-1) err("read() in msgReceiver()");
            }
        }
    }
    if (!msgValid) return MSGINCOR;
    return 0;
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
        if (count==0) return DISCONNECT;
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
                memcpy(&destMsg[msgPos],&msgBuf[bufPos],remByte);
                count-=remByte;
                msgPos+=remByte;
                bufPos+=remByte;
                remByte=0;
            }
            else
            {
                memcpy(&destMsg[msgPos],&msgBuf[bufPos],count);
                remByte-=count;
                msgPos+=count;
                bufPos=0;
                count=0;
                count=read(conSocket,msgBuf,LMSGL);
                if (count==0) return DISCONNECT;
                else if (count==-1) err("read() in msgReceiver()");
            }
        }
    }
    if (!msgValid) return MSGINCOR;
    return 0;
}


clientConnected::clientConnected()
{
    next=NULL;
    connected=false;
    rec=NULL;
}

clientConnectedList::clientConnectedList()
{
    start=NULL;
}

clientConnectedList::~clientConnectedList()
{
    #ifdef DEBUG
        printf("clientConnectedList destroyed\n");
    #endif
    clientConnected* cur=start;
    clientConnected* temp=NULL;
    while(cur!=NULL)
    {
        temp=cur;
        cur=cur->next;
        delete temp;
    }
}

ERRCOD clientConnectedList::makeConnection(char* screenName)
{
    if (connectionStatus(screenName)!=DIS) return CONNECTED;
    clientConnected* curUser=new clientConnected();
    strcpy(curUser->name,screenName);
    curUser->connected=false;

    clientConnected* cur=start;
    clientConnected* temp=NULL;

    if (start==NULL) start=curUser;
    else while(cur!=NULL)
    {
        temp=cur;
        cur=cur->next;
    }
    temp->next=curUser;
    return 0;
}

ERRCOD clientConnectedList::connectWith(char* screenName,int socket,msgReceiver** receiver)
{
    int existSocket;
    msgReceiver* existRec;
    if (connectionStatus(screenName,&existSocket,&existRec)!=DIS)
    {
        if (existSocket==socket)
        {
            if(*receiver!=existRec)
            {
                delete *receiver;
                *receiver=existRec;
                return CONNECTED;
            }
        }
        else return TOOMANYCONNECTION;
    }
    clientConnected* curUser=new clientConnected();
    strcpy(curUser->name,screenName);
    curUser->connected=true;
    curUser->socketFd=socket;
    curUser->rec=*receiver;

    clientConnected* cur=start;
    clientConnected* temp=NULL;

    if (start==NULL) start=curUser;
    else
    {
        while(cur!=NULL)
        {
            temp=cur;
            cur=cur->next;
        }
        temp->next=curUser;
    }
    return 0;
}

ERRCOD clientConnectedList::disconnectWith(char* screenName)
{
    if(connectionStatus(screenName)==DIS) return NOSUCHUSER;

    clientConnected* cur=start;
    clientConnected* temp=NULL;

    if(strcmp(start->name,screenName)==0) start=start->next;
    else while(cur!=NULL)
    {
        if(strcmp(cur->name,screenName)==0)
        {
            temp->next=cur->next;
            delete cur;
            break;
        }
    }
    return 0;
}

int clientConnectedList::connectionStatus(char* screenName)
{
    clientConnected* cur=start;
    while(cur!=NULL)
    {
        if (strcmp(cur->name,screenName)==0)
        {
            if (cur->connected) return CONNECTED;
            else return CONNECTING;
        }
    }
    return DIS;
}

int clientConnectedList::connectionStatus(char* screenName,int* destSocket,msgReceiver** receiver)
{
    clientConnected* cur=start;
    while(cur!=NULL)
    {
        if (strcmp(cur->name,screenName)==0)
        {
            if (cur->connected)
            {
                *destSocket=cur->socketFd;
                *receiver=cur->rec;
                return CONNECTED;
            }
            else return CONNECTING;
        }
    }
    return DIS;
}

sendMsg::sendMsg()
{
    next=NULL;
}

ERRCOD sendMsgBuf::insertMsg(char* sourStr)
{
    sendMsg* thisMsg=new sendMsg();

    uint32_t length=strlen(sourStr);
    thisMsg->msgLength=length;
    strcpy(thisMsg->content,sourStr);

    if (start==NULL) start=thisMsg;
    else
    {
        sendMsg* cur=start;
        sendMsg* temp=NULL;
        while(cur!=NULL)
        {
            temp=cur;
            cur=cur->next;
        }
        temp->next=thisMsg;
    }
    return 0;
}

ERRCOD sendMsgBuf::cutMsg(MYMSG* destMsg,uint32_t *length)
{
    if (start==NULL) return EMPTY;

    sendMsg* cur=start;
    start=start->next;

    destMsg[0]=MSG;
    copy2msg(destMsg,cur->msgLength,(uint8_t*)cur->content,1);
    *length=cur->msgLength+5;

    delete cur;
    return 0;
}


void sendMsgBuf::thdStart()
{
    terminate=false;
}
void sendMsgBuf::thdFinish()
{
    terminate=true;
}

bool sendMsgBuf::isTerminate()
{
    return(terminate);
}

bool sendMsgBuf::isEmpty()
{
    if (start==NULL) return true;
    else return false;
}

sendMsgBuf::sendMsgBuf()
{
    start=NULL;
    terminate=true;
}
sendMsgBuf::~sendMsgBuf()
{
    #ifdef DEBUG
        printf("sendMsgBuf destroyed\n");
    #endif
    sendMsg* cur=start;
    sendMsg* temp=NULL;
    while(cur!=NULL)
    {
        temp=cur;
        cur=cur->next;
        delete temp;
    }
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

MSGTYPE getMsgType(MYMSG* msg)
{
    if(msg[0]==0x02 || msg[0]==0x04 || msg[0]==0x10 || msg[0]==0x30) return TYPEB;
    if(msg[0]==0x20) return TYPEA;
    else return UNKNOWN;
}

#ifdef DEBUG

void restMsgBuf::print()
{
    restMsgBuf* cur=next;
    while(cur!=NULL)
    {
        for(int i=0; i<next->msgLength; i++) printf("%d",next->content[i]);
        printf("\n");
    }
}
#endif


