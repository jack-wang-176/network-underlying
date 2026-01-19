<div align="center">

# Webcoding
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

### [Part 1: Webcoding based on C](#webcoding-based-on-c)
- [01 Basic (Fundamentals)](#01-basic-fundamentals)
- [02 UDP Socket](#02-udp-socket)
- [03 TFTP Implementation](#03-tftp-implementation)
- [04 Broadcast & Multicast](#04-broadcast--multicast)
- [05 TCP Socket](#05-tcp-socket)

---

## Introduction & Details

## **1.Webcoding based on C**

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