<div align="center">

#  Webcoding
[English Version](./README_EN.md) | **中文版本**
<br>

> 这篇文章展现了基于c的网络编程，代码作为理解设计思想的必要，更重要的是体现网络中的设计哲学,我更建议你的是手敲代码，根据这份readme所提供的信息来尝试，为了更清晰的展现网络结构，所有的代码就只是对应功能的简单实现。相信你在这段过程中，可以去体悟到网络设计的底层实现。现在我主要进行的是c的tcp和udp的简单实现，后续会加入对数据报文，ip协议栈，和对go的底层设计的接口分析。尽管并不是自顶向下的构成分析，但是从对一个网络初学者而言，这是不错的起点

![Language](https://img.shields.io/badge/language-C-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20WSL-green.svg)
![Editor](https://img.shields.io/badge/Editor-VS%20Code-orange.svg)

</div>

---

## 目录结构 (Contents)

本项目按照学习路线分为以下几个模块：

### [Part 1: Webcoding based on C](#webcoding-based-on-c)
- [01 Basic (基础概念)](#01-basic-基础概念)
- [02 UDP Socket (UDP 通信)](#02-udp-socket-udp-通信)
- [03 TFTP Implementation (TFTP 协议实现)](#03-tftp-implementation-tftp-协议实现)
- [04 Broadcast & Multicast (广播与多播)](#04-broadcast--multicast-广播与多播)
- [05 TCP Socket (TCP 通信)](#05-tcp-socket-tcp-通信)

---

## 详细介绍 (Introduction)

## **1.Webcoding based on C**

### 01 Basic (基础概念)
网络通信的基石，主要解决不同层次上的数据表示差异。

* **01_endian (字节序)**
    * 展示了计算机 **小端存储 (Little-Endian)** 与 **大端存储 (Big-Endian)** 的区别。
    * **为什么会有这种存储的差异性**：这纯粹是CPU架构的历史遗留问题（比如 Intel x86 选了小端，而早期的 Motorola 选了大端）。但为了防止乱套，网络协议强行规定了必须用**大端**作为网络字节序。所以我们在发包前必须老老实实把主机的小端序转过去。
  
* **02_htol_htons (字节序转换)**
    * 基于 `<arpa/inet.h>` 头文件。
      ```c
      extern uint16_t htons (uint16_t __hostshort)
      __THROW __attribute__ ((__const__));
      ```
    * 实现 **主机字节序 (Host)** 向 **网络字节序 (Network)** 的转换 (如 `htonl`, `htons`)。
    * **解释一下 `__THROW` 和 `__attribute__`**：这其实是写给编译器看的tip。`__THROW` 告诉编译器这函数绝不抛出异常，`__const__` 告诉编译器这函数是“纯函数”（只依赖输入，没副作用）。这样编译器就能大胆地做优化，把多余的调用给省掉。

* **03_inet_pton (IP地址转换)**
    * 全称 *Presentation to Numeric*。
    * 将点分十进制字符串 (如 "192.168.1.1") 转换为网络传输用的 32位无符号整数。
      ```c
      int inet_pton (int __af, const char *__restrict __cp,
      void *__restrict __buf) __THROW;
      ```
    * **为什么需要一个 void 类型**：这里设计得很巧妙，因为 IPv4 用 `struct in_addr` (4字节)，IPv6 用 `struct in6_addr` (16字节)。用 `void*` 就能像万能插头一样，不管你是哪种协议，都能把转换后的二进制数据填进去。

* **04_inet_ntop (IP地址还原)**
    * 全称 *Numeric to Presentation*。
    * 将 32位网络字节序整数还原为人类可读的 IP 字符串。
      ```c
      extern const char *inet_ntop (int __af, const void *__restrict __cp,
      char *__restrict __buf, socklen_t __len)
      __THROW;
      ```
    * `extern` 意味着这是个外部引用，`__len` 则是为了防止缓冲区溢出（C语言老生常谈的内存安全问题），这一部分被认为是add部分单独开一行。

### 02 UDP Socket (UDP 通信)
无连接的、不可靠的数据传输协议。
* **01_socket(套接字)**
    * 展示了socket套接字创建的函数
      ```c
      int socket (int __domain, int __type, int __protocol)
      ```
    * domain决定IP类型，type则是决定tcp还是udp的传输类型。protocol是具体的协议格式
    * socket是一个int类型，靠文件描述符的抽象在系统层面上实现调用，展现了linux中一切皆文件的设计思想
    * 作为文件描述符，一定要在程序最后对其进行关闭，close在通信过程中就意味着断开连接，在tcp中，这一点变得更为复杂
* **02_sendto**
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


* **03_bind**
  * bind这个函数主要是为了固定ip和端口号，根据这个函数的面向我们可以很容易理解信息的接收方更加需要这个需求。



    ```c
    int bind (int __fd, __CONST_SOCKADDR_ARG __addr, socklen_t __len)
        __THROW;
    ```

  * 在tcp/udp编程时我们一般简单的就把server称为需要绑定的一方，但这实际上是由于信息接受和发送的相对关系所决定的，在多播和组播中我们能够看到这一点的进一步体现
* **04_recvfrom**
  * recvform是udp接受函数
    ```c
    recvfrom (int __fd, void *__restrict __buf, size_t __n, int __flags,
        __SOCKADDR_ARG __addr, socklen_t *__restrict __addr_len)
    ```


  * 这里需要注意的是recvfrom是接受别的主机的数据，所以我们需要预先创建空的结构体供其填入，并且还改变addrlen来作为接受到的信息长度输出，这种设计使得udp可以很简单的实现多线程工作，代价就是每一个client都需要相应的结构体来对应，而我们将会在后续看到，因为tcp要求的三次握手，导致tcp的设计走向了截然不同的道路


* **05_server&&06_client** 
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



### 03 TFTP Implementation (TFTP 协议实现)

*Trivial File Transfer Protocol* (简单文件传输协议)。

* **begin**
  * 在开始之前我想简单的介绍一下理解tftp的核心所在，作为基于udp协议的小文件传输协议，在c中实现tftp最让人恼火也最为关键的就是**手动构建和分析二进制报文**，你需要去拼凑每一个字节。


* **01_tftp_client**
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


* **02_tftp_server**
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




### 04 Broadcast & Multicast (广播与多播)
  * **background** * 在这里我们首先要去介绍一个函数 `setsockopt`。

      ```c
      extern int setsockopt (int __fd, int   __level, int __optname,
      const void *__optval, socklen_t __optlen) __THROW;
      ```

    * 这个函数的作用是在文件描述符的基础上对其做进一步的限制说明。
    * **参数详解**：
    * `__fd`：socket 的文件描述符。
    * `__level`：选项定义的层次。通常设为 `SOL_SOCKET` (通用套接字选项) 或 `IPPROTO_IP` (IP层选项)。
    * `__optname`：具体要设置的选项名。例如 `SO_BROADCAST` (允许广播)、`SO_REUSEADDR` (端口复用)。
    * `__optval`：指向存放选项值的缓冲区的指针。通常是一个 `int` 类型的指针，`1` 表示开启，`0` 表示关闭。
    * `__optlen`：`optval` 缓冲区的长度。


    * 在server中，我们也可以将其设置为非端口复用模式来方便调试，但为了代码的简便性，我在代码实例中并没有添加这部分内容。
    * **端口复用代码实例**：
      ```c
      int opt = 1;
      // 允许重用本地地址和端口，解决 "Address already in use" 错误
      setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
      ```
* **01_broadcast_send** 
  * 这个文件展现的是 broadcast 的发送方。和 tcp，udp 编程不同，在广播和多播里并没有传统意义上的 cs 框架，而是信息发送和接受的相对关系。
  * 这里采用 sendto 函数，除了需要额外对 socket 做功能添加外，基本上和 udp 的 client 思路一致。


* **02_broadcast_recv.c**
  * 这个文件的结构甚至比 udp_server 的结构还更加简单，因为这里广播地址是确定的，只需要监听是否有对应的数据包即可。
  * 这里有意思的是 recv 里面并不需要设置对应权限，这也和广播的设计思路相一致，广播的发送方需要额外的检验，而接收方只需要判断这个数据包是不是找自己的。


* **summary**
  * 广播的实现是基于 udp 完成的，因为广播本身就是一个一对多的单向过程，在实际网络过程中常常伴随着多次广播，所以说在这里数据的快速发送的重要性要远大于数据的稳定传输。
  * **广播的设计哲学**：广播类似于“大喇叭喊话”。因为这种行为会占用整个子网的带宽，可能造成扰民（网络风暴），所以内核设计上要求**发送者**必须显式调用 `setsockopt(SO_BROADCAST)` 来申请权限（打开开关）。而接收者是被动的，不需要特殊权限就能听到。


* **adding (IP Class Knowledge)**
  * 理解多播需要先学习 IP 分类知识：
  * **A/B/C 类**：用于单播 (Unicast)，即一对一通信。
  * **D 类 (224.0.0.0 ~ 239.255.255.255)**：**专用于多播 (Multicast)**。这部分 IP 不属于任何一台具体的主机，而是代表一个“组”。向这个 IP 发送数据，所有加入了这个组的主机都能收到。
  * **E 类**：保留科研用。




* **03_groupcast_send.c**
  * 在这里组播的发送方甚至连 `setsockopt` 都不用使用，这是因为本身有 D 类 IP 段被划分成专用于组播。所以 send 只需要向这些 ip 段里面发送数据，当它进行发送的时候，实际上就已经在对应 ip 设置了对应的广播组。


* **adding**
  * **INADDR_ANY 是什么**：在代码中常常见到 `server_addr.sin_addr.s_addr = htonl(INADDR_ANY);`。它的数值其实是 `0.0.0.0`。它的意思是“绑定到本地所有可用的网络接口”。如果你既有 Wifi 又有网线，使用 `INADDR_ANY` 可以让你从两个网卡都能接收到数据，而不需要把程序绑定死在某一个具体的 IP 上。


* **04_groupcast_recv.c**
  * recv 中需要使用 `setsockopt` 进行设置。在之前所说，setsockopt 中的 `_optval` 是 `void*` 类型，这也意味着我们可以构造结构体进行数据传参，这也是我们在 c 中常用的方法。而这里我们需要采用的是专门为了多播组设置的结构体 `ip_mreq` 进行参数设置：


    ```c
    struct ip_mreq
    {
      /* IP multicast address of group.  */
      struct in_addr imr_multiaddr; // 多播组的IP (比如 224.0.0.88)

      /* Local IP address of interface.  */
      struct in_addr imr_interface; // 自己加入该组的接口IP (通常用 INADDR_ANY)
    };

    ```


  * 这里 `imr_interface` 是本地接口，`imr_multiaddr` 是组播 IP，其下都有 `s_addr` 成员，和 `sockaddr_in` 的设计一样，都是因为历史原因导致。


  * **summary (Broadcast vs Multicast Philosophy)**
  * 这里和广播需要做出明确划分，这也体现了两者底层逻辑的截然相反：
  * **广播 (Broadcast)**：是**发送方**需要 `setsockopt`。因为广播是暴力的，默认禁止，发送者必须主动申请“我要喊话”的权限。
  * **多播 (Multicast)**：是**接收方**需要 `setsockopt` (加入组 `IP_ADD_MEMBERSHIP`)。因为多播是精准的，发送方只是往一个 D 类 IP 发数据（谁都可以发），关键在于接收方必须显式地声明“我订阅了这个频道”，内核才会把对应的数据包捞上来给你。



### 05 TCP Socket (TCP 通信)

* **background**
  * 尽管在这里 tcp 和 udp 的最大区别是 tcp 有了三次握手四次挥手来保证数据传输，但我们在调用函数进行编程时，这些复杂的状态流转大多已被内核封装。换句话说，我们在这里更多是从**应用层**的角度去考虑 socket 的生命周期管理。
  * **设计哲学的转变**：UDP 是无状态的，一个 socket 可以给任意 IP 发包；但 TCP 是面向连接的，就像打电话，必须先接通才能说话。这种设计要求服务端必须维持一个“监听 socket”专门用来接客，每来一个客人（客户端），就得新建一个“服务 socket”专门负责聊天。
  * **并发的核心矛盾**：如何高效地管理这些成百上千的“服务 socket”？这就派生出了两条技术路线：
    1. **多进程/多线程**：通过增加人手（CPU调度单元）来解决，一个连接对应一个线程/进程。
    2. **IO 多路复用 (Non-blocking)**：通过非阻塞 IO + 事件轮询（如 epoll），让一个服务员（单线程）就能看管所有桌子。




* **01_client**
  * 这里展现的是 tcp 的客户端，在创建好 socket 和封装好 server 结构体后，我们首先要调用封装好的函数去建立底层连接。


    ```c
    extern int connect (int __fd, __CONST_SOCKADDR_ARG __addr, socklen_t __len);
    ```

  *  **Connect 的底层机制 (三次握手触发器)**：
     1.  当调用 `connect` 时，内核会向 Server 发送一个 **SYN** 包。
     2. 此时函数处于阻塞状态，等待 Server 回复 **SYN+ACK**。
     3. 收到回复后，Client 再发送一个 **ACK**，此时连接建立 (ESTABLISHED)，函数返回 0。


   * 在 client 这一方通常只需要维护一个 socket，建立连接后，内核已经把这个 socket 绑定到了特定的远端 IP 和端口，所以 `send` 函数不需要像 `sendto` 那样重复指定目标地址。

      ```c
      extern ssize_t send (int __fd, const void *__buf, size_t __n, int __flags);
      ```


* **adding (Buffer Trap)**
  * **strlen vs sizeof 的大坑**：发送字符串时，**千万不要用 `sizeof(buf)`，要用 `strlen(buf)`。
  
  * **原因**：`sizeof` 计算的是数组申请的总内存（比如 1024），而 `strlen` 计算的是实际字符长度（比如 "hello" 是 5）。如果你用 `sizeof`，你会把缓冲区里后面几百个没用的乱码（垃圾数据）也发给对方，这在处理协议时是灾难性的。




* **02_server.c**
  * 这里是 tcp 服务器的实例。在创建好 socket 和填充绑定好结构体后，首先要将 socket 设置为监听状态。
     ```c
    extern int listen (int __fd, int __n) __THROW;
    ```

  * `__fd`: 之前创建的套接字文件描述符。
  * `__n`: **Backlog (积压队列长度)**
  * **为什么需要 Listen**：
  * 内核为监听套接字维护了两个队列：**半连接队列** (收到 SYN 但没收到最终 ACK) 和 **全连接队列** (三次握手完成等待 Accept 取走)。
  * `__n` 实际上决定了这些队列（通常是全连接队列）的大小。如果队列满了，新的连接请求就会被直接丢弃或拒绝（SYN Flood 攻击也是针对这里）。


  * 设置好监听状态后，通过 `accept` 从全连接队列中取出一个已完成的连接。


    ```c
    extern int accept (int __fd, __SOCKADDR_ARG __addr,
    socklen_t *__restrict __addr_len);

    ```


  * **两个 FD 的故事**：
  * `accept` 返回的 `int` 是一个**全新的文件描述符** (Connected Socket)。
  * **设计哲学**：原来的 `sockfd` 只负责把人领进门；`accept` 返回的 `fd` 专门负责这一桌的通信。这种分离设计使得 TCP Server 可以同时处理握手请求和数据传输。


  * **Recv 的返回值判断**：


    ```c
    extern ssize_t recv (int __fd, void *__buf, size_t __n, int __flags);

    ```


  * `> 0`: 接收到的字节数。
  * `= 0`: **重要！** 这代表对端关闭了连接 (FIN 包)。TCP 是全双工的，0 字节读意味着 Read 通道关闭。
  * `< 0`: 出错 (Error)，需要检查 errno。
  * 而在dup中可以直接发送长度为0的数据包


  * **summary (CS Framework)**
* **TCP C/S 交互流程图**：


    ```text
        [Server]                  [Client]
      socket()                  socket()
          |                         |
        bind()                      |
          |                         |
      listen()                     |
          |                         |
      accept() <---(3-Way)---> connect()
      (Block...)   Handshake        |
          |                         |
        recv() <----(Data)-----   send()
          |                         |
        send()  ----(Data)---->   recv()
          |                         |
      close() <----(4-Way)--->  close()
                    Wavehand

    ```


* **03_server_fork.c**
  * 这里是通过多进程的方式来实现并发。


    ```c
    extern __pid_t fork (void) __THROWNL;
    ```


  * **Fork 的魔法**：调用一次，返回两次。
  * 返回 `> 0` (子进程 PID)：当前是父进程，任务是继续 `accept` 等待新人。
  * 返回 `0`：当前是子进程，继承了父进程的所有资源（包括 socket），任务是处理刚刚那个连接的 `send/recv`。
  * **COW (Copy On Write)**：Linux 这里的效率很高，并不会真的立马把父进程所有内存复制一份，只有当子进程尝试修改数据时，才会真正复制内存页。


  * **僵尸进程与信号回收**：
  * 子进程结束时如果父进程不管，它会变成“僵尸进程”占用 PID 资源。
  * 我们利用 `signal` 机制来异步回收。




    ```c
    // 注册信号处理函数
    signal(SIGCHLD, handler);

    void handler(int sig){
      // 循环回收所有已结束的子进程
      while((waitpid(-1, NULL, WNOHANG)) > 0){}
    }

    ```


  * **Waitpid 参数详解**：
  * `-1`: 等待任意子进程。
  * `NULL`: 不关心子进程具体的退出状态码 (exit code)。
  * `WNOHANG`: **非阻塞关键**。如果当前没有子进程结束，立刻返回 0，不要卡在这里傻等。这保证了 Server 不会因为回收垃圾而停止响应新请求。




* **04_server_thread.c**
  * 使用多线程处理。进程是资源分配的单位（重），线程是 CPU 调度的单位（轻）。


    ```c
    extern int pthread_create (pthread_t *__restrict __newthread,
          const pthread_attr_t *__restrict __attr,
          void *(*__start_routine) (void *),
          void *__restrict __arg) __THROWNL __nonnull ((1, 3));

    ```


  * **参数详解**：
  * `__newthread`: 指向线程 ID 的指针，用于接收新线程 ID。
  * `__attr`: 线程属性，通常传 `NULL` 使用默认值。
  * `__start_routine`: 线程启动后要执行的函数指针。
  * `__arg`: 传给启动函数的唯一参数。由于只能传一个，所以通常需要把 socket、IP 等信息打包成结构体，转为 `void*` 传入。


  * **编译指令**：


    ```bash
    gcc server_thread.c -o server -lpthread
    ```


  * **自动垃圾回收 (Detach)**：


    ```c
    pthread_detach(pthread_self());
    ```


  * **原理**：默认情况下线程是 `joinable` 的，退出后需要主线程调用 `pthread_join` 来“收尸”。调用 `detach` 是告诉内核：“这个线程也是个普通打工人，死了直接埋了就行”，内核会在线程退出时自动释放其栈空间和资源，无需主线程操心。


* **05_server_noblock.c**
  * 在这个文件里面我们尝试将 socket 设置为非阻塞 (Non-blocking)。这是迈向高性能 IO (Epoll/IOCP) 的第一步。


    ```c
    // 获取当前 flag
    int flag = fcntl(sockfd, F_GETFL, 0);
    // 设置新 flag = 旧 flag + 非阻塞位
    fcntl(sockfd, F_SETFL, flag | O_NONBLOCK, 0);

    ```

  * **位运算图解**：
  * `fcntl` 通过位掩码来管理状态。
  * `flag` (假设): `0000 0010` (代表已有的属性)
  * `O_NONBLOCK`: `0000 0100` (非阻塞属性)
  * `|` (OR) 操作: `0000 0110` (同时拥有两种属性)


  * **非阻塞的代价 (Errno)**：
  * 当 socket 非阻塞时，如果 `recv` 缓冲区里没数据，它不会卡住，而是立刻返回 `-1`。
  * 此时必须检查 `errno`。如果 `errno == EAGAIN` (Try again) 或 `EWOULDBLOCK`，说明**“现在没数据，不是出错了，待会再来”**。这使得程序可以在没数据时去干别的事。


* **06_server_epoll.c**
  * **Epoll**: Linux 下最高效的 IO 多路复用器。它解决了 `select/poll` 轮询所有 socket 效率低下的问题。


    ```c
    extern int epoll_create1 (int __flags) __THROW;
    ```


  * 创建一个 epoll 实例（红黑树根节点），返回句柄 `epfd`。


    ```c
    struct epoll_event {
        uint32_t events;  /* Epoll events */
        epoll_data_t data; /* User data variable */
    } __EPOLL_PACKED;
    ```


  * **核心参数**：
  * `events`: 感兴趣的事件。
  * `EPOLLIN`: 有数据可读 (包括新连接)。
  * `EPOLLET`: **边缘触发 (Edge Triggered)**。数据这就只有一次通知，没读完下次不提醒（高效但难写）。默认是 **LT (Level Triggered)**，没读完一直提醒。

  * `data`:data里面有多种数据结构，这里我们使用文件描述符
  * `data.fd`: 记录是哪个 socket 发生了事件。


    ```c
    extern int epoll_ctl (int __epfd, int __op, int __fd,
            struct epoll_event *__event) __THROW;
    ```


  * **操作类型 (`__op`)**:
  * `EPOLL_CTL_ADD`: 注册新的 socket。
  * `EPOLL_CTL_MOD`: 修改监听事件。
  * `EPOLL_CTL_DEL`: 移除 socket。




    ```c
    extern int epoll_wait (int __epfd, struct epoll_event *__events,
            int __maxevents, int __timeout)
    ```


  * **Event Loop 逻辑**：
  * `epoll_wait` 阻塞等待，一旦有 socket 就绪，它会将这就绪的 socket 填入 `__events` 数组并返回数量 `n`。
  * 我们只需要遍历这 `n` 个活跃的 socket，而不需要遍历所有 10000 个 socket。
  * **分流处理**：
  * 如果 `events[i].data.fd == listen_fd`: 说明有新连接 -> 调用 `accept` -> `epoll_ctl(ADD)` 加入监控。
  * 否则: 说明是已连接的客户端发数据了 -> 调用 `recv/send` 处理业务。






* **adding**
  * **总结**：Epoll 用单线程实现了高并发，避免了多线程频繁切换上下文的开销 (Context Switch)。但如果业务逻辑非常耗时（比如计算密集型），单线程会被卡死。
  * **Go 的伏笔**：Go 语言的 Goroutine 实际上就是将“多线程的易用性”和“Epoll 的高性能”结合了起来——底层用 Epoll 监听，上层用轻量级协程伪装成阻塞 IO，我们将在后续部分看到这种天才般的设计。

  ## **webcoding on go sdk**

  ### **the goal of this part**
  * 在上一节中，我们看到了c语言中netpoll的实现，在这一节，我们将去探讨go中netpoll的应用和巧妙设计，不过在此之前，我强烈建议你去观看[Core Dumped](https://www.youtube.com/watch?v=7ge7u5VUSbE)的系列视频，来补充基本的计算机知识。
  * 下面是通过这一小结我们需要
    ```go
      package main

      import (
        "fmt"
         "net"
       )

      func main() {

      listener, err := net.Listen("tcp", ":8080")
      if err != nil {
        panic(err)
      }
      defer listener.Close()
      fmt.Println("Server is running on :8080...")

      for {
        
        conn, err := listener.Accept()
        if err != nil {
          fmt.Println("Accept error:", err)
          continue
        }
        go handleConnection(conn)
      }
    }

    func handleConnection(conn net.Conn) {
      defer conn.Close()
      buf := make([]byte, 1024)

      for {
      
        n, err := conn.Read(buf)
        if err != nil {
          fmt.Println("Connection closed or read error")
          return
        }
        _, err = conn.Write(buf[:n])
        if err != nil {
          fmt.Println("Write error:", err)
          return
        }
      }
    }

      ```
