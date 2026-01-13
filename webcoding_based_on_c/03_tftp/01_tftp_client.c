#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<string.h>
#include<sys/stat.h>
#include<fcntl.h>
void download(int sockfd,struct sockaddr_in serveraddr){
    char filename[128] = "";
    printf("输入要下载的文件名");
    scanf("%s",filename);
    //这里是数据报文的传输层级
    unsigned char packet_buf[1024]= "";
    //这个只是临时使用
    //只在sendto里面使用
    //因为packet_buf一直都在变
    int packet_buf_len;
    socklen_t addrlen = sizeof(serveraddr);
    //文件描述符
    int file;
    //判断是否创建文件
    int flag = 0;
    //这个num用来计块
    int num = 0;
    //这个是recv里面用来记录缓冲区数组长度
    ssize_t text_len;
    //构建数据报文
    packet_buf_len = sprintf((char*)packet_buf,"%c%c%s%c%s%c",0,1,filename,0,"octet",0);
    if((sendto(sockfd,packet_buf,packet_buf_len,0,(struct sockaddr*)&serveraddr,addrlen))== -1){
        perror("fail to send");
        exit(1);
    }
    while(1){
        //从这里recvfrom开始我们就要去接受整个数据包
        if(((text_len =recvfrom(sockfd,packet_buf,sizeof(packet_buf),0,(struct sockaddr*)&serveraddr,&addrlen)))==-1){
        perror("fail to recevie");
        exit(1);
    }
    if(packet_buf[1]== 5){
        perror("there is an error in sending data package");
        printf("TFTP Error: %s\n", packet_buf + 4);
        exit(1);
    }
    else if(packet_buf[1]==3){
        if(flag == 0){
            if((file = open(filename,O_WRONLY|O_CREAT|O_TRUNC,0664))<0){
                perror("fail to create file");
                exit(1);
            }else{
                flag = 1;
            }
        }
        if ((num +1) == ntohs(*(unsigned short*)(packet_buf+2))){
                num++;
                if(write(file,packet_buf+4,text_len-4)<0){
                    perror("fail to write");
                    exit(1);
                }
                //返回ack报文
                //报文就是04操作码和对应的num区块值
                packet_buf[1]= 4;
                if(sendto(sockfd,packet_buf,4,0,(struct sockaddr*)&serveraddr,addrlen)<0){
                    perror("fail to send");
                    exit(1);
                }
                if(text_len<516){
                    printf("文件下载完毕");
                    close(file);
                    return;
                }
            }
    }
  }
    //这里recvfrom的核心机制就是根据数据包重新写入
    
}

int main(int argc, char* const argv[]){
    if(argc<2){
         fprintf(stderr,"Usage : %s <ServerIP> <Port>\n", argv[0]);
         exit(1);
    }
    int sockfd;
    if((sockfd = socket(AF_INET,SOCK_DGRAM,0))== -1){
        perror("fail to bind");
        exit(1);
    }
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
    serveraddr.sin_port = htons(69);
    socklen_t addrlen = sizeof(serveraddr);

    download(sockfd,serveraddr);
    return 0;
}