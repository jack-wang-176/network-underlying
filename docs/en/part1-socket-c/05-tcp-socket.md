# 05 TCP Socket (TCP Communication)

* **background**
* Although the biggest difference between TCP and UDP here is that TCP uses a three-way handshake and four-way wave to guarantee data transmission, most of these complex state transitions are encapsulated by the kernel when we program using functions. In other words, we consider socket lifecycle management more from the **Application Layer** perspective here.
* **Shift in Design Philosophy**: UDP is stateless; a socket can send packets to any IP. However, TCP is connection-oriented, like making a phone call; you must connect before speaking. This design requires the server to maintain a "listening socket" specifically to welcome guests, and for every guest (client) that arrives, a new "service socket" must be created specifically for chatting.
* **Core Contradiction of Concurrency**: How to efficiently manage these hundreds or thousands of "service sockets"? This leads to two technical routes:
1. **Multi-process/Multi-thread**: Solve by adding manpower (CPU scheduling units); one connection corresponds to one thread/process.
2. **IO Multiplexing (Non-blocking)**: Use non-blocking IO + Event Polling (e.g., epoll) to let one waiter (single thread) watch over all tables.




* **[01_client](../../../01_socket_underlying_c//05_tcp/01_client.c)**
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


* **[02_server](../../../01_socket_underlying_c//05_tcp/02_server.c)**
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


* **[03_server_fork](../../../01_socket_underlying_c//05_tcp/03_server_fork.c)**
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


* **[04_server_thread](../../../01_socket_underlying_c//05_tcp/04_server_thread.c)**
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


* **[05_server_noblock](../../../01_socket_underlying_c//05_tcp/05_server_noblock.c)**
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


* **[06_server_epoll](../../../01_socket_underlying_c//05_tcp/06_server_epoll.c)**
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