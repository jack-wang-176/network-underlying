# 04 Broadcast & Multicast (广播与多播)
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
* **[01_broadcast_send](../../../01_socket_underlying_c//04_broadcast&&groupcast/01_broadcast_send.c)** 
  * 这个文件展现的是 broadcast 的发送方。和 tcp，udp 编程不同，在广播和多播里并没有传统意义上的 cs 框架，而是信息发送和接受的相对关系。
  * 这里采用 sendto 函数，除了需要额外对 socket 做功能添加外，基本上和 udp 的 client 思路一致。


* **[02_broadcast_recv](../../../01_socket_underlying_c//04_broadcast&&groupcast/02_broadcast_recv.c)**
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




* **[03_groupcast_send.c](../../../01_socket_underlying_c//04_broadcast&&groupcast/03_groupcast_send.c)**
  * 在这里组播的发送方甚至连 `setsockopt` 都不用使用，这是因为本身有 D 类 IP 段被划分成专用于组播。所以 send 只需要向这些 ip 段里面发送数据，当它进行发送的时候，实际上就已经在对应 ip 设置了对应的广播组。


* **adding**
  * **INADDR_ANY 是什么**：在代码中常常见到 `server_addr.sin_addr.s_addr = htonl(INADDR_ANY);`。它的数值其实是 `0.0.0.0`。它的意思是“绑定到本地所有可用的网络接口”。如果你既有 Wifi 又有网线，使用 `INADDR_ANY` 可以让你从两个网卡都能接收到数据，而不需要把程序绑定死在某一个具体的 IP 上。


* **[04_groupcast_recv.c](../../../01_socket_underlying_c//04_broadcast&&groupcast/04_groupcast_recv.c)**
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

