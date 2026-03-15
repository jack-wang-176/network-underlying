#  详细目录 (Table of Contents)

欢迎来到 `network-underlying` 的阅读指引。请根据以下模块的递进关系，选择你要阅读的章节。

---

## Part 1: Socket-Underlying-C
探讨传统的 C 语言系统调用与操作系统级别的网络通信原理。
* [01 基础概念](./part1-socket-c/01-basic.md)
* [02 UDP 通信](./part1-socket-c/02-udp-socket.md)
* [03 TFTP 协议实现](./part1-socket-c/03-tftp.md)
* [04 广播与多播](./part1-socket-c/04-broadcast-multicast.md)
* [05 TCP 通信](./part1-socket-c/05-tcp-socket.md)

## Part 2: Netpoll-Underlying-Go
深入 Go Runtime，解析 Go 语言高并发网络模型背后的核心机制。
* [01 本节目标与机制概览](./part2-netpoll-go/01-overview.md)
* [02 listen 函数的内部调用](./part2-netpoll-go/02-listen-internal.md)
* [03 netpoll 的网络体系：深入 Runtime](./part2-netpoll-go/03-runtime-architecture.md)
* [04 Accept 的底层实现](./part2-netpoll-go/04-accept-internal.md)
* [05 数据的传输、I/O 模型与总结](./part2-netpoll-go/05-io-models-and-summary.md)

## Part 3: Rawsocket-Underlying-C
越过操作系统标准传输层，直面原始数据链路层帧。
* [01 深入协议栈的准备与指针强转解封装](./part3-rawsocket-c/01-prep-and-casting.md)
* [02 TCP 握手包的发送](./part3-rawsocket-c/02-send-tcp-handshake.md)
* [03 阶段性总结与架构思考](./part3-rawsocket-c/03-summary.md)


---
>  **[返回项目主页 (Back to Home)](../../README.md)**