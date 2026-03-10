#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<fcntl.h>
#include<sys/fcntl.h>
#include<sys/ioctl.h>
#include<linux/if.h>
#include<linux/if_tun.h>

int tun_alloc(char *dev){
    int fd,err;
    struct ifreq ifr;
    memset(&ifr,0,sizeof(ifr));
    if((fd = open("/dev/net/tun",O_RDWR))<0){
        perror("fail to open /dev/net/tun");
        return fd;
    }
    ifr.ifr_flags |= IFF_TAP |IFF_NO_PI;
    if(*dev){
        strncpy(ifr.ifr_name,dev,IFNAMSIZ);
    }
    if((err = ioctl(fd,TUNSETIFF,&ifr))<0){
        perror("fail to ioctl tun");
        close(fd);
        return err;
    }
    strcpy(dev,ifr.ifr_name);
    return fd;
}
int main(int argc, char*argv[]){
    char tap_name[IFNAMSIZ] = "tap0";
    int tap_fd = tun_alloc(tap_name);
    if(tap_fd<0){
        fprintf(stderr, "Error connecting to tap interface %s!\n", tap_name);
        exit(1);
    }
    printf("Successfully attached to %s\n", tap_name);
    printf("Listening for packets...\n\n");

    unsigned char buffer[1522];
    while(1){
        ssize_t nread = read(tap_fd,buffer,sizeof(buffer));
        if (nread < 0){
            perror("fail to read");
            continue;
        } 
        printf("Read %zd bytes from %s:\n", nread, tap_name);
        
        for (int i = 0; i < nread; i++) {
            printf("%02x ", buffer[i]);
            if ((i + 1) % 16 == 0)
            printf("\n"); 
        }
        printf("\n\n");
    }
    close(tap_fd);
    return 0;
}