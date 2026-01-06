#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#define TFTP_PORT 69
#define BLK_SIZE 512
//发送错误数据报文
void senderr(int sockfd,struct sockaddr* clientaddr,char* err,int errcode,socklen_t addrlen){
  unsigned char buf[516] = "";
  //构造错误数据报文
  // 构造错误包: [05] [ErrCode] [ErrMsg] [0]
  int buf_len = sprintf((char*)buf, "%c%c%c%c%s%c", 0, 5, 0, errcode, err, 0);
  sendto(sockfd,buf,buf_len,0,clientaddr,addrlen);
}

void buildtftpserver(int sockfd){
    //记录客户端的ip和port
    struct sockaddr_in clientaddr;
    //这里来记录发送的tftp数据报文,数据报文必须是unsigned
    unsigned char packet_buf[1024] = "";
    //文件名
    char filename[128]= "";
    //文件描述符，类似sockfd的概念
    int file;
    //记录客户端的addrlen
    socklen_t addrlen_cl = sizeof(clientaddr);

    while(1){
        //每次循环都要重置，因为可能有多个客户端
        //在循环最开始时重置
        //因为recvfrom会进行修改
        addrlen_cl = sizeof(clientaddr);
        //记录第一次从客户端发来的tftp数据报文
        if((recvfrom(sockfd,packet_buf,sizeof(packet_buf),0,(struct sockaddr*)&clientaddr,&addrlen_cl))<0){
            perror("fail to recevie packet_buf of client");
            continue;
        }
        // "1" 是字符串（双引号），在C语言中这代表内存地址。
        // packet_buf[1] 是一个字节（整数）。
        // 一个整数永远不可能等于一个内存地址，这个 if 永远为假。
        if(packet_buf[1] == 1){
            
            //这里filename是具体的字符
            //将二进制报文解析出来
            strcpy(filename,(char*)packet_buf+2);
            file = open(filename,O_RDONLY);
            if (file <0){
                senderr(sockfd,(struct sockaddr*)&clientaddr,"file not found",1,addrlen_cl);
                fprintf(stderr,"not such file");
                continue;
            }

            unsigned char fl_ctx[BLK_SIZE];
            //tftp里面数据内容为两个字节
            unsigned short bl_num = 0;
            int fl_ctx_l;

            
            //进行文件发送
            while(1){
                //将file文件的内容暂时储存在fl_ctx中
                fl_ctx_l =read(file,fl_ctx,BLK_SIZE);
                if (fl_ctx_l < 0) break; // 读取错误处理
                bl_num++;
                //构造数据报文并发送
                //最后应该是516个字节的数据报文
                unsigned char sd_pk[516];
                sd_pk[0] = 0;
                sd_pk[1] = 3;
                //最后还是要以二进制的格式进行输出
                *(unsigned short*)(sd_pk+2)=htons(bl_num);
                //文件数据在缓冲区里面
                memcpy(sd_pk + 4, fl_ctx, fl_ctx_l);
                //这里不能直接用sizeof(sd_pk)，因为客户端会对数据长度进行解析
                if((sendto(sockfd,sd_pk,fl_ctx_l + 4,0,(struct sockaddr*)&clientaddr,addrlen_cl))== -1){
                    perror("fail to send");
                    //发送失败也就没有必要去等待数据报文
                    break;
                }
                //等待并分析ack报文
                unsigned char ack_ctx[516];
                int ack_len = recvfrom(sockfd, ack_ctx, sizeof(ack_ctx), 0, (struct sockaddr*)&clientaddr, &addrlen_cl);
                
                if(ack_len < 0){
                    perror("no ack response");
                    break;
                }
                unsigned short ack_bl = ntohs(*(unsigned short*)(ack_ctx+2));
                //必须有ack_len这个变量，如果没有而使用sizeof值恒定为初始化的数组大小
                if(ack_len>=4&&ack_ctx[1]== 4&&ack_bl== bl_num){
                    // 收到正确的 ACK，准备下一轮
                     printf("Block %d ACKed\n", bl_num);
                }else {
                    // 收到错误的块号 (可能是以前的包延迟到了)
                    printf("Wrong ACK number, expected %d got %d\n", bl_num, ack_bl);
                    // 在简单实现中，这里可以选择忽略或重发，为简单起见我们break
                    break;
                }
                if (fl_ctx_l < BLK_SIZE) {
                    printf("File transfer finished.\n");
                    break;
                }

            }
            //下次注意写的时候就要把关闭写上
            close(file);
        }


    }



}

int main(int argc,char const *argv[]){
   
    int sockfd;
    if((sockfd = socket(AF_INET,SOCK_DGRAM,0))<0){
        perror("fail to build socket");
        exit(1);
    }
    struct sockaddr_in serveraddr;
    memset(&serveraddr,0,sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(TFTP_PORT);
    socklen_t addrlen = sizeof(serveraddr);

    if((bind(sockfd,(struct sockaddr*)&serveraddr,addrlen))== -1){
        perror("fail to bind");
        exit(1);
    }

    buildtftpserver(sockfd);
    close(sockfd);
    return 0;

}