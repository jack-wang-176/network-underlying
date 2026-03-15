# 03 TFTP Implementation (TFTP 协议实现)

*Trivial File Transfer Protocol* (简单文件传输协议)。

* **begin**
  * 在开始之前我想简单的介绍一下理解tftp的核心所在，作为基于udp协议的小文件传输协议，在c中实现tftp最让人恼火也最为关键的就是**手动构建和分析二进制报文**，你需要去拼凑每一个字节。


* **[01_tftp_client](../../../01_socket_underlying_c//03_tftp/01_tftp_client.c)**
* **Part 1: 报文构造区**
  * 明确下载文件名 `scanf("%s",filename);`
  * 第一个难点是去构造数据报文，这玩意不是字符串，是紧凑的二进制。
  * **TFTP 二进制报文结构图**：
    ```text
    2 bytes     string    1 byte     string   1 byte
    ------------------------------------------------
    | Opcode |  Filename  |   0  |    Mode    |   0  |
    ------------------------------------------------
    ```


  * 代码里用了一个很巧妙的操作 `sprintf` 来拼接：
    ```c
    packet_buf_len = sprintf((char*)packet_buf,"%c%c%s%c%s%c",0,1,filename,0,"octet",0);
    ```


  * **解释一下字节序**：这里为什么不需要在意大端存储和小端存储的转换？因为 `sprintf` 是按顺序写入单字节的。写入 0 再写入 1，内存里就是 `00 01`，这恰好符合网络字节序的大端要求。


* **Part 2: 接收与解析循环 (State Machine)**
  * packet_buf 用来接受server发送来的数据，这里发送数据全部使用unsigned char 类型来进行发送，而通过数据来储存这个数据，意味者可以简单直接的通过使用这个数组来对数据包头进行解析
    ```c
     //这里是数据报文的传输层级
      unsigned char packet_buf[1024]= "";
    ```


    ```c
    //错误信息
    if(packet_buf[1]== 5){...exit(1)}
    //收到server正确的反馈请求
    if(packet_buf[1]==3){//进行下一步处理}
    ```


  * 需要注意的是首先要去判断是否存在相应文件，可以用bool或int类型数据来标识，如果没有则需要先创建相应文件


* **Part 3: 验证与ACK (手动可靠性)**
  * 另一个难点就是接受核对数据保重的区块编号，因为udp是不可靠的连接，所以说需要手动去验证是否存在数据丢失。我们需要从数据报头中读取并和本地记录的进行比对。
  * **数据验证流程**：


    ```c
    if((num +1) == ntohs(*(packet_buf+2)))
    //success -> 发送ack报文
    //fail    -> 数据丢失，推出
    ```


  * 如果当前数据包没问题，需要构建ack报文发送给server，来让他来发送下一块数据。这里必须用 `ntohs`，因为包头里的序号是网络序，得转成本地序才能对比。


    ```c
    packet_buf[1]= 4;
    ```


  * 这里尽管发送了文件块数据，但是server只需要对数据包头进行验证
  * 如果数据块小于516，即文件数据小于512，那么说明写入结束，但在这里没有去考虑文件大小刚好是512倍数的情况


  * **总结**：这样一个客户端主要的难题就是二进制报文的处理，因为udp本身是不可靠的，所以我们就需要手工做数据包做数据包头进行验证，我们可以看到的是，这样一个验证思路实际上和tcp的三次握手非常相像，所以一般由tcp承担文件传输的工作


* **[02_tftp_server](../../../01_socket_underlying_c//03_tftp/02_tftp_server.c)**
  * 服务端逻辑相对被动，主要是解析和反馈：
    1. 验证数据报文和是否由相关文件


    2. 自定义区块num并写入包头，用缓冲区作为文件数据中转


    3. 等待并解析ack数据包


  * **构造错误包 (辅助函数)**
  这里需要注意的是一个构建错误数据包的函数，为了代码整洁抽出来的：
    ```c
    void senderr(int sockfd,struct sockaddr* clientaddr,char* err,int errcode,socklen_t addrlen){
      unsigned char buf[516] = "";
      // 构造错误包: [00] [05] [00] [ErrCode] [ErrMsg] [00]
      int buf_len = sprintf((char*)buf, "%c%c%c%c%s%c", 0, 5, 0, errcode, err, 0);
      sendto(sockfd,buf,buf_len,0,clientaddr,addrlen);
    }
    ```


  * 本身这样的函数使得我们可以迅速的在逻辑末节点（比如文件打不开）发送相关信息。
  * **总结**：这里服务端展现的设计思想基本与客户端一致，这里需要注意的是两边都在本地储存了区块号,双方都对数据报文进行了解析
