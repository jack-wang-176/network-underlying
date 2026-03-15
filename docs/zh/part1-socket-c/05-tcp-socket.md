# 05 TCP Socket (TCP 通信)

* **background**
  * 尽管在这里 tcp 和 udp 的最大区别是 tcp 有了三次握手四次挥手来保证数据传输，但我们在调用函数进行编程时，这些复杂的状态流转大多已被内核封装。换句话说，我们在这里更多是从**应用层**的角度去考虑 socket 的生命周期管理。
  * **设计哲学的转变**：UDP 是无状态的，一个 socket 可以给任意 IP 发包；但 TCP 是面向连接的，就像打电话，必须先接通才能说话。这种设计要求服务端必须维持一个“监听 socket”专门用来接客，每来一个客人（客户端），就得新建一个“服务 socket”专门负责聊天。
  * **并发的核心矛盾**：如何高效地管理这些成百上千的“服务 socket”？这就派生出了两条技术路线：
    1. **多进程/多线程**：通过增加人手（CPU调度单元）来解决，一个连接对应一个线程/进程。
    2. **IO 多路复用 (Non-blocking)**：通过非阻塞 IO + 事件轮询（如 epoll），让一个服务员（单线程）就能看管所有桌子。




* **[01_client](../../../01_socket_underlying_c//05_tcp/01_client.c)**
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




* **[02_server](../../../01_socket_underlying_c//05_tcp/02_server.c)**
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


* **[03_server_fork](../../../01_socket_underlying_c//05_tcp/03_server_fork.c)**
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




* **[04_server_thread](../../../01_socket_underlying_c//05_tcp/04_server_thread.c)**
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


* **[05_server_noblock](../../../01_socket_underlying_c//05_tcp/05_server_noblock.c)**
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


* **[06_server_epoll](../../../01_socket_underlying_c//05_tcp/06_server_epoll.c)**
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