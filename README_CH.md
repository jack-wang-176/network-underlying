<div align="center">

# network-underlying

[English Version](./README.md) | **中文版本**

</div>

你好，这是我学习网络部分的笔记，在这份笔记中我尝试去依照网络五层模型来组织这一整篇笔记，来帮助读者和我自己去建立基本的网络模型
 **[点击这里进入核心目录与阅读指南 (Table of Contents)](./docs/zh/SUMMARY.md)**

 **Part 1**：基于 C 语言构建基础网络模型，演示服务端并发模型从 fork 到 epoll 的演进。集中展示网络层的套接字应用。

 **Part 2**：深入 Go 语言 Runtime 源码，全面解构 netpoll 网络多路复用体系与 Goroutine 调度。展示应用层工具的开发和封装和现代web的网络基础。

 **Part 3**：通过 C 语言 Raw Socket 越过传输层，手动构造、发送并解析底层的 IP、TCP 与 UDP 数据包。这一部分接受数据包并递进解释数据包头的套接和从链路层到网络层的协议实现

---
*注：本项目仍在持续更新中 (Work in Progress)，欢迎 Watch 与 Star 以获取最新动态。*