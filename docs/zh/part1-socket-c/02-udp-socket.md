# 02 UDP Socket (UDP 通信)
无连接的、不可靠的数据传输协议。
* **[01_socket](../../../01_socket_underlying_c//02_udp/01_socket.c)**
    * 展示了socket套接字创建的函数
      ```c
      int socket (int __domain, int __type, int __protocol)
      ```
    * domain决定IP类型，type则是决定tcp还是udp的传输类型。protocol是具体的协议格式
    * socket是一个int类型，靠文件描述符的抽象在系统层面上实现调用，展现了linux中一切皆文件的设计思想
    * 作为文件描述符，一定要在程序最后对其进行关闭，close在通信过程中就意味着断开连接，在tcp中，这一点变得更为复杂
* **[02_sendto](../../../01_socket_underlying_c//02_udp/02_sendto.c)**
   * 展示了udp传输类型的数据发送
      ```c
      ssize_t sendto (int __fd, const void *__buf, size_t __n,int __flags, __CONST_SOCKADDR_ARG __addr,socklen_t __addr_len);
      ```
  * **ssize_t 是个啥**：其实就是 `signed int`。因为这函数成功时返回发送字节数（正数），失败要返回 -1。如果用普通的 `size_t`（无符号），就没法表示 -1 这个错误状态了。
  * **adding**
  * 在这里进行数据传输的时候通过addrsocket_in记录和写入ip和port


    ```c
    struct sockaddr_in
    {
    __SOCKADDR_COMMON (sin_);
    in_port_t sin_port;/* Port number. */
    struct in_addr sin_addr;/* Internet address. */

    /* Pad to size of `struct sockaddr'.  */
    // ... padding ...
    };
     ```
  * 需要注意的是，尽管我们写入的是sockaddr_in但是封装函数写入的确实sockaddr结构体


    ```c
    struct sockaddr
    {
    __SOCKADDR_COMMON (sa_);	/* Common data: address family and length.  */
    char sa_data[14];		/* Address data.  */
    };
    ```


  * 可以看到，这实际上是进行了一个数据压缩的过程，这样的设计展现了编程最核心的问题，自然语言编程和机器二进制构成的矛盾


* **[03_bind](../../../01_socket_underlying_c//02_udp/03_bind.c)**
  * bind这个函数主要是为了固定ip和端口号，根据这个函数的面向我们可以很容易理解信息的接收方更加需要这个需求。



    ```c
    int bind (int __fd, __CONST_SOCKADDR_ARG __addr, socklen_t __len)
        __THROW;
    ```

  * 在tcp/udp编程时我们一般简单的就把server称为需要绑定的一方，但这实际上是由于信息接受和发送的相对关系所决定的，在多播和组播中我们能够看到这一点的进一步体现
* **[04_recvfrom](../../../01_socket_underlying_c//02_udp/04_recvfrom.c)**
  * recvform是udp接受函数
    ```c
    recvfrom (int __fd, void *__restrict __buf, size_t __n, int __flags,
        __SOCKADDR_ARG __addr, socklen_t *__restrict __addr_len)
    ```


  * 这里需要注意的是recvfrom是接受别的主机的数据，所以我们需要预先创建空的结构体供其填入，并且还改变addrlen来作为接受到的信息长度输出，这种设计使得udp可以很简单的实现多线程工作，代价就是每一个client都需要相应的结构体来对应，而我们将会在后续看到，因为tcp要求的三次握手，导致tcp的设计走向了截然不同的道路


* **[05_server](../../../01_socket_underlying_c//02_udp/05_server.c)&&[06_client](../../../01_socket_underlying_c//02_udp/06_client.c)** 
  * 这一段是具体的udp的客户端和服务端的运行代码。本质上就是调用上述函数具体实现通信过程
    ```c
      if(argc<3){
    fprintf(stderr,"Usage : %s<IP> <PORT>\n",argv[0]);
    exit(1);
    } 
    ```


  * 这一段保证了运行程序时输入了ip和port，这里需要注意的是，client里面输入的也是server的ip和port，因为client根本不需要在乎自己，他只需要保证数据交互


    ```c
    if(recvfrom(sockfd,buf,sizeof(buf),0,(struct sockaddr*)&clientaddr,&addrlen)==-1){
        perror("fail to recvfrom");
        //即时接受失败也可以继续进行
        continue;
    }
    ```


  * 这里体现了recvfrom的核心用处，是udp传输中的核心所在，通过接受数据，将数据发送主机的ip记录下来，用来进行sendto操作，这种设计使得udp的多client能极为容易实现，尽管在实际执行写起来的时候稍微有点冗余。
* **udp通信交互流程简图**：


```text
[Client]                          [Server]
   |                                 |
   |--- sendto(Data, ServerIP) ----->|
   |                                 | recvfrom (获取 ClientIP)
   |                                 |
   |<-- sendto(Echo, ClientIP) ------|
   |                                 |
recvfrom(Echo)

```


* **adding**
  * 我们这里的输入输出主要使用fgets和printf方法


    ```c
    extern char *fgets (char *__restrict __s, int __n, FILE *__restrict __stream)
      __wur __fortified_attr_access (__write_only__, 1, 2) __nonnull ((3));
    ```


  * **缓冲区的大坑**：在c中string以'/0'作为在内存中的数据界限，而fgets则会将换行符计入，如果为了数据的纯洁性应该将换行符去掉，但是对于printf中来说只有遇到换行符才会将数据从缓冲区中打印出。所以这两个函数搭配时不需要做什么处理。