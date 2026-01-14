<div align="center">

# NetProtocol-Lab (Webcoding)
**English Version** | [中文版本](./README.md)
<br>
> This project demonstrates C-based network programming. While the code serves as a necessary tool for understanding, the primary focus is on the **design philosophy** within network protocols. I strongly recommend **typing the code by hand** and using this README as a guide to experiment. To clearly illustrate the network structure, all code acts as a simplified implementation of specific functions. Through this process, I believe you will grasp the underlying implementation of network design. Currently, this project covers simplified implementations of TCP and UDP in C. Future updates will include analyses of data packets, the IP protocol stack, and interface analysis of Go's underlying design. Although not a top-down structural analysis, this is an excellent starting point for network beginners.

![Language](https://img.shields.io/badge/language-C-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20WSL-green.svg)
![Editor](https://img.shields.io/badge/Editor-VS%20Code-orange.svg)

</div>

---

## Contents

This project is divided into the following modules based on the learning path:

### Part 1: C Network Programming
- [01 Basic (Fundamentals)](#01-basic-fundamentals)
- [02 UDP Socket](#02-udp-socket)
- [03 TFTP Implementation](#03-tftp-implementation)
- [04 Broadcast & Multicast](#04-broadcast--multicast)
- [05 TCP Socket](#05-tcp-socket)

---

## Introduction & Details

### 01 Basic (Fundamentals)
The cornerstone of network communication, primarily addressing data representation differences across layers.

* **01_endian (Byte Order)**
    * Demonstrates the difference between **Little-Endian** and **Big-Endian**.
    * **Why the difference?** This is a historical legacy of CPU architectures (e.g., Intel x86 chose Little-Endian, while early Motorola chose Big-Endian). To prevent chaos, network protocols mandate **Big-Endian** as the standard "Network Byte Order." Therefore, we must convert host Little-Endian data before sending packets.
  
* **02_htol_htons (Byte Order Conversion)**
    * Based on `<arpa/inet.h>`.
      ```c
      extern uint16_t htons (uint16_t __hostshort)
      __THROW __attribute__ ((__const__));
      ```
    * Implements conversion from **Host Byte Order** to **Network Byte Order** (e.g., `htonl`, `htons`).
    * **Note on `__THROW` and `__attribute__`**: These are hints for the compiler. `__THROW` tells the compiler the function won't throw exceptions, and `__const__` indicates it's a "pure function" (depends only on input, no side effects), allowing the compiler to optimize safely.

* **03_inet_pton (IP Conversion)**
    * *Presentation to Numeric*.
    * Converts dotted-decimal strings (e.g., "192.168.1.1") into 32-bit unsigned integers for network transmission.
      ```c
      int inet_pton (int __af, const char *__restrict __cp,
      void *__restrict __buf) __THROW;
      ```
    * **Why `void *`?** This is a clever design. IPv4 uses `struct in_addr` (4 bytes), while IPv6 uses `struct in6_addr` (16 bytes). Using `void*` acts as a universal adapter to accept binary data for either protocol.

* **04_inet_ntop (IP Restoration)**
    * *Numeric to Presentation*.
    * Restores 32-bit network integers back to human-readable IP strings.
    * `__len` is included to prevent buffer overflows—a classic memory safety issue in C.

### 02 UDP Socket
A connectionless, unreliable data transmission protocol.

* **01_socket**
    * Demonstrates the socket creation function:
      ```c
      int socket (int __domain, int __type, int __protocol)
      ```
    * `domain` determines the IP type; `type` determines TCP or UDP; `protocol` specifies the format.
    * A socket is an `int` (File Descriptor), reflecting the Unix philosophy: "Everything is a file."
    * **Crucial**: Always `close` the socket at the end. In UDP, it cleans up resources; in TCP, it triggers the connection termination process.

* **02_sendto**
   * Demonstrates UDP data transmission.
      ```c
      ssize_t sendto (int __fd, const void *__buf, size_t __n,int __flags, __CONST_SOCKADDR_ARG __addr,socklen_t __addr_len);
      ```
  * **What is `ssize_t`?** It is a `signed int`. It returns the number of bytes sent (positive) or -1 for errors. Unsigned `size_t` cannot represent the -1 error state.
  * **Note**: We populate `struct sockaddr_in` (for IP/Port) but cast it to the generic `struct sockaddr` when calling the function. This highlights the C polymorphism technique to handle data compression and the conflict between natural language and machine binary.

* **03_bind**
  * Fixes the IP and Port.
    ```c
    int bind (int __fd, __CONST_SOCKADDR_ARG __addr, socklen_t __len) __THROW;
    ```
  * In TCP/UDP, we typically call the side that binds the "Server". However, this is strictly determined by the relative relationship of sending and receiving information, which becomes more evident in Broadcast and Multicast.

* **04_recvfrom**
  * The UDP receiving function.
  * **Key Design**: `recvfrom` accepts data from *any* host. We must pre-allocate an empty structure for the kernel to fill in the sender's details. This design makes UDP naturally suitable for multi-client concurrency without complex handshakes.

* **05_server && 06_client** * Concrete implementation of UDP communication.
  * **Logic**: The client needs the server's IP/Port, but doesn't need to care about its own.
  * **Core Utility**: The server uses `recvfrom` to capture the client's address, which is then immediately used in `sendto` to reply. This "stateless" nature makes UDP multi-client implementation slightly verbose but simple.

* **UDP Interaction Flow**:
    ```text
    [Client]                          [Server]
       |                                 |
       |--- sendto(Data, ServerIP) ----->|
       |                                 | recvfrom (Get ClientIP)
       |                                 |
       |<-- sendto(Echo, ClientIP) ------|
       |                                 |
    recvfrom(Echo)
    ```

### 03 TFTP Implementation
*Trivial File Transfer Protocol*.

* **Overview**
  * The core challenge of implementing TFTP in C is **manually constructing and parsing binary packets**. You are piecing together bytes, not strings.

* **01_tftp_client**
  * **Packet Construction**:
    * **Structure**: `| Opcode (2B) | Filename | 0 | Mode | 0 |`
    * **Technique**: Used `sprintf` to concatenate data.
      ```c
      packet_buf_len = sprintf((char*)packet_buf, "%c%c%s%c%s%c", 0, 1, filename, 0, "octet", 0);
      ```
    * *Byte Order Note*: Writing `0` then `1` sequentially into memory naturally forms `00 01`, satisfying the Big-Endian requirement.
  * **State Machine & Reliability**:
    * Since UDP is unreliable, we must manually verify packet headers (`Opcode` and `Block Number`).
    * **Manual ACK**: If the received block number matches expectations (`ntohs` required for comparison), we construct and send an ACK packet (`Opcode 4`).

* **02_tftp_server**
  * Passive logic: Verify Request -> Read File -> Send Data -> Wait for ACK.
  * **Error Handling**: Extracted a `senderr` function to construct error packets (`Opcode 5`) swiftly when logic fails (e.g., file not found).

### 04 Broadcast & Multicast

* **Background**
  * Introduced `setsockopt` to modify socket permissions (e.g., `SO_BROADCAST`, `SO_REUSEADDR`).

* **01_broadcast_send & 02_broadcast_recv**
  * **Philosophy**: Broadcast is like a "Loudspeaker." It consumes subnet bandwidth. Therefore, the **Sender** must explicitly request permission (`setsockopt`) to "shout," while the Receiver listens passively without special permissions.

* **03_groupcast_send & 04_groupcast_recv (Multicast)**
  * **IP Classes**:
    * Class D (224.0.0.0 ~ 239.255.255.255) is reserved for Multicast.
  * **Philosophy**: Contrary to Broadcast, Multicast is precise.
    * **Sender**: Does NOT need `setsockopt`. Just sends to a Class D IP.
    * **Receiver**: MUST use `setsockopt` (`IP_ADD_MEMBERSHIP`) to explicitly "subscribe" to the channel.
    * **struct ip_mreq**: Used to define the Multicast Group IP and the Local Interface (`INADDR_ANY` allows listening on all interfaces).

### 05 TCP Socket

* **Background**
  * **Design Shift**: UDP is stateless; TCP is connection-oriented.
  * **Concurrency Contradiction**: How to manage thousands of "Service Sockets"?
    1. **Multi-process/Thread**: Add more workers (heavy).
    2. **IO Multiplexing**: Non-blocking IO + Event Loop (Epoll) (efficient).

* **01_client**
  * Uses `connect()` to trigger the **Three-Way Handshake**.
  * **Buffer Trap**: When sending strings, use `strlen(buf)`, NOT `sizeof(buf)`. `sizeof` includes garbage data in the rest of the array.

* **02_server**
  * **Listen**: `listen(sockfd, backlog)` maintains the "Incomplete Connection Queue" and "Completed Connection Queue".
  * **Accept**: Returns a **NEW** File Descriptor.
    * *Philosophy*: The original `sockfd` is the "Doorman"; the new `fd` is the "Waiter" for that specific table. This separation allows handling handshakes and data simultaneously.

* **03_server_fork**
  * Uses `fork()` for concurrency.
  * **COW (Copy On Write)**: Ensures efficiency.
  * **Zombie Processes**: Solved by registering a `signal(SIGCHLD)` handler with `waitpid(..., WNOHANG)` to asynchronous clean up child processes.

* **04_server_thread**
  * Uses `pthread_create`.
  * **Detach**: `pthread_detach(pthread_self())` allows the kernel to auto-recycle resources upon thread exit, avoiding the need for `pthread_join`.

* **05_server_noblock**
  * Sets `O_NONBLOCK` via `fcntl`.
  * **Errno**: If `recv` returns `-1` with `EAGAIN`, it means "No data now, try later," not a fatal error.

* **06_server_epoll**
  * The pinnacle of Linux IO.
  * **Level Triggered (LT)** vs **Edge Triggered (ET)**.
  * **Workflow**: `epoll_wait` blocks until events occur. It returns *only* the active sockets, avoiding the need to iterate through all connections (O(1) vs O(n)).
  * **Insight**: This single-thread + event loop model is the precursor to modern high-concurrency solutions (like Go's underlying netpoller).

---