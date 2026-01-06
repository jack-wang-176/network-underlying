#include<stdio.h>
#include<arpa/inet.h>

int main(int argc, char const* argv[]){
    int a = 0x12345678;
    short b = 0x1234;
    //host to net long int
    //0x78563412
    //16位 2的四次方。一个数是四位，两个数是一个字节
    //这里两个数字是一位。改成大端存储低地址对应后进来的数
    printf("%#x\n",htonl(a));
    printf("%#x\n",htons(b));
    return 0;

}