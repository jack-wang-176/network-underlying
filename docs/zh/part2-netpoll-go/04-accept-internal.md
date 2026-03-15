# Accept 的底层实现


在上两节中，我们深入探讨了 `listen` 的底层系统调用实现。在正式进入 `accept` 之前，我们先自下而上地回顾一下这个函数的返回值封装，理清 Go 是如何将底层资源暴露给用户层的。

首先，对接到我们 C 语言视角的部分是 `socket` 文件描述符。这个原始的文件描述符经由上层 `listenTCPProto` 封装，最终成为了 [TCPlistener](../../../02_go_sdk/go/src/net/tcpsock.go#L291) 结构体。这个结构体不仅仅是文件描述符的容器，它还内嵌了 [listenConfig](../../../02_go_sdk/go/src/net/dial.go#L672) 结构体。`listenConfig` 包含了 `control` 钩子函数、KeepAlive 探测周期等配置，它的意义在于**将操作系统底层的网络参数设置向用户层敞开**，允许开发者在 Go 语言层面通过配置字段来对底层的 `socket` 行为做进一步的限制和明确。

最终，通过结构体定义和对 `tcplistener` 的方法封装，Go 将底层的 `listen` 系统调用和上层的配置结构体紧密“偶联”在一起。这种关系在我们调用 `net.Listen` 时，通过传入的 network 和 address 字段就已经决定好了。

值得注意的是，`listen` 这个接口主要用于处理流式和面向连接的协议（如 `tcp`, `ssl` 等）；与其平行的功能接口还有 `PacketConn`，用于处理数据包协议（如 `udp`, `dns` 等）。这种设计完美体现了 Go 语言**面向接口编程**的思想，将不同协议的实现细节隐藏在统一的接口之下，实现了代码的模块化与组件化。


现在我们理解了 `Listener` 的构建过程。我们在业务代码中调用的 `Accept` 方法，本质上是通过接口与 `func (ln *TCPListener) accept()` 关联起来的。接下来我们将从 [accept](../../../02_go_sdk/go/src/net/tcpsock_posix.go#L158) 的上层入口出发，自上而下地剖析其底层实现原理。

## 1. 网络轮询器的入口：fd_unix

* 首先，代码逻辑会来到 `internal/poll` 包下的 [accept](../../../02_go_sdk/go/src/net/fd_unix.go#L171) 函数。这个函数是网络轮询器（Netpoller）与 socket 交互的关键枢纽，主要完成了以下四个核心功能：

  1. **调用系统调用**：封装并执行底层的 `accept` 系统调用。
  2. **对象包装**：将返回的新文件描述符包装成 `netFD` (Network File Descriptor) 对象。
  3. **注册轮询**：将新的 `netFD` 注册到 epoll（或对应平台的 IO 多路复用器）中，以便监听后续的读写事件。
  4. **完善信息**：填充对端（Remote）和本地（Local）的地址信息。

* 我们重点关注 `internal/poll/fd_unix.go` 中的核心逻辑 [accept](../../../02_go_sdk/go/src/internal/poll/fd_unix.go#L594)：

  ```go
  func (fd *FD) Accept() (int, syscall.Sockaddr, string, error) {
      // ... 准备工作与加锁 ...
      
      // 循环尝试 Accept
      for {
          // 执行底层系统调用 accept
          s, rsa, errcall, err := accept(fd.Sysfd)
          
          // 1. 如果成功，直接返回
          if err == nil {
              return s, rsa, "", err
          }
          
          // 2. 处理系统调用返回的错误
          switch err {
          case syscall.EINTR:
              // 信号中断，重试
              continue
          case syscall.EAGAIN:
              // 核心逻辑：EAGAIN 表示当前 socket 接收缓冲区为空（无新连接）
              // 如果 fd 是可轮询的 (pollable)，则挂起当前 Goroutine 等待读事件
              if fd.pd.pollable() {
                  if err = fd.pd.waitRead(fd.isFile); err == nil {
                      continue // 被唤醒后，继续循环尝试 accept
                  }
              }
          case syscall.ECONNABORTED:
              // 这种错误通常意味着连接在建立过程中被对端复位，忽略并重试
              continue
          }
          return -1, nil, errcall, err
      }
  }
  ```

* 这里的非阻塞 I/O 思路与我们在 C 语言中 [netpoll](./01_socket_underlying_c/05_tcp/06_server_epoll.c) 实现一节的思路完全一致：当 `accept` 返回 `EAGAIN` 时，不让线程睡眠，而是让出 CPU。
* **关键点**：Go 的强大之处在于，当遇到 `EAGAIN` 时，它调用 `fd.pd.waitRead` 将当前 **Goroutine** 挂起，而不是阻塞操作系统线程。

## 2. 包装核心：newFD

* 在系统调用 `accept` 成功返回拿到文件描述符后，Go 并没有直接使用这个裸露的 int 类型句柄，而是通过 [newFD](../../../02_go_sdk/go/src/net/fd_unix.go#L26) 函数将其包装成了一个功能丰富的对象。

* 这个函数不仅仅是简单的内存分配，它确定了 Go 运行时如何看待这个网络连接：

  * **poll.FD**：这是核心中的核心，它充当了 Go 语言 IO 层（用户代码）和 Runtime 层（调度器）之间的桥梁。
  * **Sysfd**：保存了底层的 socket 句柄，一切操作最终都落实在这个整数上。
  * **IsStream**：标记是否为流式套接字。
  * 如果是 TCP，此值为 `true`。
  * 如果是 UDP，此值为 `false`。


  * **ZeroReadIsEOF**：这是结束判定的关键规则。
  * 正如我们在 [udp](./01_socket_underlying_c/02_udp/04_recvfrom.c) 章节中提到的，UDP 允许发送 0 字节的数据包，这在 UDP 中不代表连接结束。
* 而在 TCP 中，`read` 返回 0 字节通常意味着对端发送了 FIN 包（EOF），连接需要关闭。
* `newFD` 会根据 `IsStream` 的值自动设置这个字段，确保上层业务逻辑能正确处理“读到 0 字节”的含义。


* **并发安全**：`newFD` 初始化的对象内部维护了读写锁（`fdMutex`），这是 Go 能够让多个 Goroutine 安全地对同一个 socket 进行并发操作（尽管通常不建议这样做）的底层保障。

## 3. 注册轮询：init

* 包装完成后，紧接着就是“激活”这个连接，这一步由 [init](../../../02_go_sdk/go/src/internal/poll/fd_unix.go#L55) 完成。

* 这个函数的逻辑与我们之前在 [listen](./02-listen-internal.md) 中看到的思路完全一致，可谓是**殊途同归**：

* **Listen** 阶段将监听 socket (`listener`) 注册到 epoll 中，为了监听“新连接到来”的事件。
* **Accept** 阶段将新建立的连接 socket (`conn`) 注册到 epoll 中，为了监听“数据可读/可写”的事件。

* 底层最终都调用了 `poll.runtime_pollOpen`，将 `Sysfd` 添加到 epoll 实例的红黑树中。一旦这一步完成，这个连接就正式进入了 Go 的网络轮询器（Netpoller）的管理范围，为后续的异步 I/O 奠定了基础。

## 4. 深入 Runtime：poll_runtime_pollWait

* 进一步深入 `waitRead` 函数，我们最终通过 `//go:linkname` 链接机制，从 `internal/poll` 包跨越到了 `runtime` 包的 [poll_runtime_pollWait](../../../02_go_sdk/go/src/runtime/netpoll.go#L336)。

* 注意，此时我们的业务结构体已经从 `poll.FD` 转化为了 runtime 内部的 `polldesc` (poll descriptor)，这种转化依赖于结构体的映射关系。

  ```go
  func poll_runtime_pollWait(pd *pollDesc, mode int) int {
      // 1. 检查连接是否已经出错或关闭
      errcode := netpollcheckerr(pd, int32(mode))
      if errcode != pollNoError {
          return errcode
      }
      
      // 2. 循环等待，直到 netpollblock 返回 true (表示 IO 就绪)
      for !netpollblock(pd, int32(mode), false) {
          // 被唤醒后再次检查错误
          errcode = netpollcheckerr(pd, int32(mode))
          if errcode != pollNoError {
              return errcode
          }
          // 如果是因为超时被唤醒，但还没来得及运行超时就被重置了，
          // 则假装没发生，继续重试。
      }
      return pollNoError
  }

  ```

* 在真正挂起之前，runtime 先通过 [netpollcheckerr](../../../02_go_sdk/go/src/runtime/netpoll.go#L512) 检查是否有超时或关闭错误。
* 这里的 `for` 循环逻辑是为了处理一些边缘情况（例如超时触发后又被重置），防止并发修改导致状态不一致，确保每一次挂起都是有效的。

## 5. 核心阻塞逻辑：netpollblock

* 接下来我们深入 [netpollbloack](../../../02_go_sdk/go/src/runtime/netpoll.go#L548)，这是 Goroutine 挂起的决策中心。

  ```go
  func netpollblock(pd *pollDesc, mode int32, waitio bool) bool {
      // 根据 mode 选择是操作读通道(rg) 还是 写通道(wg)
      gpp := &pd.rg
      if mode == 'w' {
          gpp = &pd.wg
      }

      for {
          // CAS 操作：原子性地判断状态
          
          // Case 1: 如果状态已经是 pdReady (IO 就绪)，则将状态重置为 pdNil 并返回 true
          // 表示不需要等待，直接去读/写数据
          if gpp.CompareAndSwap(pdReady, pdNil) {
              return true
          }
          
          // Case 2: 如果状态是 pdNil (初始状态)，则将其设置为 pdWait (等待中)
          // 只有设置成功，才能跳出循环去执行 gopark 挂起
          if gpp.CompareAndSwap(pdNil, pdWait) {
              break
          }
          
          // Case 3: 如果既不是 Ready 也不是 Nil，说明出现了并发等待，抛出异常
          if v := gpp.Load(); v != pdReady && v != pdNil {
              throw("runtime: double wait")
          }
      }
      
      // 执行挂起操作
      // 传入 netpollblockcommit 作为回调，它会在 g0 栈上执行
      if waitio || netpollcheckerr(pd, mode) == pollNoError {
          gopark(netpollblockcommit, unsafe.Pointer(gpp), waitReasonIOWait, traceBlockNet, 5)
      }
      
      // 被唤醒后的逻辑
      old := gpp.Swap(pdNil)
      if old > pdWait {
          throw("runtime: corrupted polldesc")
      }
      return old == pdReady
  }

  ```

* **状态机管理**：`pd.rg` 和 `wg` 是原子操作的 uintptr，它们在前面 [polldesc](./03-runtime-architecture.md#5-polldesc-的内存管理高效复用与-gc-隔离) 一节中提到，默认值为 `pdNil` (0)。
* **CAS (CompareAndSwap)**：这里利用 CAS 实现无锁状态流转。将“值检测”和“值交换”融为一体，确保了多线程下的安全性。
* **挂起与唤醒**：`gopark` 是分水岭。执行 `gopark` 前，当前 Goroutine 正在运行；`gopark` 返回后，说明 Goroutine 已经被 epoll 事件唤醒，此时检查返回值是否为 `pdReady`，确认是由数据包唤醒而非超时。

## 6. 调度器切换：gopark 与 g0 栈

* 最后，我们解析最底层的调度器接口 [gopark](../../../02_go_sdk/go/src/runtime/proc.go#L443)。这是 Goroutine 让出 CPU 控制权的关键。

  ```go
  func gopark(unlockf func(*g, unsafe.Pointer) bool, lock unsafe.Pointer, reason waitReason, traceEv byte, traceskip int) {
      // ...
      mp := acquirem() // 1. 禁止当前 M 被抢占
      gp := mp.curg
      // ... 状态检查 ...
      
      // 2. 保存当前 Goroutine 的现场 (SP, PC 等)
      // 3. 切换到 g0 栈执行 park_m 函数
      mcall(fastrand) 
      // 注意：mcall 会调用 park_m，代码逻辑跳转到 park_m
      // ...
  }

  ```

* **acquirem/release**：`acquirem` 本质是增加当前 M (系统线程) 的锁计数，防止在保存现场这种敏感操作期间，M 被调度器抢占或被 GC 扫描干扰。
* **mcall 与 g0**：Goroutine 的栈是动态伸缩的，而挂起操作涉及到栈的切换。为了安全，必须切换到 **g0 栈**（系统栈，大小固定且较大）来执行调度逻辑。
* **回调函数**：这里的 `unlockf` 就是我们在 `netpollblock` 中传入的 [netpollblockcommit](../../../02_go_sdk/go/src/runtime/netpoll.go#L529)。

* 在 g0 栈中，系统会执行 [park_m](../../../02_go_sdk/go/src/runtime/proc.go#L4007) 函数：

  ```go
  func park_m(gp *g) {
      // ...
      // 1. 修改 Goroutine 状态：从 _Grunning 变为 _Gwaiting
      casgstatus(gp, _Grunning, _Gwaiting)
      
      // 2. 解除 M 和 G 的绑定
      dropg()
      
      // 3. 执行回调函数 netpollblockcommit
      if fn := mp.waitunlockf; fn != nil {
          ok := fn(gp, mp.waitlock) // 在这里，gp 的地址被写入了 pollDesc 中
          // ...
      }
      
      // 4. M 寻找下一个可运行的 Goroutine
      schedule()
  }
  ```

* [casgstatus](../../../02_go_sdk/go/src/runtime/proc.go#L1105)：原子地将协程状态从运行中（Running）标记为等待中（Waiting）。
* [dropg](../../../02_go_sdk/go/src/runtime/proc.go#L4001)：彻底解绑 `mp.curg = nil` 和 `gp.m = nil`。此时 G 已经“睡”在堆上了，而 M 获得了自由。
* **关键回调**：执行 [netpollblockcommit](../../../02_go_sdk/go/src/runtime/netpoll.go#L529)，通过 `atomic.Store(gpp, gp)` 将当前 Goroutine 的内存地址填入 `pd.rg` 中。**这一步至关重要**，它相当于告诉 netpoller：“当这个 socket 有数据来时，请唤醒地址为 `gp` 的这个协程”。
* [schedule](../../../02_go_sdk/go/src/runtime/proc.go#L3839)：M 并没有休息，它立即执行 `schedule()` 去全局队列或本地队列寻找下一个待执行的 G。这就是 Go 高并发的核心秘密——**IO 阻塞的是 Goroutine，而不是系统线程**。

## 总结

至此，我们完整解构了 `Accept` 从用户层 API 到 Runtime 调度器的全过程。这一复杂的调用链完美诠释了 Go 语言**同步的代码逻辑，异步的底层实现**的核心设计哲学：

1. **表象与本质的统一**：
* **对开发者**：`Accept` 表现为标准的**阻塞式 I/O**。代码线性执行，逻辑清晰，符合人类直觉，不需要像 C 语言 epoll 那样编写复杂的回调或状态机。
* **对操作系统**：`Accept` 实际上是**非阻塞 I/O**。底层通过 `EAGAIN` 错误码和 `epoll` 机制，确保了系统线程永远不会因为等待网络数据而阻塞。


2. **M 与 G 的接力**：
整个流程的关键点在于 `gopark` 和 `schedule` 的配合。当 I/O 未就绪时：
* **Goroutine (G)** 选择“让出”：它保存现场，进入 `_Gwaiting` 状态，乖乖在堆上等待数据到来。
* **系统线程 (M)** 选择“复用”：它通过切换到 `g0` 栈，迅速摆脱了当前 G 的纠缠，立即执行 `schedule()` 寻找下一个需要 CPU 的 G。


3. **高性能的秘密**：
这就解释了为什么 Go 服务端可以用少量的系统线程支撑数万计的并发连接——**因为所有的“等待”成本都由极度廉价的 Goroutine 承担了，而昂贵的系统线程（M）始终处于高负载的有效计算状态，从未真正休息。** 这正是 Go 网络模型区别于传统多线程模型的最大护城河。

---