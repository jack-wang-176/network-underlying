# **listen 函数的内部调用**

## 1. 创建 Socket

* 如果我们沿着 `listen` 函数一路深入，会发现核心入口是 [socket](../../../02_go_sdk/go/src/net/sock_posix.go#L18) 函数。这个函数是 Go 底层创建 socket 的实例，和我们之前在 C 语言中调用的 `socket()` 系统调用一样，它也会返回一个相应的文件描述符（File Descriptor）。

  ```go
  // 核心逻辑简述
  s, err := sysSocket(family, sotype, proto)
  // ......
  err = setDefaultSockopts(s, family, sotype, ipv6only)
  // ......
  // 根据 socket 类型分发逻辑
  switch sotype {
  case syscall.SOCK_STREAM, syscall.SOCK_SEQPACKET:
      // 如果是流式套接字（TCP），进入 listenStream
      if err := fd.listenStream(ctx, laddr, listenerBacklog(), ctrlCtxFn); 
  // ......
  }

  ```

* **底层交互与自举**：在这里我抽出了 `sysSocket` 这一核心逻辑。它调用的是底层封装的汇编命令。值得一提的是，Go 区别于 PHP 等解释型语言的一个显著特征在于 **Go 是自举（Self-hosted）的**——Go 语言本身也是由 Go 编写的。这意味着 Go 拥有自己的汇编代码和对应的 `cmd` 编译目录。当我们追溯 Go 底层时，可以清晰地看到 Go 代码与汇编代码的交界处。
* **参数配置**：宏观上看，这个函数相当于我们在 C 中配合使用的 `socket()` 和 `setsockopt()`。`sysSocket` 负责创建，而 `setDefaultSockopts` 负责设置基础属性。
* **角色定型**：完全体的 Socket（是作为客户端还是服务器？是流式传输还是数据包？）最终通过上层传参确定。在本例中，区别于客户端的 `Dial`，`Listen` 操作通过绑定本地端口（如代码中的 `8080`），明确了当前机器作为**服务端**的角色。

## 2. 流式套接字的构建：`listenStream`

* 在函数内部，创建流式套接字（TCP）和数据包套接字（UDP）被封装成了不同的路径。以我们关注的流式套接字为例，看看 [listenStream](../../../02_go_sdk/go/src/net/sock_posix.go#L150) 做了什么：

  ```go
  // 1. 设置监听器的默认 Socket 选项
  setDefaultListenerSockopts(fd.pfd.Sysfd)
  // ...
  // 2. 将地址转化为系统识别的 sockaddr 结构体
  // ...
  // 3. 应用用户自定义的 Socket 属性
  // ...
  // 4. 绑定端口 (对应 C 中的 bind)
  syscall.Bind(fd.pfd.Sysfd, lsa)
  // 5. 开始监听 (对应 C 中的 listen)
  listenFunc(fd.pfd.Sysfd, backlog)
  // 6. 初始化文件描述符（关键步骤！）
  fd.init()

  ```

* **C 语言的影子**：数据包套接字的创建逻辑类似，只是多了对多播地址的判断。可以明显看到，Go 的这一套流程完美复刻了我们在 C 代码中执行的 `socket` -> `bind` -> `listen` 标准三部曲。这揭示了 Go 的网络底层依然是基于标准 OS 套接字机制的封装。

## 3. 进入 Netpoll：`fd.init`

* 之前的步骤和我们在 C 中实现的逻辑别无二致，但从 [fd.init](../../../02_go_sdk/go/src/internal/poll/fd_unix.go#L55) 开始，我们将进入 Go 独有的魔法领域——**Netpoll（网络轮询器）**。

* 这个函数的主要功能是判断当前文件描述符是否属于网络文件（即非普通文件），如果是，则为它初始化网络轮询机制：

  ```go
  // 初始化 pollDesc (poll descriptor)
  fd.pd.init(fd)
  ```

## 4. 运行时与网络的交汇：`pd.init`

* [init](../../../02_go_sdk/go/src/internal/poll/fd_poll_runtime.go#L38) 函数是 Go `net` 标准库与 `runtime` 运行时包的关键交汇点，也是“同步代码、异步执行”的基石。

  ```go
  func (pd *pollDesc) init(fd *FD) error {
      // 1. 保证全局网络轮询器只被初始化一次
      serverInit.Do(runtime_pollServerInit)
      
      // 2. 将文件描述符注册到轮询器中 (底层对应 epoll_ctl/kqueue 等)
      ctx, errno := runtime_pollOpen(uintptr(fd.Sysfd))
      if errno != 0 {
          return errnoErr(syscall.Errno(errno))
      }
      
      // 3. 保存上下文
      pd.runtimeCtx = ctx
      return nil
  }
  ```

* **`serverInit.Do`**：利用 `sync.Once` 机制，确保在整个程序生命周期中，全局的网络轮询器（Poller）只会被初始化一次。
* **`runtime_pollOpen`**：这是最关键的一步。它将我们之前创建的 Socket 文件描述符（fd）注册到底层的 IO 多路复用器中（在 Linux 下即调用 `epoll_ctl` 添加 `EPOLLIN` 等事件）。如果注册失败，通常意味着系统资源耗尽或环境异常。

---

**小结**：
在这一部分，我们验证了 Go 底层的 Socket 封装在前半程与我们 [C 语言实现 (socket-underlying-c)](https://github.com/jack-wang-176/socket-underlying-c) 的逻辑高度一致。
然而，分水岭出现在 `fd.init` 之后——Go 并没有止步于创建 Socket，而是通过 `runtime` 将其无缝接入了 Netpoll 体系。在接下来的部分中，我们将进一步探索这个围绕 Netpoll 建立起来的庞大网络处理体系，以及它是如何通过调度器与 Go 的多协程（Goroutine）完美结合的。
