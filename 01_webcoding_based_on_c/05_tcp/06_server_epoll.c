#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<sys/epoll.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<errno.h>
#include<fcntl.h>

#define MAX_EVENTS 10

void unblock(int sockfd){
    int flag = fcntl(sockfd,F_GETFL,0);
    fcntl(sockfd,F_SETFL,flag | O_NONBLOCK,0);
}
void add_ev(int epollfd,int sockfd){
    struct epoll_event ev;
    ev.data.fd = sockfd;
    ev.events =EPOLLIN | EPOLLET;
    if((epoll_ctl(epollfd,EPOLL_CTL_ADD,sockfd,&ev))<0){
        perror("fail to add epoll");
        exit(1);
    }
}
int communication(int sockfd){
    char buf[128]= "";
    int len = sizeof(buf)-1;
    int num;
    while(1){
        num = recv(sockfd,buf,len,0);
        if(num == 0){
            printf("client exit\n");
            return -1;
        }else if(num <0){
            if(errno == EWOULDBLOCK || errno == EAGAIN){
                return 0;
            }else{
                perror("fail to recv");
                return -1;
            }

        }else{     
            buf[num] = '\0';
            printf("%s\n",buf);
            if((send(sockfd,buf,num,0))<0)
            {
                if (errno == EWOULDBLOCK || errno == EAGAIN)
                {
                    return 0;
                }
                else
                {
                    perror("fail to recv");
                    return -1;
                }
            }
            memset(buf,0,sizeof(buf));
        }

    }
}

int main(int argc, char const* argv[]){
    if(argc<3){
        fprintf(stderr,"Usage %s <IP><Port>",argv[0]);
        exit(1);
    }

    int sockfd;
    if((sockfd = socket(AF_INET,SOCK_STREAM,0))<0){
        perror("fail to buil socket");
        exit(1);
    }
    unblock(sockfd);

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[2]));
    server.sin_addr.s_addr= inet_addr(argv[1]);
    socklen_t addr_len = sizeof(server);

    if((bind(sockfd,(struct sockaddr*)&server,addr_len)<0)){
        perror("fail to bind");
        exit(1);
    }

    if((listen(sockfd,5))<0){
        perror("fail to listen");
        exit(1);
    }

    int epollfd;
    struct epoll_event ev,evs[MAX_EVENTS];
    if((epollfd= epoll_create1(0))<0){
        perror("fail to build epoll");
        exit(1);
    }

    ev.data.fd = sockfd;
    ev.events = EPOLLIN | EPOLLET;
    if((epoll_ctl(epollfd,EPOLL_CTL_ADD,sockfd,&ev))<0){
        perror("fail to add into epoll");
        exit(1);
    }

    int ev_num;
    struct sockaddr_in client;
    while(1){
        ev_num = epoll_wait(epollfd,evs,MAX_EVENTS,-1);
        for(int i =0;i<ev_num;i++){
            int now_sock = evs[i].data.fd;
            if(now_sock == sockfd){
                while(1){
                    int new_sock = accept(sockfd, (struct sockaddr *)&client, &addr_len);
                    if(new_sock<0){
                        if(errno == EWOULDBLOCK || errno == EAGAIN){
                            break;
                        }else{
                            perror("fail to accept");
                            break;
                        }
                    }
                    unblock(new_sock);
                    add_ev(epollfd, new_sock);
                }  
            }else{
                int flag_com =communication(now_sock);
                if(flag_com == -1){
                    epoll_ctl(epollfd,EPOLL_CTL_DEL,now_sock,NULL);
                    close(now_sock);
                }
            }
        }    
    }
    close(epollfd);
    close(sockfd);
    return 0;
}



