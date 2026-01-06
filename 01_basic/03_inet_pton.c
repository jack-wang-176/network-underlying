#include<stdio.h>
#include<arpa/inet.h>
int main(){
    char ip_str[] = "192.168.3.103";
    unsigned int ip_int = 0;
    unsigned char *ip_p = NULL;
    //将字符串转化为32位无符号整数

    inet_pton(AF_INET,ip_str,&ip_int);
    printf("in_uint = %d\n",ip_int);
    //将这个指向32位无符号整数的指针强转为指向字符串的指针

    //以10进制方式打印，一个一个字节的打印
    ip_p = (char*)&ip_int;
    printf("int_uint = %d %d %d %d\n",*ip_p,*(ip_p+1),*(ip_p+2),*(ip_p+3));
    return 0;
}