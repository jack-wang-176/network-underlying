
# 01 Basic (基础概念)
网络通信的基石，主要解决不同层次上的数据表示差异。

* **[01_endian](../../../01_socket_underlying_c/01_basic/01_endian.c) (字节序)**
    * 展示了计算机 **小端存储 (Little-Endian)** 与 **大端存储 (Big-Endian)** 的区别。
    * **为什么会有这种存储的差异性**：这纯粹是CPU架构的历史遗留问题（比如 Intel x86 选了小端，而早期的 Motorola 选了大端）。但为了防止乱套，网络协议强行规定了必须用**大端**作为网络字节序。所以我们在发包前必须老老实实把主机的小端序转过去。
  
* **[02_htol_htons](../../../01_socket_underlying_c/01_basic/02_htol_htons.c) (字节序转换)**
    * 基于 `<arpa/inet.h>` 头文件。
      ```c
      extern uint16_t htons (uint16_t __hostshort)
      __THROW __attribute__ ((__const__));
      ```
    * 实现 **主机字节序 (Host)** 向 **网络字节序 (Network)** 的转换 (如 `htonl`, `htons`)。
    * **解释一下 `__THROW` 和 `__attribute__`**：这其实是写给编译器看的tip。`__THROW` 告诉编译器这函数绝不抛出异常，`__const__` 告诉编译器这函数是“纯函数”（只依赖输入，没副作用）。这样编译器就能大胆地做优化，把多余的调用给省掉。

* **[03_inet_pton](../../../01_socket_underlying_c/01_basic/03_inet_pton.c) (IP地址转换)**
    * 全称 *Presentation to Numeric*。
    * 将点分十进制字符串 (如 "192.168.1.1") 转换为网络传输用的 32位无符号整数。
      ```c
      int inet_pton (int __af, const char *__restrict __cp,
      void *__restrict __buf) __THROW;
      ```
    * **为什么需要一个 void 类型**：这里设计得很巧妙，因为 IPv4 用 `struct in_addr` (4字节)，IPv6 用 `struct in6_addr` (16字节)。用 `void*` 就能像万能插头一样，不管你是哪种协议，都能把转换后的二进制数据填进去。

* **[04_inet_ntop](../../../01_socket_underlying_c/01_basic/04_inet_ntop.c)(IP地址还原)**
    * 全称 *Numeric to Presentation*。
    * 将 32位网络字节序整数还原为人类可读的 IP 字符串。
      ```c
      extern const char *inet_ntop (int __af, const void *__restrict __cp,
      char *__restrict __buf, socklen_t __len)
      __THROW;
      ```
    * `extern` 意味着这是个外部引用，`__len` 则是为了防止缓冲区溢出（C语言老生常谈的内存安全问题），这一部分被认为是add部分单独开一行。
