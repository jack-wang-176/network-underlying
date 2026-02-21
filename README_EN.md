<div align="center">

# network-underlying
**English Version** | [中文版本](./README.md)
<br>
> This project aims to explore the underlying principles of network programming and the evolution of concurrency architectures from the perspective of code implementation. All example code has been simplified, stripping away complex business logic to retain only the core mechanisms of network communication and concurrency scheduling. This serves as an objective reference for understanding low-level system design.
> **In Part 1**, the project builds foundational network models using C. The content covers endianness conversion, UDP/TCP communication mechanisms, manual construction and parsing of TFTP protocol binary packets, as well as the implementation of broadcast and multicast. More importantly, this section intuitively demonstrates the evolution of server concurrency models: transitioning from multi-processing (`fork`) and multi-threading (`pthread`) to the Linux `epoll`-based I/O multiplexing model. This establishes the necessary operating system-level foundational knowledge for understanding modern high-concurrency architectures.
> **In Part 2**, the project's perspective shifts to the Go runtime source code to comprehensively deconstruct its `netpoll` network multiplexing system. Building on the `epoll` foundation from the first half, it provides a detailed breakdown of how Go internalizes the underlying asynchronous event polling mechanism. The content includes penetrating the call boundaries between the `internal` and `runtime` packages, analyzing the cross-level pointer mapping of the `pollDesc` struct, and exploring how `gopark` unbinds system threads (M) and suspends goroutines (G). This reveals the core of Go's network implementation: "synchronous calls at the user level, asynchronous execution at the lower level."
> **Practical Advice:** It is highly recommended to compile and debug the code of this project in a Linux/WSL environment. Although this project does not provide a top-down panoramic overview of computer networks, it serves as an objective and intuitive source-level starting point. It is ideal for beginners looking to solidify their system-level networking foundation, or for advanced developers dedicated to exploring the underlying black boxes of high-level languages.

![Language](https://img.shields.io/badge/language-C-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20WSL-green.svg)
![Editor](https://img.shields.io/badge/Editor-VS%20Code-orange.svg)
![Language](https://img.shields.io/badge/Language-Go-00ADD8?style=flat&logo=go&logoColor=white

</div>

---

## Contents

This project is divided into the following modules based on the learning path:

### [Part 1: Socket-Underlying-C](#webcoding-based-on-c)
- [01 Basic (Fundamentals)](#01-basic-fundamentals)
- [02 UDP Socket](#02-udp-socket)
- [03 TFTP Implementation](#03-tftp-implementation)
- [04 Broadcast & Multicast](#04-broadcast--multicast)
- [05 TCP Socket](#05-tcp-socket)
### [Part 2: Netpoll-Underlying-Go](#netpoll-underlying-go)
- [01 The Goal of This Part](#the-goal-of-this-part)
- [02 Sample Code Dissection & netpoll Internals Overview](#sample-code-dissection--overview-of-the-underlying-netpoll-mechanism)
- [03 listen Function Internals](#internal-calls-of-the-listen-function)
- [04 The Netpoll Architecture](#the-netpoll-network-architecture-deep-dive-into-the-runtime)
- [05 Underlying implementation of Accept](#the-underlying-implementation-of-accept)
- [06 Data transmission and I/O models](#data-transmission-and-io-models)
- [07 Summary)](#summary)
---

## Introduction & Details

## **1.Socket-Underlying-C**

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

### 02 UDP Socket (UDP Communication)

Connectionless, unreliable data transmission protocol.

* **01_socket (Socket)**
* Shows the function for creating a socket.
```c
int socket (int __domain, int __type, int __protocol)

```


* `domain` determines the IP type, while `type` determines the transmission type (TCP or UDP). `protocol` is the specific protocol format.
* `socket` is an `int` type; it relies on the abstraction of file descriptors to implement calls at the system level, demonstrating the "everything is a file" design philosophy in Linux.
* As a file descriptor, it must be closed at the end of the program. `close` means disconnecting during communication; in TCP, this becomes more complex.


* **02_sendto**
* Shows data transmission for the UDP transport type.
```c
ssize_t sendto (int __fd, const void *__buf, size_t __n,int __flags, __CONST_SOCKADDR_ARG __addr,socklen_t __addr_len);

```


* **What is ssize_t**: It is actually `signed int`. Because this function returns the number of sent bytes (positive) on success, and needs to return -1 on failure. If distinct `size_t` (unsigned) were used, the error status -1 could not be represented.
* **adding**
* Here, when performing data transmission, the IP and port are recorded and written through `sockaddr_in`.
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


* It should be noted that although we write to `sockaddr_in`, the encapsulation function actually writes to the `sockaddr` structure.
```c
struct sockaddr
{
__SOCKADDR_COMMON (sa_);  /* Common data: address family and length.  */
char sa_data[14];   /* Address data.  */
};

```


* As can be seen, this is actually a data compression process. Such a design reveals a core issue in programming: the contradiction between natural language programming and machine binary composition.


* **03_bind**
* The `bind` function is mainly used to fix the IP and port number. Based on the orientation of this function, it is easy to understand that the information receiver needs this requirement more.
```c
int bind (int __fd, __CONST_SOCKADDR_ARG __addr, socklen_t __len)
    __THROW;

```


* In TCP/UDP programming, we generally simply refer to the side that needs binding as the server, but this is actually determined by the relative relationship between information receiving and sending. We can see further embodiment of this in multicast and group casting.


* **04_recvfrom**
* `recvfrom` is the UDP receiving function.
```c
recvfrom (int __fd, void *__restrict __buf, size_t __n, int __flags,
    __SOCKADDR_ARG __addr, socklen_t *__restrict __addr_len)

```


* It is important to note here that `recvfrom` receives data from other hosts, so we need to pre-create an empty structure for it to fill in. Also, `addrlen` is changed to output the length of the received information. This design allows UDP to implement multi-threading very simply. The cost is that every client needs a corresponding structure. We will see later that because of the three-way handshake required by TCP, TCP's design takes a completely different path.


* **05_server&&06_client** * This section contains the specific running code for the UDP client and server. Essentially, it invokes the functions mentioned above to concretely implement the communication process.
```c
  if(argc<3){
fprintf(stderr,"Usage : %s<IP> <PORT>\n",argv[0]);
exit(1);
} 

```


* This part ensures that IP and port are entered when running the program. Note that what is entered in the client is also the server's IP and port, because the client does not need to care about itself; it only needs to guarantee data interaction.
```c
if(recvfrom(sockfd,buf,sizeof(buf),0,(struct sockaddr*)&clientaddr,&addrlen)==-1){
    perror("fail to recvfrom");
    // Even if reception fails, it can continue
    continue;
}

```


* This reflects the core utility of `recvfrom`, which is the heart of UDP transmission. By receiving data, it records the sending host's IP for use in `sendto` operations. This design makes UDP multi-client implementation extremely easy, although it is slightly redundant when actually writing the execution code.


* **Simple diagram of UDP communication interaction process**:

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

* **adding**
* Here, our input and output mainly use `fgets` and `printf` methods.
```c
extern char *fgets (char *__restrict __s, int __n, FILE *__restrict __stream)
  __wur __fortified_attr_access (__write_only__, 1, 2) __nonnull ((3));

```


* **The Pitfall of Buffers**: In C, strings use `\0` as the data boundary in memory, while `fgets` includes the newline character. If data purity is desired, the newline character should be removed. However, for `printf`, data is printed from the buffer only when a newline character is encountered. Therefore, no special processing is needed when pairing these two functions.


### 03 TFTP Implementation (TFTP Protocol Implementation)

*Trivial File Transfer Protocol*.

* **begin**
* Before we begin, I want to briefly introduce the core of understanding TFTP. As a small file transfer protocol based on UDP, the most annoying yet critical part of implementing TFTP in C is **manually constructing and analyzing binary packets**. You need to piece together every single byte.


* **01_tftp_client**
* **Part 1: Packet Construction Area**
* Specify the download filename `scanf("%s",filename);`
* The first difficulty is constructing the data packet; this isn't a string, it's compact binary.
* **TFTP Binary Packet Structure Diagram**:
```text
2 bytes     string    1 byte     string   1 byte
------------------------------------------------
| Opcode |  Filename  |   0  |    Mode    |   0  |
------------------------------------------------

```


* The code uses a clever operation `sprintf` to splice:
```c
packet_buf_len = sprintf((char*)packet_buf,"%c%c%s%c%s%c",0,1,filename,0,"octet",0);

```


* **Explain Byte Order**: Why do we not need to worry about big-endian and little-endian conversion here? Because `sprintf` writes single bytes sequentially. Writing 0 and then 1 results in `00 01` in memory, which happens to match the big-endian requirement of network byte order.


* **Part 2: Receive and Parse Loop (State Machine)**
* `packet_buf` is used to receive data sent from the server. Here, all data is sent using the `unsigned char` type. Storing this data means we can simply and directly use this array to parse the packet header.
```c
 // This is the transport level of the data packet
 unsigned char packet_buf[1024]= "";

```


```c
// Error message
if(packet_buf[1]== 5){...exit(1)}
// Received correct feedback request from server
if(packet_buf[1]==3){// Proceed to next step}

```


* It is important to note that we must first determine if the corresponding file exists; a `bool` or `int` type can be used as a flag. If not, the corresponding file needs to be created first.


* **Part 3: Verification and ACK (Manual Reliability)**
* Another difficulty is receiving and checking the block number in the data packet. Since UDP is an unreliable connection, we need to manually verify if data loss has occurred. We need to read from the packet header and compare it with the local record.
* **Data Verification Process**:
```c
if((num +1) == ntohs(*(packet_buf+2)))
// success -> send ACK packet
// fail    -> data lost, exit

```


* If the current data packet is fine, we need to construct an ACK packet and send it to the server to trigger sending the next block of data. Here, `ntohs` must be used because the sequence number in the header is in network order and must be converted to host order for comparison.
```c
packet_buf[1]= 4;

```


* Although file block data is sent here, the server only needs to verify the packet header.
* If the data block is smaller than 516 (meaning file data is less than 512), it indicates the writing is finished. However, the case where the file size is exactly a multiple of 512 is not considered here.
* **Summary**: The main challenge for such a client is the processing of binary packets. Since UDP itself is unreliable, we need to manually verify packet headers. As we can see, this verification logic is actually very similar to TCP's three-way handshake, which is why TCP usually handles file transfer tasks.


* **02_tftp_server**
* The server-side logic is relatively passive, mainly focusing on parsing and feedback:
1. Verify the data packet and check if the relevant file exists.
2. Define a block number and write it into the header, using the buffer as a relay for file data.
3. Wait for and parse the ACK packet.


* **Construct Error Packet (Helper Function)**
Note here a function for constructing error packets, extracted for code cleanliness:
```c
void senderr(int sockfd,struct sockaddr* clientaddr,char* err,int errcode,socklen_t addrlen){
  unsigned char buf[516] = "";
  // Construct error packet: [00] [05] [00] [ErrCode] [ErrMsg] [00]
  int buf_len = sprintf((char*)buf, "%c%c%c%c%s%c", 0, 5, 0, errcode, err, 0);
  sendto(sockfd,buf,buf_len,0,clientaddr,addrlen);
}

```


* Such a function allows us to quickly send relevant information at logical terminal nodes (e.g., file cannot be opened).
* **Summary**: The design philosophy exhibited by the server here is consistent with the client. It should be noted that both sides store block numbers locally, and both sides parse the data packets.


### 04 Broadcast & Multicast

* **background** * Here we first need to introduce a function `setsockopt`.
```c
extern int setsockopt (int __fd, int   __level, int __optname,
const void *__optval, socklen_t __optlen) __THROW;

```


* The role of this function is to apply further restrictions or specifications to the file descriptor.
* **Parameter Explanation**:
* `__fd`: The file descriptor of the socket.
* `__level`: The layer where the option is defined. Usually set to `SOL_SOCKET` (generic socket options) or `IPPROTO_IP` (IP layer options).
* `__optname`: The specific option name to set. For example, `SO_BROADCAST` (allow broadcast), `SO_REUSEADDR` (port reuse).
* `__optval`: A pointer to the buffer containing the option value. Usually a pointer to an `int`, where `1` enables and `0` disables.
* `__optlen`: The length of the `optval` buffer.


* In the server, we can also configure port reuse modes to facilitate debugging (or restarting), but for the sake of code simplicity, I did not add this part in the code instance.
* **Port Reuse Code Example**:
```c
int opt = 1;
// Allow reuse of local address and port, solving "Address already in use" error
setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

```




* **01_broadcast_send** * This file demonstrates the broadcast sender. Unlike TCP/UDP programming, in broadcast and multicast, there is no traditional Client-Server framework, but rather a relative relationship between information sending and receiving.
* Here `sendto` is used. Aside from needing to add extra functionality to the socket, the logic is basically consistent with the UDP client.


* **02_broadcast_recv.c**
* The structure of this file is even simpler than `udp_server`, because here the broadcast address is fixed, and we only need to listen for corresponding data packets.
* What's interesting here is that `recv` doesn't need specific permissions set. This aligns with the design philosophy of broadcast: the sender needs extra verification, while the receiver only needs to judge if the packet is meant for it.


* **summary**
* Broadcast implementation is based on UDP because broadcast itself is a one-to-many unidirectional process. In actual networking, it often involves repeated broadcasts, so the importance of rapid data transmission far outweighs stable transmission here.
* **Broadcast Design Philosophy**: Broadcast is like "shouting with a loudspeaker." Because this behavior consumes the bandwidth of the entire subnet and may cause disturbance (broadcast storms), the kernel design requires the **sender** to explicitly call `setsockopt(SO_BROADCAST)` to request permission (turn on the switch). The receiver is passive and can hear it without special permissions.


* **adding (IP Class Knowledge)**
* To understand multicast, one must first learn IP classification knowledge:
* **Class A/B/C**: Used for Unicast (one-to-one communication).
* **Class D (224.0.0.0 ~ 239.255.255.255)**: **Dedicated to Multicast**. These IPs do not belong to any specific host but represent a "group". Sending data to this IP means all hosts that have joined this group will receive it.
* **Class E**: Reserved for research.


* **03_groupcast_send.c**
* Here, the multicast sender doesn't even need to use `setsockopt`. This is because Class D IP segments are inherently dedicated to multicast. So, `send` only needs to transmit data to these IP segments; when it sends, it has effectively already set the corresponding multicast group on that IP.


* **adding**
* **What is INADDR_ANY**: We often see `server_addr.sin_addr.s_addr = htonl(INADDR_ANY);` in code. Its value is actually `0.0.0.0`. It means "bind to all available local network interfaces". If you have both Wi-Fi and an Ethernet cable, using `INADDR_ANY` allows you to receive data from both network cards, without binding the program to a specific IP.


* **04_groupcast_recv.c**
* `recv` needs to use `setsockopt` for configuration. As mentioned before, `_optval` in `setsockopt` is a `void*` type, which means we can construct a structure to pass data parameters; this is a common method in C. Here we need to use the `ip_mreq` structure, specifically designed for multicast groups, to set parameters:
```c
struct ip_mreq
{
  /* IP multicast address of group.  */
  struct in_addr imr_multiaddr; // Multicast group IP (e.g. 224.0.0.88)

  /* Local IP address of interface.  */
  struct in_addr imr_interface; // Interface IP to join the group with (usually INADDR_ANY)
};


```


* Here `imr_interface` is the local interface, and `imr_multiaddr` is the multicast IP. Both have `s_addr` members; like the design of `sockaddr_in`, this is due to historical reasons.
* **summary (Broadcast vs Multicast Philosophy)**
* A clear distinction must be made here from broadcast, reflecting the diametrically opposite underlying logic of the two:
* **Broadcast**: The **sender** needs `setsockopt`. Because broadcast is forceful/violent and disabled by default, the sender must actively request permission to "shout".
* **Multicast**: The **receiver** needs `setsockopt` (join group `IP_ADD_MEMBERSHIP`). Because multicast is precise, the sender just sends data to a Class D IP (anyone can send). The key is that the receiver must explicitly declare "I subscribe to this channel" before the kernel will fish out the corresponding data packets for you.

### 05 TCP Socket (TCP Communication)

* **background**
* Although the biggest difference between TCP and UDP here is that TCP uses a three-way handshake and four-way wave to guarantee data transmission, most of these complex state transitions are encapsulated by the kernel when we program using functions. In other words, we consider socket lifecycle management more from the **Application Layer** perspective here.
* **Shift in Design Philosophy**: UDP is stateless; a socket can send packets to any IP. However, TCP is connection-oriented, like making a phone call; you must connect before speaking. This design requires the server to maintain a "listening socket" specifically to welcome guests, and for every guest (client) that arrives, a new "service socket" must be created specifically for chatting.
* **Core Contradiction of Concurrency**: How to efficiently manage these hundreds or thousands of "service sockets"? This leads to two technical routes:
1. **Multi-process/Multi-thread**: Solve by adding manpower (CPU scheduling units); one connection corresponds to one thread/process.
2. **IO Multiplexing (Non-blocking)**: Use non-blocking IO + Event Polling (e.g., epoll) to let one waiter (single thread) watch over all tables.




* **01_client**
* This section shows the TCP client. After creating the socket and encapsulating the server structure, we first need to call the encapsulated function to establish the underlying connection.
```c
extern int connect (int __fd, __CONST_SOCKADDR_ARG __addr, socklen_t __len);

```


* **Underlying Mechanism of Connect (Three-Way Handshake Trigger)**:
1. When `connect` is called, the kernel sends a **SYN** packet to the Server.
2. At this point, the function blocks, waiting for the Server to reply with **SYN+ACK**.
3. After receiving the reply, the Client sends an **ACK**. At this point, the connection is established (ESTABLISHED), and the function returns 0.


* The client side usually only needs to maintain one socket. After the connection is established, the kernel has already bound this socket to a specific remote IP and port, so the `send` function does not need to repeatedly specify the destination address like `sendto`.
```c
extern ssize_t send (int __fd, const void *__buf, size_t __n, int __flags);

```




* **adding (Buffer Trap)**
* **The Pitfall of strlen vs sizeof**: When sending strings, **never use `sizeof(buf)`, use `strlen(buf)**`.
* **Reason**: `sizeof` calculates the total memory allocated for the array (e.g., 1024), while `strlen` calculates the actual character length (e.g., "hello" is 5). If you use `sizeof`, you will send the hundreds of bytes of useless garbage data (乱码) following the string in the buffer to the other party, which is disastrous when processing protocols.


* **02_server.c**
* Here is a TCP server instance. After creating the socket and filling/binding the structure, the socket must first be set to the listening state.
```c
extern int listen (int __fd, int __n) __THROW;

```


* `__fd`: The socket file descriptor created previously.
* `__n`: **Backlog (Queue Length)**
* **Why Listen is Needed**:
* The kernel maintains two queues for the listening socket: the **Half-open Connection Queue** (SYN received but final ACK not received) and the **Fully Connected Queue** (Three-way handshake complete, waiting for Accept to take it).
* `__n` actually determines the size of these queues (usually the fully connected queue). If the queue is full, new connection requests will be directly dropped or rejected (SYN Flood attacks target this).
* After setting the listening state, use `accept` to retrieve a completed connection from the fully connected queue.
```c
extern int accept (int __fd, __SOCKADDR_ARG __addr,
socklen_t *__restrict __addr_len);


```


* **A Tale of Two FDs**:
* The `int` returned by `accept` is a **brand new file descriptor** (Connected Socket).
* **Design Philosophy**: The original `sockfd` is only responsible for welcoming people in; the `fd` returned by `accept` is specifically responsible for communicating with this specific table. This separation allows the TCP Server to handle handshake requests and data transmission simultaneously.
* **Judgment of Recv Return Value**:
```c
extern ssize_t recv (int __fd, void *__buf, size_t __n, int __flags);


```


* `> 0`: Number of bytes received.
* `= 0`: **Important!** This represents that the peer has closed the connection (FIN packet). TCP is full-duplex, so reading 0 bytes means the Read channel is closed.
* `< 0`: Error, need to check `errno`.
* *Note*: While in UDP (likely a typo for "dup" in source context), you can directly send data packets of length 0.
* **summary (CS Framework)**


* **TCP C/S Interaction Flowchart**:
```text
    [Server]                  [Client]
  socket()                  socket()
      |                         |
    bind()                      |
      |                         |
  listen()                      |
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
* Here, concurrency is implemented using multi-processing.
```c
extern __pid_t fork (void) __THROWNL;

```


* **The Magic of Fork**: Call once, return twice.
* Returns `> 0` (Child Process PID): Current is the parent process; the task is to continue `accept` and wait for new people.
* Returns `0`: Current is the child process; it inherits all resources of the parent process (including the socket). Its task is to handle the `send/recv` for the connection just made.
* **COW (Copy On Write)**: Linux is very efficient here; it does not immediately copy all the parent process's memory. It only truly copies memory pages when the child process attempts to modify data.
* **Zombie Processes and Signal Recovery**:
* If the parent process ignores the child process when it ends, it becomes a "Zombie Process" occupying a PID resource.
* We use the `signal` mechanism for asynchronous recovery.
```c
// Register signal handler
signal(SIGCHLD, handler);

void handler(int sig){
  // Loop to reclaim all finished child processes
  while((waitpid(-1, NULL, WNOHANG)) > 0){}
}


```


* **Waitpid Parameter Explanation**:
* `-1`: Wait for any child process.
* `NULL`: Don't care about the specific exit status code.
* `WNOHANG`: **Non-blocking Key**. If no child process has finished currently, return 0 immediately; don't sit there blocking. This ensures the Server doesn't stop responding to new requests just to clean up garbage.


* **04_server_thread.c**
* Using multi-threading for processing. A process is the unit of resource allocation (Heavy); a thread is the unit of CPU scheduling (Light).
```c
extern int pthread_create (pthread_t *__restrict __newthread,
      const pthread_attr_t *__restrict __attr,
      void *(*__start_routine) (void *),
      void *__restrict __arg) __THROWNL __nonnull ((1, 3));


```


* **Parameter Explanation**:
* `__newthread`: Pointer to thread ID, used to receive the new thread ID.
* `__attr`: Thread attributes, usually pass `NULL` to use defaults.
* `__start_routine`: Function pointer to execute after thread start.
* `__arg`: The unique argument passed to the start function. Since only one can be passed, socket, IP, etc., usually need to be packed into a struct and passed as `void*`.
* **Compilation Instruction**:
```bash
gcc server_thread.c -o server -lpthread

```


* **Automatic Garbage Collection (Detach)**:
```c
pthread_detach(pthread_self());

```


* **Principle**: By default, threads are `joinable`, meaning the main thread must call `pthread_join` to "collect the body" after exit. Calling `detach` tells the kernel: "This thread is also just a regular worker; just bury it when it dies." The kernel will automatically release its stack space and resources upon exit, without the main thread worrying about it.


* **05_server_noblock.c**
* In this file, we attempt to set the socket to non-blocking (Non-blocking). This is the first step towards high-performance IO (Epoll/IOCP).
```c
// Get current flag
int flag = fcntl(sockfd, F_GETFL, 0);
// Set new flag = old flag + non-blocking bit
fcntl(sockfd, F_SETFL, flag | O_NONBLOCK, 0);


```


* **Bitwise Operation Diagram**:
* `fcntl` manages state via bitmasks.
* `flag` (Hypothetical): `0000 0010` (Existing attributes)
* `O_NONBLOCK`: `0000 0100` (Non-blocking attribute)
* `|` (OR) Operation: `0000 0110` (Possesses both attributes)
* **The Cost of Non-blocking (Errno)**:
* When a socket is non-blocking, if the `recv` buffer has no data, it won't get stuck but will immediately return `-1`.
* At this time, you must check `errno`. If `errno == EAGAIN` (Try again) or `EWOULDBLOCK`, it means **"No data right now, not an error, come back later"**. This allows the program to do other things when there is no data.


* **06_server_epoll.c**
* **Epoll**: The most efficient IO multiplexer on Linux. It solves the inefficiency of `select/poll` polling all sockets.
```c
extern int epoll_create1 (int __flags) __THROW;

```


* Create an epoll instance (Red-Black Tree root node), returns handle `epfd`.
```c
struct epoll_event {
    uint32_t events;  /* Epoll events */
    epoll_data_t data; /* User data variable */
} __EPOLL_PACKED;

```


* **Core Parameters**:
* `events`: Events of interest.
* `EPOLLIN`: Data available to read (including new connections).
* `EPOLLET`: **Edge Triggered**. Notifies only once when data arrives; if you don't finish reading, it won't remind you next time (Efficient but hard to write). Default is **LT (Level Triggered)**, which keeps reminding if not fully read.
* `data`: Contains multiple data structures; here we use the file descriptor.
* `data.fd`: Records which socket the event occurred on.
```c
extern int epoll_ctl (int __epfd, int __op, int __fd,
        struct epoll_event *__event) __THROW;

```


* **Operation Type (`__op`)**:
* `EPOLL_CTL_ADD`: Register new socket.
* `EPOLL_CTL_MOD`: Modify monitored events.
* `EPOLL_CTL_DEL`: Remove socket.
```c
extern int epoll_wait (int __epfd, struct epoll_event *__events,
        int __maxevents, int __timeout)

```


* **Event Loop Logic**:
* `epoll_wait` blocks and waits. Once sockets are ready, it fills the ready sockets into the `__events` array and returns the count `n`.
* We only need to iterate through these `n` active sockets, not all 10,000 sockets.
* **Branching Processing**:
* If `events[i].data.fd == listen_fd`: Means new connection -> Call `accept` -> `epoll_ctl(ADD)` to add to monitoring.
* Otherwise: Means an established client sent data -> Call `recv/send` to handle business.


* **adding**
* **Summary**: Epoll implements high concurrency with a single thread, avoiding the overhead of frequent Context Switching in multi-threading. However, if the business logic is very time-consuming (e.g., computationally intensive), the single thread will get stuck.
* **Foreshadowing for Go**: The Go language's Goroutines actually combine the "ease of use of multi-threading" with the "high performance of Epoll"—using Epoll at the bottom layer for listening, and lightweight coroutines at the top layer masquerading as blocking IO. We will see this genius design in subsequent parts.


## **netpoll-underlying-go**

### **The Goal of This Part**

* In the previous section, we explored the underlying implementation of `netpoll` (network polling) in C. In this section, we will turn our attention to Go, deeply analyzing the practical application and ingenious design of `netpoll` in the Go language. Before we proceed, if you are not yet familiar with basic concepts like "programs" and "processes," I highly recommend watching [this educational video by Core Dumped](https://www.youtube.com/watch?v=7ge7u5VUSbE [[00:46](http://www.youtube.com/watch?v=7ge7u5VUSbE&t=46)]) to solidify the necessary low-level computer knowledge.
* Below is a typical Go network programming code snippet. By dissecting the execution mechanism of this code, we will gradually uncover the implementation principles of Go's underlying network model.
  ```go
  package main

  import (
      "fmt"
      "net"
  )

  func main() {
      // 1. Listen on local port 8080
      listener, err := net.Listen("tcp", ":8080")
      if err != nil {
          panic(err)
      }
      defer listener.Close()
      fmt.Println("Server is running on :8080...")

      for {
          // 2. Block and wait for new client connections
          conn, err := listener.Accept()
          if err != nil {
              fmt.Println("Accept error:", err)
              continue
          }
          // 3. Start an independent Goroutine to handle each connection
          go handleConnection(conn)
      }
  }

  func handleConnection(conn net.Conn) {
      defer conn.Close()
      buf := make([]byte, 1024)

      for {
          // 4. Read data sent by the client
          n, err := conn.Read(buf)
          if err != nil {
              fmt.Println("Connection closed or read error")
              return
          }
          // 5. Write the read data back exactly as is (Echo Server)
          _, err = conn.Write(buf[:n])
          if err != nil {
              fmt.Println("Write error:", err)
              return
          }
      }
  }

  ```



---

### **Sample Code Dissection & Overview of the Underlying `netpoll` Mechanism**

Although this code appears to be written in a very simple "synchronous blocking" style, thanks to the encapsulation by the Go runtime, it actually operates as highly efficient **asynchronous non-blocking I/O** at the lowest level. Let's break it down step-by-step in conjunction with `netpoll`:

#### **1. `net.Listen("tcp", ":8080")`: Initialization and Low-Level Registration**

* **Surface Logic**: Creates a TCP listener bound to port 8080.
* **Underlying Mechanism**: At this stage, the Go runtime does more than just call the low-level `socket()` and `bind()` system functions. More importantly, it sets this listening socket to **Non-blocking** mode and registers its File Descriptor (FD) with the operating system's event poller (e.g., `epoll` in Linux, `kqueue` in macOS).  This serves as the entry point for the `netpoll` mechanism in Go.

#### **2. `listener.Accept()`: Suspending and Waking Up Goroutines**

* **Surface Logic**: The program "gets stuck" (blocks) here until a new client connection arrives.
* **Underlying Mechanism**: Because the underlying socket is non-blocking, if there are no new connections, the low-level `accept` system call will immediately return an error (such as `EAGAIN`). At this point, Go's `netpoll` mechanism steps in: it will **suspend (Park)** the current Goroutine, releasing the CPU thread to execute other tasks.  It is only when the underlying `epoll` detects a new connection arriving at this port that `netpoll` will **wake up (Ready)** this suspended Goroutine to continue execution. This design allows even a single-core CPU to support an extremely high concurrency of waiting connections.

#### **3. `go handleConnection(conn)`: The Classic Goroutine-per-Connection Model**

* **Surface Logic**: After acquiring a new connection, a new Goroutine is launched to exclusively serve this client, while the main loop goes back to execute `Accept()` and wait for the next incoming connection.
* **Underlying Mechanism**: This is the most central design pattern in Go network programming. Compared to C/C++, where handling concurrency requires manually writing complex callback functions or state machines, Go achieves simple concurrency through extremely lightweight Goroutines (which initially consume only 2KB of memory). Thousands of connections correspond to thousands of Goroutines, all efficiently managed by the Go Scheduler.

#### **4. `conn.Read(buf)` and `conn.Write()`: Synchronous Code, Asynchronous Soul**

* **Surface Logic**: Inside the independent Goroutine, it continuously loops to read data sent by the client. If no data is received, `Read` will block.
* **Underlying Mechanism**: The blocking logic here is exactly the same as in `Accept()`. When there is no data to read in the buffer, this read operation triggers the Go scheduler to suspend the current `handleConnection` Goroutine and registers this Socket into `netpoll`. Once the client actually sends network data and the operating system's network stack finishes receiving it, `epoll` triggers an event. Go's background network polling thread will then put this Goroutine back into the runnable queue, and the code immediately "wakes up" from `conn.Read` and continues execution.

**Summary**: This code perfectly demonstrates the ingenuity of Go's design—**writing a high-performance asynchronous non-blocking server driven by `epoll` + `Goroutines` under the hood, using the simplest synchronous code logic**. Developers do not need to worry about complex file descriptor polling or state machine switching. All the "dirty work" is encapsulated within Go's runtime network multiplexer (`netpoll`). In the following steps, we will deconstruct this entire logical architecture from top to bottom.


### **Internal Calls of the listen Function**

#### 1. Creating the Socket

* If we trace down the `listen` function all the way, we will find that the core entry point is the [socket](./02_go_sdk/go/src/net/sock_posix.go#L18) function. This function is the instance where Go creates a socket at the lowest level. Similar to the `socket()` system call we used in C, it also returns a corresponding File Descriptor.
  ```go
  // Brief overview of the core logic
  s, err := sysSocket(family, sotype, proto)
  // ......
  err = setDefaultSockopts(s, family, sotype, ipv6only)
  // ......
  // Dispatch logic based on socket type
  switch sotype {
  case syscall.SOCK_STREAM, syscall.SOCK_SEQPACKET:
      // If it is a stream socket (TCP), enter listenStream
      if err := fd.listenStream(ctx, laddr, listenerBacklog(), ctrlCtxFn); 
  // ......
  }
  ```


* **Low-level Interaction and Self-hosting**: Here I extracted the core logic of `sysSocket`. It calls assembly commands encapsulated at the lowest level. It is worth mentioning a significant feature that distinguishes Go from interpreted languages like PHP: **Go is self-hosted**—Go itself is written in Go. This means Go has its own assembly code and a corresponding `cmd` compiler directory. When we trace down to Go's bottom layer, we can clearly see the boundary between Go code and assembly code.
* **Parameter Configuration**: Macroscopically, this function is equivalent to the combination of `socket()` and `setsockopt()` we used in C. `sysSocket` is responsible for creation, while `setDefaultSockopts` handles setting the basic properties.
* **Role Definition**: The final form of the Socket (is it a client or a server? Stream transmission or datagram?) is ultimately determined by the arguments passed from the upper layer. In this example, unlike the `Dial` operation for a client, the `Listen` operation clearly defines the current machine's role as a **server** by binding to a local port (like `:8080` in the code).

#### 2. Building Stream Sockets: `listenStream`

* Inside the function, the creation of stream sockets (TCP) and datagram sockets (UDP) are encapsulated into different paths. Taking the stream socket we focus on as an example, let's see what [listenStream](./02_go_sdk/go/src/net/sock_posix.go#L150) does:
  ```go
  // 1. Set the listener's default Socket options
  setDefaultListenerSockopts(fd.pfd.Sysfd)
  // ...
  // 2. Convert the address into a system-recognized sockaddr struct
  // ...
  // 3. Apply user-defined Socket properties
  // ...
  // 4. Bind the port (corresponding to bind in C)
  syscall.Bind(fd.pfd.Sysfd, lsa)
  // 5. Start listening (corresponding to listen in C)
  listenFunc(fd.pfd.Sysfd, backlog)
  // 6. Initialize the file descriptor (Critical step!)
  fd.init()
  ```


* **The Shadow of C**: The creation logic for datagram sockets is similar, just with added checks for multicast addresses. It is obvious that this Go workflow perfectly replicates the standard tri-step of `socket` -> `bind` -> `listen` we executed in our C code. This reveals that Go's networking bottom layer is still based on the encapsulation of standard OS socket mechanisms.

#### 3. Entering Netpoll: `fd.init`

* The previous steps are no different from the logic we implemented in C, but starting from [fd.init](./02_go_sdk/go/src/internal/poll/fd_unix.go#L55), we will enter Go's unique realm of magic—**Netpoll (Network Poller)**.
* The main function of this method is to determine whether the current file descriptor belongs to a network file (i.e., not a regular file). If it is, it initializes the network polling mechanism for it:
  ```go
  // Initialize pollDesc (poll descriptor)
  fd.pd.init(fd)
  ```



#### 4. The Intersection of Runtime and Network: `pd.init`

* The [init](./02_go_sdk/go/src/internal/poll/fd_poll_runtime.go#L38) function is the critical intersection between Go's standard `net` library and the `runtime` package, serving as the cornerstone for "synchronous code, asynchronous execution."
  ```go
  func (pd *pollDesc) init(fd *FD) error {
      // 1. Ensure the global network poller is initialized only once
      serverInit.Do(runtime_pollServerInit)

      // 2. Register the file descriptor into the poller (underlying maps to epoll_ctl/kqueue, etc.)
      ctx, errno := runtime_pollOpen(uintptr(fd.Sysfd))
      if errno != 0 {
          return errnoErr(syscall.Errno(errno))
      }

      // 3. Save the context
      pd.runtimeCtx = ctx
      return nil
  }
  ```


* **`serverInit.Do`**: Utilizing the `sync.Once` mechanism, it ensures that throughout the entire program's lifecycle, the global network poller is initialized only once.
* **`runtime_pollOpen`**: This is the most crucial step. It registers the Socket file descriptor (fd) we previously created into the underlying I/O multiplexer (for example, calling `epoll_ctl` to add events like `EPOLLIN` under Linux). If registration fails, it usually indicates exhausted system resources or an abnormal environment.

---

**Summary**:
In this section, we verified that the first half of Go's low-level Socket encapsulation is highly consistent with the logic in our [C implementation (socket-underlying-c)](#1socket-underlying-c).
However, the watershed appears after `fd.init`—Go doesn't stop at just creating the Socket; instead, it seamlessly integrates it into the Netpoll system via the `runtime`. In the next section, we will further explore this massive network processing system built around Netpoll, and how it perfectly combines with Go's multiple coroutines (Goroutines) through the scheduler.



### The `netpoll` Network Architecture: Deep Dive into the Runtime

Continuing our previous train of thought, we have mapped out the high-level logic in the `internal/poll` package. Now, we will shift our focus from the surface to the core, officially descending into Go's central `runtime` package.

Before crossing this boundary, we must reiterate a core mechanism: the seemingly inconspicuous line of code `pd.runtimeCtx = ctx` mentioned earlier. This is actually the crowning touch of Go's Network Poller (`netpoll`) design:

* **A Two-Sided Unified Structure**: There is a `pollDesc` struct in both the `internal` package and the `runtime` package. Logically, they correspond one-to-one, like two "faces" of a single entity.
* `internal/poll.pollDesc`: Faces the user layer, handling general logic such as the lifecycle of file descriptors and read/write timeouts.
* `runtime.pollDesc`: Faces the bottom layer, bearing the specific state for interacting with the operating system kernel (e.g., epoll/kqueue).
* **Pointer "Smuggling" and Bridging**: The operation `pd.runtimeCtx = ctx` essentially "smuggles" the struct pointer from the `runtime` level (in the form of `uintptr`, which is generic and untracked by the GC) and encapsulates it into the struct at the `internal` level.
* **Layer Decoupling and Business Coupling**: This design allows the upper `internal` package to simply pass this "handle" back when performing low-level operations, enabling the `runtime` to instantly retrieve its corresponding kernel-mode context. Go uses this **non-intrusive** approach to achieve strict encapsulation of different business layers while ensuring highly efficient coupling when necessary.

#### 1. The Low-Level Defense Line of Netpoll Creation: Double-Checked Locking

When tracing the initialization flow of `netpoll` in [netpollGenericInit](https://www.google.com/search?q=./02_go_sdk/go/src/runtime/netpoll.go%23L218), we can see a classic piece of concurrency control code. To ensure that `netpoll` is initialized only once in a highly concurrent, multi-threaded scenario, Go adopts the **Double-Checked Locking** pattern:

1. **First Check (Lock-Free Check)**: It first uses an atomic operation (Atomic Load) to quickly check the `netpollInited` flag. If already initialized, it returns directly, avoiding expensive lock overhead.
2. **Lock**: If not initialized, it acquires the global lock and enters the critical section.
3. **Second Check (Locked Check)**: It checks the flag again. This prevents another thread from completing the initialization during the tiny time window between the "first check" and "acquiring the lock."
4. **Init (Execute Initialization)**: Only after passing these two defense lines will it truly call the platform-specific `netpollinit()`.

This rigorous logical loop ensures absolute thread safety for the Netpoller during high-concurrency startups.

#### 2. Passing Downward: The Art of Assembly and System Calls

Going deeper, the code takes us into the realm of assembly language. Although we don't need to delve into every assembly instruction, two design philosophies here deserve our special attention:

* **The "Autopilot" of Multiplexers and Shielding Differences**
Go follows the philosophy of "write once, compile anywhere." At the source code level, Go uses Build Tags to provide different implementation files for different operating systems (e.g., `netpoll_epoll.go` for Linux, `netpoll_kqueue.go` for macOS).
The upper logic doesn't need to care whether the bottom layer is `epoll`, `kqueue`, or `IOCP`; the Go Runtime automatically links the corresponding low-level operation set based on the target compilation platform. Through this encapsulation that **shields differences**, the user layer experiences a unified asynchronous I/O experience, and the name `netpoll` itself is a highly abstract representation of all these multiplexing technologies.
* **Direct to the Kernel: Syscall6 and Register Operations**
In Go 1.19 and later versions, specifically under the `internal/syscall/unix` path, Go exposed low-level interfaces like `Syscall6`. Such a generic assembly call interface demonstrates Go's self-implemented characteristics, distinguishing it from languages like Java or PHP.
* **Register-Level Operations**: This is actually Go's springboard for jumping from "user space" to "kernel space." As discussed in the previously provided video ([Core Dumped's educational video](https://www.youtube.com/watch?v=7ge7u5VUSbE [[00:46](http://www.youtube.com/watch?v=7ge7u5VUSbE&t=46)])), system calls essentially involve loading parameters into specific CPU registers (like RAX, RDI, RSI, etc.) and then triggering a software interrupt (Trap) to request operating system kernel intervention.
* **Zero-Cost Encapsulation**: Go does not rely on a massive C standard library (libc) here; instead, it directly encapsulates these OpCodes through assembly code. This not only reduces the binary size but, more importantly, helps developers avoid complex register management, providing a system call interface that is close to the hardware's maximum speed while ensuring type safety.




#### 3. Netpoll's File Integration and the pollDesc Lifecycle

* **Core Entry Point:** `poll_runtime_pollOpen`
In [poll_runtime_pollOpen](./02_go_sdk/go/src/runtime/netpoll.go#L244), we can see how `netpoll` brings the low-level network file descriptor (FD) under the Runtime's supervision. This function plays a transitional role:
  ```go
  // Pseudocode logic overview
  func poll_runtime_pollOpen(fd uintptr) (*pollDesc, int) {
      // 1. Get or allocate a new pollDesc from the cache pool
      pd := pollcache.alloc()

      // 2. Initialize pollDesc, this step is critical
      // Must ensure that no other goroutine is reading, writing, or waiting on this pd
      // This sets pd.fd = fd and generates a new sequence number fdseq
      lock(&pd.lock)
      if pd.wg != 0 && pd.wg != pdReady {
          throw("runtime: blocked write on free polldesc")
      }
      ...
      unlock(&pd.lock)

      // 3. Call the platform-specific implementation (e.g., epoll/kqueue) to register the fd to the kernel
      errno := netpollopen(fd, pd)
      return pd, errno
  }
  ```


* **State Checking and Version Control**: The initialization here is more than just assignment. `pollDesc` instances will be reused (the caching mechanism will be detailed later), so it must ensure that the acquired `pd` is "clean" and has no residual Goroutines waiting on it.
* **Concurrency and Version Numbers**: To prevent race conditions, locking operations are used here. More importantly, `fdseq` (file descriptor sequence number) is introduced. This is an extremely crucial design used to solve the **ABA problem**: it prevents a new Socket from reusing the same FD and the same `pollDesc` after an old Socket is closed, which would cause an old event to mistakenly wake up a new connection.

#### 4. Deep into the Bottom Layer: netpollopen and Tagged Pointer Magic

Continuing deeper into [netpollopen](./02_go_sdk/go/src/runtime/netpoll_epoll.go#L49) (using Linux Epoll as an example), there are two low-level optimizations highly characteristic of Go:

**1. Edge Triggered (ET) Mode**:

* The code sets `ev.Events = syscall.EPOLLIN | syscall.EPOLLOUT | syscall.EPOLLRDHUP | syscall.EPOLLET`.
* Go unhesitatingly chose `EPOLLET` (Edge Triggered), which aligns with our advanced practices in C network programming.
* **Reason**: ET mode only notifies once when the state changes, reducing the number of `epoll_wait` returns and significantly lowering the frequency of system calls. This is the cornerstone of building a high-performance network library.

**Tagged Pointer (Pointer Compression Technology)**:

* This is an incredibly ingenious trick. In the `epoll_data` union of `epoll_ctl`, we only have a 64-bit space to store the context. If we only store the `pd` pointer, we cannot carry the `fdseq` version number; if we only store the FD, we cannot quickly find the `pd` object.
Go's solution is to compress both the **pointer address** and the **version number** into the same `uintptr`:
* **Low-Bit Utilization**: Due to memory alignment on 64-bit machines (typically 8-byte alignment), the last 3 bits of a pointer are always 0.
* **High-Bit Utilization**: Although pointers are 64-bit, modern CPUs (like AMD64) typically only use the lower 48 bits for addressing (virtual address space limitations).
* **Packing Logic**: Go utilizes these "unused" bits (combining the high bits and the space squeezed out via bitwise operations to gather 10 bits capable of holding 1023 version numbers) to embed `fdseq` into it.
* **Validation**: When in use, disassembling this Tagged Pointer accurately restores the `pd` memory address while extracting the version number to compare against the current `pd`'s version number, thereby perfectly detecting if the data has expired or leaked.

Finally, registration with the kernel is completed via `syscall.EpollCtl`.

#### 5. pollDesc Memory Management: Efficient Reuse and GC Isolation

Returning to the beginning of [poll_runtime_pollOpen](./02_go_sdk/go/src/runtime/netpoll.go#L244), let's explore how `pollDesc` is created and managed.

* **Batch Allocation and Linked-List Caching**:
The Go Runtime deeply hates frequent small object memory allocations. Therefore, `pollDesc` is managed using a global `pollCache` linked list:
  ```go
  // Pseudocode
  lock(&c.lock)
  if c.first == nil {
      // Cache is empty, call persistentalloc to allocate a batch at once (e.g., 4KB in size)
      // and string them into a linked list
  }
  pd := c.first
  c.first = pd.link
  unlock(&c.lock)
  ```


Acquiring and freeing are merely simple linked-list pointer operations; the overhead is almost negligible.
* **PersistentAlloc and GC Isolation**:
Here, `persistentalloc` is used to allocate memory instead of a regular `new`.
* **Non-GC Memory**: This block of memory is marked as "persistent," meaning Go's Garbage Collector (GC) **will not scan** this memory region.
* **Performance Considerations**: `pollDesc` is an internal struct used by the Runtime; it does not contain pointers to Go heap objects (except for weak references), and its lifecycle is manually managed by the Runtime. Excluding it from GC scanning massively reduces the GC's workload (overhead during the Mark phase). This is one of the invisible heroes enabling Go to support millions of concurrent connections.
* **Address Stability**: This also guarantees that the physical memory address of `pollDesc` will not move, which is absolutely vital when passing its address to the operating system kernel (like Epoll).



#### 6. pollDesc and pollCache: Core Data Structure Analysis

**1. pollCache: Linked-List Memory Pool**:

* First, let's review the [pollcache](./02_go_sdk/go/src/runtime/netpoll.go#L192) struct. It does not directly participate in data transmission during actual network business logic; rather, it plays the role of a **Memory Pool** or **Free List**.
* **Structure Definition**: It is essentially a singly linked list head protected by a lock.
  ```go
  type pollCache struct {
      lock  mutex
      first *pollDesc // Points to the first node in the free list
  }
  ```


* **Architectural Significance**:
* **Reuse Mechanism**: When a network connection closes, its corresponding `pollDesc` is not immediately freed back to the operating system but is recycled into this linked list.
* **Performance Optimization**: In scenarios with high-frequency short connections, this design avoids frequent calls to `persistentalloc` and GC pressure. Getting a `pollDesc` is just a simple pointer operation, making it extremely fast.



**2. pollDesc: The Heart of Netpoll**

* [pollDesc](./02_go_sdk/go/src/runtime/netpoll.go#L75) (Polling Descriptor) is the most complex struct in the entire `netpoll` system. It is the "shadow object" at the Go Runtime level corresponding to the low-level network file descriptor.
* To clearly understand its responsibilities, we can divide its fields into four major functional modules:
**1. Identity & Linkage**
* `link *pollDesc`: Linked list pointer. When this object is in the `pollcache`, it points to the next free node; when active, this field is usually nil.
* `fd uintptr`: **Core identity indicator**. This is the raw Socket File Descriptor allocated by the operating system. It is exactly this value that is registered into epoll/kqueue.
* *Note*: In `netpollopen` earlier, we wrote the pointer address of `pollDesc` (encapsulated via Tagged Pointer) into `epoll_event.data`, achieving reverse mapping from kernel events back to the Go Runtime object.


**2. Data Protection (Concurrency Control)**
* `lock mutex`: Mutex lock. Used to protect the atomicity of `pollDesc`'s own state, preventing multiple Goroutines from operating on the same FD simultaneously (e.g., concurrent reads/writes or concurrent closing).
* `atomicInfo atomic.Uint32`: Atomic state bits. Used to quickly determine the current FD's status (e.g., whether it is closed or interrupted), enabling lock-free rapid checks.


**3. Scheduler Coupling (Scheduler Integration) — The Most Critical Design**
This is the core of Go's implementation of "synchronous semantics, asynchronous foundation". `pollDesc` contains two key fields:
* `rg uintptr` (Read Group / Read G)
* `wg uintptr` (Write Group / Write G)
* These two fields act as a lightweight **state machine**; their values are not just simple 0s or 1s, but encompass the following three states:
* **`0` (pdNil)**: Idle state. Currently, no Goroutine is waiting on read/write events for this FD.
* **`pdReady` (1)**: Ready state. Indicates Epoll has notified the Runtime that this FD is readable or writable. At this point, a Goroutine calling Read/Write will not block but will directly execute the system call.
* **`pdWait` (2)**: Waiting state. Indicates the Goroutine is preparing to suspend.
* **`> 2` (G Pointer)**: **This is the real magic.** When a Goroutine needs to block because I/O is not ready, it writes **its own address (`*g`)** here. When Epoll wakes up, Netpoll reads this address and directly tosses the corresponding Goroutine back into the scheduler's Run Queue.




**4. Timeout Control (Deadline Management)**
* `rt timer` (Read Timer) / `wt timer` (Write Timer): Timers at the Go language level.
* `seq uintptr` (Sequence): A globally unique sequence number.
* **Mechanism**: When we call `SetReadDeadline`, it actually registers an event into Go's heap timer. If the timer triggers and I/O is still incomplete, the Runtime will compare the `seq` to ensure this is a timeout for the current operation, then forcibly wake up the Goroutine blocked on `rg/wg` and return an `i/o timeout` error.



**Summary**:
`pollDesc` ingeniously tightly couples the low-level **IO resource** (FD), the middle-layer **IO state** (rg/wg state machine), and the upper-layer **scheduling entity** (Goroutine address) through a single struct. This enables Go, with O(1) complexity, to instantly find and wake up the correct Goroutine the moment a kernel notification event arrives.


### The Underlying Implementation of Accept

In the previous two sections, we deeply explored the underlying system call implementation of `listen`. Before formally diving into `accept`, let's do a bottom-up review of this function's return value encapsulation to clarify how Go exposes underlying resources to the user layer.

First, the part that interfaces with our C-language perspective is the `socket` file descriptor. This raw file descriptor is encapsulated by the upper-level `listenTCPProto`, ultimately becoming the [TCPlistener](./02_go_sdk/go/src/net/tcpsock.go%23L291) struct. This struct is not just a container for the file descriptor; it also embeds the [listenConfig](./02_go_sdk/go/src/net/dial.go#L672) struct. `listenConfig` contains configurations like the `control` hook function and KeepAlive probe intervals. Its significance lies in **opening up the operating system's low-level network parameter settings to the user layer**, allowing developers to further restrict and specify the behavior of the underlying `socket` through configuration fields at the Go language level.

Ultimately, through struct definitions and the method encapsulation of `tcplistener`, Go tightly "couples" the underlying `listen` system call with the upper-level configuration struct. This relationship is already determined when we call `net.Listen` and pass in the `network` and `address` fields.

It is worth noting that the `listen` interface is primarily used for streaming and connection-oriented protocols (like `tcp`, `ssl`, etc.); parallel to it is the `PacketConn` interface, used for datagram protocols (like `udp`, `dns`, etc.). This design perfectly embodies Go's philosophy of **interface-oriented programming**, hiding the implementation details of different protocols behind unified interfaces, thereby achieving code modularity and componentization.

Now that we understand the construction process of the `Listener`, the `Accept` method we call in our business code is essentially linked to `func (ln *TCPListener) accept()` via interfaces. Next, we will start from the upper-level entry point of [accept](./02_go_sdk/go/src/net/tcpsock_posix.go#L158) and dissect its underlying implementation principles top-down.

#### 1. The Entry to the Network Poller: fd_unix

* First, the code logic arrives at the [accept](./02_go_sdk/go/src/net/fd_unix.go#L171) function under the `internal/poll` package. This function is the critical hub for interactions between the network poller (`Netpoller`) and the socket. It primarily accomplishes the following four core functions:
1. **Invoke System Call**: Encapsulates and executes the underlying `accept` system call.
2. **Object Wrapping**: Wraps the newly returned file descriptor into a `netFD` (Network File Descriptor) object.
3. **Register for Polling**: Registers the new `netFD` into `epoll` (or the equivalent I/O multiplexer for the specific platform) to listen for subsequent read/write events.
4. **Populate Information**: Fills in the remote and local address information.


* Let's focus on the core logic of [accept](./02_go_sdk/go/src/internal/poll/fd_unix.go#L594) in `internal/poll/fd_unix.go`:
  ```go
  func (fd *FD) Accept() (int, syscall.Sockaddr, string, error) {
      // ... Preparation and locking ...

      // Loop to try Accept
      for {
          // Execute the underlying system call accept
          s, rsa, errcall, err := accept(fd.Sysfd)

          // 1. If successful, return directly
          if err == nil {
              return s, rsa, "", err
          }

          // 2. Handle errors returned by the system call
          switch err {
          case syscall.EINTR:
              // Signal interrupt, retry
              continue
          case syscall.EAGAIN:
              // Core logic: EAGAIN means the current socket's receive buffer is empty (no new connection)
              // If fd is pollable, suspend the current Goroutine and wait for a read event
              if fd.pd.pollable() {
                  if err = fd.pd.waitRead(fd.isFile); err == nil {
                      continue // After being woken up, continue the loop to try accept
                  }
              }
          case syscall.ECONNABORTED:
              // This error usually means the connection was reset by the peer during establishment; ignore and retry
              continue
          }
          return -1, nil, errcall, err
      }
  }
  ```


* The non-blocking I/O approach here is completely consistent with the logic we implemented in the C language [netpoll](./01_webcoding_based_on_c/05_tcp/06_server_epoll.c) section: when `accept` returns `EAGAIN`, do not put the thread to sleep; instead, yield the CPU.
* **Key Point**: The brilliance of Go lies in the fact that when encountering `EAGAIN`, it calls `fd.pd.waitRead` to suspend the current **Goroutine**, rather than blocking the operating system thread.

#### 2. The Core of Encapsulation: newFD

* After the `accept` system call successfully returns and obtains the file descriptor, Go does not use this bare `int` handle directly. Instead, it wraps it into a feature-rich object via the [newFD](./02_go_sdk/go/src/net/fd_unix.go#L26) function.
* This function is more than just simple memory allocation; it defines how the Go runtime views this network connection:
* **poll.FD**: This is the absolute core, acting as the bridge between Go's I/O layer (user code) and the Runtime layer (scheduler).
* **Sysfd**: Saves the underlying socket handle; all operations ultimately boil down to this integer.
* **IsStream**: A flag indicating whether it is a stream socket.
* If it is TCP, this value is `true`.
* If it is UDP, this value is `false`.


* **ZeroReadIsEOF**: This is a critical rule for determining termination.
* As we mentioned in the [udp](./01_webcoding_based_on_c/02_udp/04_recvfrom.c) section, UDP allows sending 0-byte packets, which does not signify connection closure in UDP.
* However, in TCP, a `read` returning 0 bytes usually means the peer has sent a FIN packet (EOF), and the connection needs to be closed.
* `newFD` automatically sets this field based on the value of `IsStream`, ensuring the upper-level business logic can correctly handle the meaning of "reading 0 bytes."




* **Concurrency Safety**: The object initialized by `newFD` internally maintains a read/write lock (`fdMutex`). This is the underlying guarantee that allows Go to safely permit multiple Goroutines to perform concurrent operations on the same socket (although it is generally not recommended to do so).

#### 3. Registration for Polling: init

* Immediately after wrapping is complete, the connection is "activated." This step is handled by [init](./02_go_sdk/go/src/internal/poll/fd_unix.go#L55).
* The logic of this function is entirely consistent with the approach we saw earlier in [listen](#internal-calls-of-the-listen-function); you could say **all roads lead to Rome**:
* The **Listen** phase registers the listening socket (`listener`) into `epoll` to listen for the "new connection arrival" event.
* The **Accept** phase registers the newly established connection socket (`conn`) into `epoll` to listen for "data readable/writable" events.


* Under the hood, both ultimately call `poll.runtime_pollOpen` to add the `Sysfd` into the red-black tree of the `epoll` instance. Once this step is complete, the connection officially enters the management scope of Go's Network Poller (`Netpoller`), laying the foundation for subsequent asynchronous I/O.

#### 4. Deep into Runtime: poll_runtime_pollWait

* Diving further into the `waitRead` function, we eventually cross from the `internal/poll` package into the `runtime` package's [poll_runtime_pollWait](./02_go_sdk/go/src/runtime/netpoll.go#L336) via the `//go:linkname` linking mechanism.
* Note that at this point, our business struct has transformed from `poll.FD` into the runtime's internal `polldesc` (poll descriptor). This transformation relies on the mapping relationship between the structs.
  ```go
  func poll_runtime_pollWait(pd *pollDesc, mode int) int {
      // 1. Check if the connection has already errored or closed
      errcode := netpollcheckerr(pd, int32(mode))
      if errcode != pollNoError {
          return errcode
      }

      // 2. Loop and wait until netpollblock returns true (indicating IO is ready)
      for !netpollblock(pd, int32(mode), false) {
          // Check for errors again after being woken up
          errcode = netpollcheckerr(pd, int32(mode))
          if errcode != pollNoError {
              return errcode
          }
          // If woken up due to a timeout, but the timeout was reset before it had a chance to run,
          // pretend it didn't happen and retry.
      }
      return pollNoError
  }
  ```


* Before truly suspending, the runtime first checks for timeout or closure errors via [netpollcheckerr](./02_go_sdk/go/src/runtime/netpoll.go%23L512).
* The `for` loop logic here handles edge cases (e.g., a timeout triggers but is then reset), preventing state inconsistencies caused by concurrent modifications, and ensuring that every suspension is valid.

#### 5. Core Blocking Logic: netpollblock

* Next, we delve into [netpollblock](./02_go_sdk/go/src/runtime/netpoll.go%23L548), the decision center for Goroutine suspension.
  ```go
  func netpollblock(pd *pollDesc, mode int32, waitio bool) bool {
      // Choose whether to operate on the read channel (rg) or write channel (wg) based on the mode
      gpp := &pd.rg
      if mode == 'w' {
          gpp = &pd.wg
      }

      for {
          // CAS operation: Atomically check the state

          // Case 1: If the state is already pdReady (IO is ready), reset the state to pdNil and return true
          // Indicates no waiting is needed; proceed directly to read/write data
          if gpp.CompareAndSwap(pdReady, pdNil) {
              return true
          }

          // Case 2: If the state is pdNil (initial state), set it to pdWait (waiting)
          // Only if the setting is successful can it break out of the loop and execute gopark to suspend
          if gpp.CompareAndSwap(pdNil, pdWait) {
              break
          }

          // Case 3: If it's neither Ready nor Nil, it means there's a concurrent wait; throw an exception
          if v := gpp.Load(); v != pdReady && v != pdNil {
              throw("runtime: double wait")
          }
      }

      // Execute the suspend operation
      // Pass in netpollblockcommit as a callback; it will execute on the g0 stack
      if waitio || netpollcheckerr(pd, mode) == pollNoError {
          gopark(netpollblockcommit, unsafe.Pointer(gpp), waitReasonIOWait, traceBlockNet, 5)
      }

      // Logic after being woken up
      old := gpp.Swap(pdNil)
      if old > pdWait {
          throw("runtime: corrupted polldesc")
      }
      return old == pdReady
  }
  ```


* **State Machine Management**: `pd.rg` and `pd.wg` are atomic `uintptr`s. As mentioned earlier in the `polldesc` section, their default value is `pdNil` (0).
* **CAS (CompareAndSwap)**: This utilizes CAS to achieve lock-free state transitions. It integrates "value checking" and "value swapping" into one atomic step, ensuring safety in a multi-threaded environment.
* **Suspend and Wake Up**: `gopark` is the watershed. Before executing `gopark`, the current Goroutine is running; when `gopark` returns, it means the Goroutine has been woken up by an `epoll` event. At this point, it checks whether the return value is `pdReady` to confirm that it was woken up by a data packet and not by a timeout.

#### 6. Scheduler Context Switch: gopark and the g0 Stack

* Finally, we analyze the lowest-level scheduler interface, [gopark](./02_go_sdk/go/src/runtime/proc.go#L443). This is the key to a Goroutine yielding CPU control.
  ```go
  func gopark(unlockf func(*g, unsafe.Pointer) bool, lock unsafe.Pointer, reason waitReason, traceEv byte, traceskip int) {
      // ...
      mp := acquirem() // 1. Forbid the current M from being preempted
      gp := mp.curg
      // ... State checks ...

      // 2. Save the current Goroutine's context (SP, PC, etc.)
      // 3. Switch to the g0 stack to execute the park_m function
      mcall(fastrand) 
      // Note: mcall will call park_m, and the code logic jumps to park_m
      // ...
  }
  ```


* **acquirem/release**: `acquirem` essentially increments the lock count of the current M (system thread), preventing M from being preempted by the scheduler or interfered with by GC scanning during sensitive operations like saving context.
* **mcall and g0**: A Goroutine's stack grows and shrinks dynamically, while the suspend operation involves a stack switch. For safety, it must switch to the **g0 stack** (the system stack, which has a fixed and relatively large size) to execute scheduling logic.
* **Callback Function**: The `unlockf` here is the [netpollblockcommit](./02_go_sdk/go/src/runtime/netpoll.go#L529) that we passed in `netpollblock`.
* On the g0 stack, the system executes the [park_m](./02_go_sdk/go/src/runtime/proc.go#L4007) function:
  ```go
  func park_m(gp *g) {
      // ...
      // 1. Modify Goroutine state: from _Grunning to _Gwaiting
      casgstatus(gp, _Grunning, _Gwaiting)

      // 2. Unbind M and G
      dropg()

      // 3. Execute the callback function netpollblockcommit
      if fn := mp.waitunlockf; fn != nil {
          ok := fn(gp, mp.waitlock) // Here, gp's address is written into the pollDesc
          // ...
      }

      // 4. M looks for the next runnable Goroutine
      schedule()
  }
  ```


* [casgstatus](./02_go_sdk/go/src/runtime/proc.go#L1105): Atomically updates the coroutine state from Running to Waiting.
* [dropg](./02_go_sdk/go/src/runtime/proc.go#L4001): Completely unbinds them by setting `mp.curg = nil` and `gp.m = nil`. At this point, G is "sleeping" on the heap, and M is set free.
* **Crucial Callback**: Executes [netpollblockcommit](./02_go_sdk/go/src/runtime/netpoll.go#L529), using `atomic.Store(gpp, gp)` to write the memory address of the current Goroutine into `pd.rg`. **This step is of paramount importance**; it essentially tells the netpoller: "When data arrives for this socket, please wake up the coroutine at address `gp`."
* [schedule](./02_go_sdk/go/src/runtime/proc.go#L3839): M does not rest. It immediately executes `schedule()` to search the global queue or local queue for the next pending G to execute. This is the core secret of Go's high concurrency—**I/O blocks the Goroutines, not the system threads.**

#### Summary

At this point, we have completely deconstructed the entire process of `Accept` from the user-level API down to the Runtime scheduler. This complex call chain perfectly illustrates the core design philosophy of Go: **Synchronous code logic, asynchronous underlying implementation**:

1. **Unification of Appearance and Essence**:
* **To the developer**: `Accept` manifests as standard **blocking I/O**. The code executes linearly and the logic is clear and intuitive, eliminating the need to write complex callbacks or state machines as one would with C's `epoll`.
* **To the operating system**: `Accept` is actually **non-blocking I/O**. Under the hood, through the `EAGAIN` error code and the `epoll` mechanism, it ensures that system threads never block while waiting for network data.


2. **The Relay Between M and G**:
The crux of the entire process lies in the coordination between `gopark` and `schedule`. When I/O is not ready:
* **The Goroutine (G)** chooses to "yield": It saves its context, enters the `_Gwaiting` state, and obediently waits on the heap for data to arrive.
* **The System Thread (M)** chooses to "reuse": By switching to the `g0` stack, it quickly untangles itself from the current G and immediately executes `schedule()` to find the next G that needs CPU time.


3. **The Secret to High Performance**:
This explains why a Go server can support tens of thousands of concurrent connections with only a small number of system threads. **It is because the cost of all "waiting" is borne by extremely cheap Goroutines, while the expensive system threads (M) remain in a state of high-load, effective computation, never truly resting.** This is the ultimate moat that distinguishes Go's network model from traditional multi-threading models.

### Data Transmission and I/O Models

In the previous two sections, we explained the underlying mechanisms of `listen` and `accept`. Before diving into the low-level analysis of `read` and `write`, let's focus on a crucial detail: `go handleConnection(conn)`.

When we use the `go` keyword in our code, the compiler automatically translates it into a call to `newproc`. At this point, the pointer to the target function and the parent goroutine's PC (Program Counter, used for tracing its origin) are saved. Subsequently, the program calls the `systemstack` function to switch to the `g0` system stack and calls `newproc1` to create the execution stack for the new goroutine.

There is an incredibly elegant "fake stack" design in this function: the return address of the new stack is hardcoded to the address of the `goexit` function, ensuring resources are automatically reclaimed after the goroutine finishes executing. Meanwhile, the target execution function's address is also pushed onto the stack.

Therefore, the entity executing `handleConnection` here is a newly born and awakened goroutine. The original parent goroutine, on the other hand, continues its mission, continuously calling `accept` in an infinite loop until the next round of code executes and returns a value. Note that at this point, the file descriptor (FD) of the new connection has already been safely wrapped in the return value of `accept`.

Returning to the `accept` function, what we didn't specifically mention earlier is that the exact return type of this function is `TCPConn`. However, our focus here is not on the specific mechanisms exclusive to the `TCP` protocol, but rather on the universal underlying mechanisms for all network connections—namely, the `write` and `read` methods that implement the abstract `Conn` interface. At this stage, the return value has already perfectly encapsulated all the underlying information required for this connection.

---

#### The Underlying Implementation of Read

* **The GC Defense Line:** First, we look at the `read` method wrapped by the upper-level `netFD`. Aside from delegating the call downwards, this method invokes a vital lifecycle defense line: `runtime.KeepAlive(fd)`. It acts as an "immunity amulet," preventing the garbage collector (GC) from incorrectly reclaiming the object. It strictly ensures that when a goroutine is suspended waiting for data, the corresponding underlying `netFD` is not treated as an orphan object by the GC, which would otherwise automatically trigger the `close` method.
* **The Core Polling Logic:** Moving further down, we reach the `read` method of `internal/poll.FD`. This method is the ultimate underlying implementation of the network read function. Its core polling logic is highly consistent with the `recvfrom` operation we usually see in C. The code skeleton looks like this:
  ```go
  // Business-level locking
  // ...
  // 0-byte return check
  // ...
  // Readiness check
  // ...
  for {
      n, err := ignoringEINTRIO(syscall.Read, fd.Sysfd, p)
      if err != nil {
          n = 0
          if err == syscall.EAGAIN && fd.pd.pollable() {
              if err = fd.pd.waitRead(fd.isFile); err == nil {
                  continue
              }
          }
      }
      err = fd.eofError(n, err)
      return n, err
  }
  ```


* **Business-Level Locking:** It is particularly important to note the "business-level locking" (`readLock`) here. While the `KeepAlive` mentioned earlier prevents the **GC from secretly reclaiming** the connection, the locking here prevents **another business goroutine** (for instance, if the user manually calls `conn.Close()` elsewhere) from mistakenly closing the current file descriptor, which would lead to crossed wires in concurrent reads and writes.
* **Handling Interrupts and Blocking:** Go sets up a universal system call wrapper interface, `ignoringEINTRIO`, to shield against `EINTR` interrupt signals sent by the underlying operating system. This matches the `while` loop fault-tolerance logic often implemented in C. If no blocking occurs, `read` directly reads the data through the system call and returns. If the kernel buffer is empty, causing it to block (triggering `EAGAIN`), it calls `waitRead` to suspend and sleep.
* **Convergence:** Ultimately, we see that whether it's the `read` method reading data or the `accept` method waiting for new connections, when they encounter blocking, they both converge to `poll_runtime_pollWait` at the runtime layer. Furthermore, their wait modes (`mode`) are completely identical (both are treated as waiting for a readable event, `'r'`).

---

#### The Underlying Implementation of Write

At the `poll.FD` level, the error wrapping and throwing logic of the `write` method is basically identical to that of the `read` method. The difference lies in the underlying system call itself and the corresponding block transmission mechanism.

Let's look at the specific method implementation of `write` at the FD level:

```go
// Lock
// ...
// Write preparation
for {
    // Ensure data is successfully written
    // Slice the data if it's too large
}
```

* **Zero-Copy and Data Slicing:** The reason for locking here is exactly the same as in `read`. The `[]byte` slice is passed down from the user business layer all the way here with zero-copy. To protect the operating system's kernel space, Go internally maintains a maximum byte limit for a single system call (`maxRW`). If the incoming data exceeds this limit, it is sliced. Simultaneously, the internal variable `nn` is maintained to record the progress of the bytes already written.
* **Ensuring Delivery:** Go cleverly uses slice offsets (`p[nn:max]`) to chunk the data. It also uses `ignoringEINTRIO` to prevent system signal jitter and strictly relies on this `for` loop to **guarantee that every last drop of data is successfully sent out**. If the operating system's send buffer is full at this time (returning `EAGAIN`), the current writing goroutine will be temporarily suspended.
* **Write Suspension Logic:** Now let's compare the suspension logic of the `write` goroutine. The difference compared to the suspension of `accept` and `read` is that the underlying wait mode `mode` changes to write mode (`'w'`). The only subsequent difference is this: when the underlying `netpollblock` executes, based on this `'w'` mode, it pushes the address of the sleeping goroutine into the write goroutine field (`wg`) of the `pollDesc` structure, rather than the read goroutine field (`rg`).

---

### Summary

In this section, we conducted a step-by-step, top-down deep dive into the principles of our sample code. This exploration has led to a profound understanding of Go's network transmission model and the fundamental reasons behind its high performance. Simultaneously, we cross-referenced parts of the logic consistent with our previous [C code](https://www.google.com/search?q=%231socket-underlying-c), completing the cognitive leap from traditional low-level C calls to modern Web network architecture. While we have spent the entire discussion dissecting the underlying implementation of Go’s communication, let us shift our perspective to examine the macro background of programming language evolution before we conduct a final global review of this network architecture.

#### The Direction of Modern Languages and Go

* In my subpar university textbooks, there was a saying: "C generates all things." This phrase is based on a fantasy: that the foundation of the computing world is fixed and unchanging, with C and Assembly serving as the absolute bedrock. It suggests that while high-level languages flourish with their own unique characteristics, they must eventually return to the underlying logic of C. However, through this top-down research into Go's underlying mechanisms—and incorporating knowledge from YouTube’s [Core Dumped science videos](https://www.youtube.com/watch?v=7ge7u5VUSbE [[00:46](http://www.youtube.com/watch?v=7ge7u5VUSbE&t=46)])—we know that the "fixed" nature of these low-level languages is merely a byproduct of historical inertia (much like how most major websites still use IPv4 despite IPv6 being promoted for years).
* The article [C Is Not a Low-level Language](https://www.google.com/search?q=https://queue.acm.org/detail.cfm%3Fid%3D3212479) profoundly reveals the deep-seated contradiction between C and modern processor architectures, as well as the series of hardware compromises modern processors are forced to make to accommodate C’s legacy machine model. For modern CPUs, multi-threading, vector operations (SIMD), and multi-level cache structures are fundamental design cornerstones. Yet, C remains tethered to a flat memory model, a single-threaded execution perspective, and traditional programming ideologies. This not only fails to fully exploit modern hardware performance but also generates a vast array of difficult-to-trace memory safety and concurrency issues.
* Different modern programming languages attempt to resolve this core contradiction through their respective philosophies. **Rust** introduces a strict Ownership mechanism to solve memory management and utilizes a Borrow Checker to eliminate dangling pointers, effectively removing "Null" at the language level. These measures essentially solve safety issues through stricter compile-time language specifications. While Rust has made strides in performance (e.g., designing against the abuse of traditional linked lists to favor CPU caches), its starting point and design logic make it more of an ultimate "patch" for C’s safety flaws rather than a total replacement of underlying concurrency concepts.
* **Go**, on the other hand, took a completely different path. By introducing a powerful **Runtime** mechanism, Go takes over memory maintenance, freeing developers from the tedious manual handling of memory allocation and deallocation. More crucially, it achieves extremely high concurrency and performance elegantly through lightweight **Goroutines** combined with a scheduler. Of course, this Runtime comes at a cost: microsecond-level GC pauses (Stop-The-World) and scheduling overhead make Go feel inadequate for extremely rigorous, low-latency low-level requirements. Furthermore, highly automated memory management provides development convenience but strips developers of the ability to precisely control memory layout.
* Meanwhile, the **Zig** language attempts to find another balance: it returns memory management power to the developer via Explicit Allocators, utilizes a powerful `comptime` mechanism to replace traditional C macros, and handles coroutine calls cleverly through Colorless Functions, attempting to replace C’s ecological niche from the roots of systems programming.
* Nevertheless, we cannot deny that C remains the core language dominating mainstream computer science education today. It is still one of the best gateways to understanding the operating laws of the computing underworld. Rather than attempting an impossible "total abandonment" or falling into "religious superstition" regarding the language, the more sensible and scientific learning path is to use C to peer into the foundations of computing—and in that process, deeply understand the development trajectory and performance dilemmas of contemporary computer architecture.

#### Go's Networking System

* Our summary discussion here is built upon the understanding of the [epoll](./01_webcoding_based_on_c/05_tcp/06_server_epoll.c) principles previously implemented in C.
* From a macro perspective, Go's networking system can be clearly divided into the underlying `runtime` layer and the encapsulation `internal` layer. The former is deeply bound to Go’s Garbage Collector (GC) and scheduler, directly handling assembly-level system calls and multi-goroutine concurrency states. The latter provides general I/O interface encapsulations for upper layers (such as the `net` package).
* During the service startup phase, the `listen` function first creates and explicitly configures the corresponding network connection interface (**Listener**), which encapsulates specific method implementations for different protocols. This creation process implicitly initializes the `netpoll` (network poller) object and the underlying socket file descriptor, eventually registering (writing) the socket into the global `netpoll` monitoring red-black tree. Simultaneously, it executes core system calls like `bind` at the `runtime` layer.
* Logically, the `accept` function can be viewed entirely as a unique `read` operation triggered by the arrival of a new connection. Here, we see the intersection of all the I/O functions we analyzed: [init](./02_go_sdk/go/src/internal/poll/fd_poll_runtime.go#L38). For `listen`, this `init` function is the business logic endpoint for its registration into the multiplexer. For `accept`, `read`, and `write`, this `init` is merely a prerequisite call for the upper-layer encapsulation to access the asynchronous I/O system. Go cleverly encapsulates the initialization of the underlying environment with the operation of writing the socket into the poller (using a double-checked lock), mechanically ensuring that no illegal read/write events are written before `netpoll` is fully initialized. At the source code level of the `net` package, the strict functional boundary between the upper `net.netFD` and the internal `poll.FD` is not our primary focus. We simply need to understand that the latter (`poll.FD`) is the general low-level business object that executes actual network calls and interfaces downward with the `runtime` state machine, while the former (`net.netFD`) is an abstract shell providing independent network protocol encapsulation and business-level error checking for the end-user.
* When handling a new connection via the `go` keyword, the scheduler creates a brand-new Goroutine and pushes the business function to be executed onto that coroutine's stack. At this point, we allow this new coroutine to focus on the send/receive logic of the specific network connection, while the original parent coroutine continues to block on the `accept` call within an infinite loop to detect the next incoming connection.
* As the code execution deepens, control flow enters the `runtime` layer. Here, the business-level structures transform (map) into the underlying `pollDesc` structure. These structures, spanning different layers, achieve safe and efficient indirect coupling by storing the underlying `runtime` address on the `internal` side and utilizing memory versioning via special embedding techniques.
* Tracing to the deepest point of network blocking, the execution paths of `accept`, `read`, and `write` converge into a single lane: [poll_runtime_pollWait](./02_go_sdk/go/src/runtime/netpoll.go#L336). This function is responsible for pre-execution error checks and initiating a critical `for` loop mechanism to ensure retry reliability. Here, the only underlying difference between these three operations emerges: based on the `mode` parameter passed by the caller, they precisely choose whether to store the current coroutine's address in the read-wait field `rg` (Read Goroutine, for `accept` and `read`) or the write-wait field `wg` (Write Goroutine, for `write`).
* Subsequently, the [netpollblock](./02_go_sdk/go/src/runtime/netpoll.go#L548) function appears, using CAS (Compare-And-Swap) atomic instructions to locklessly and safely exchange and confirm the internal state of the corresponding `pollDesc`. Once everything is ready, the ultimate call, [gopark](./02_go_sdk/go/src/runtime/proc.go#443), occurs. This function critically switches the execution flow to the `g0` system stack, safely saves the context of the current user coroutine, completely detaches the coroutine (G) from the underlying OS thread (M), and finally puts the coroutine to sleep in heap memory. This yields thread resources to schedule other tasks.

---

In this architecture, we see the core philosophy of Go’s design: at the lowest level, it resolutely designs and abstracts unified, general system call interfaces; at the highest level, it implements independent, elegant object-oriented encapsulations for specific protocols and functions. Structures at different architectural levels strictly implement business functions appropriate to their level. They are tightly coupled—either directly or indirectly—through pointer passing or version number validation.

The greatest innovation lies in the fact that Go did not treat network polling as a plug-in extension library. Instead, it **internalized the asynchronous multiplexing mechanism of network connections as the core driving force of the underlying Runtime architecture**, deeply binding it with the GC mechanism to power the operation of `netpoll`. It is this tight coordination across language boundary levels that ultimately creates Go's unshakable advantage in the fields of modern Web development and high-concurrency applications.