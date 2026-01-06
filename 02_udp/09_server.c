#include <stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>//close
#include<string.h>

int main(int argc,char*argv[]){
   if(argc<3){
    fprintf(stderr,"Usage : %s<IP> <PORT>\n",argv[0]);
    exit(1);
   } 
   struct sockaddr_in serveraddr,clientaddr;
   serveraddr.sin_family = AF_INET;
   serveraddr.sin_addr.s_addr = inet_addr(argv[1]);//点分十进制转化为整形数据
   serveraddr.sin_port= htons(atoi(argv[2]));//
   socklen_t addrlen = sizeof(clientaddr);//atoi数字型字符串转化为整形数据
   //htons 主机字节序转化为网络字节序
   char buf[32] = "";
   int sockfd;
   sockfd = socket(AF_INET,SOCK_DGRAM,0);
//服务端需要进行绑定
   if(bind(sockfd,(struct sockaddr*)&serveraddr,addrlen)==-1){
        perror("fail to bind");
        exit(1);
    }
   printf("UDP Server is running on %s:%s...\n", argv[1], argv[2]);
   //要先接收到数据
   //recvfrom接收到数据并写入clientserver
   //sendto再进行发送
   //这时候serveraddr指定的是自己的
   //sendto只需要标记发出者
   while(1)
   {
    //服务端应该先接受，而且应接受clientaddr
    
    if(recvfrom(sockfd,buf,sizeof(buf),0,(struct sockaddr*)&clientaddr,&addrlen)==-1){
        perror("fail to recvfrom");
        //即时接受失败也可以继续进行
        continue;
    }
    printf("Received from [%s:%d]: %s\n", 
            inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), buf);
    
    //把收到的发回去
    //注意这里的结构体是目标地址而不是重新写入
    if(sendto(sockfd,buf,sizeof(buf),0,(struct sockaddr*)&clientaddr,sizeof(serveraddr))== -1){
        perror("fail to sendto");
    }

   }
   close(sockfd);
   return 0;
    

}