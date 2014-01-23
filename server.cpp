#include <stdio.h>
#include "protocol.h"
int main(void)
{
    MYMSG msg[MSGL];
    char name[512];
    scanf("%s",name);
    createLoginMsg(msg,name,12345);
    return 0;
}
