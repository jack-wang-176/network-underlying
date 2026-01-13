#include<stdio.h>
#include<arpa/inet.h>

int main(){
    unsigned char ip_int[] = {192,168,3,103};
    char ip_str[16]= "";
    //将点分十进制数传转变为32位无符号整数。
    //AF_INET是协议运算符
    //IP int 实际上可以是void类型，最后返回一个字符串。
    inet_ntop(AF_INET,&ip_int,ip_str,16);
    printf("ip_s = %s\n",ip_str);
    return 0;
}