#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/ether.h> //ETH_P_ALL
#include<unisted.h>
#include<stdlib.h>
#include<stdio.h>


int main(int argc,char *argv[])
{
    int sockfd;
    if((sockfd = socket(AF_PACKET,SOCK_RAW,htons(EHT_P_ALL))) < 0)
    {
        perror("fail to sockfd");
        exit(1);
    }
    printf("sockfd if %d\n",sockfd);
    close(fd);
    return 0;
    
}