#include<stdio.h>

union un{
    int a;
    char b;
};
int main(int argc,char const *argv[]){
    union un myun;
    myun.a = 0x12345678;
    //这里是小端
    //低地址对应后存储进来的数
    if(myun.b == 0x78){
        printf("小端存储");
    }else{
        printf("大端存储");
    }

    return 0;
}