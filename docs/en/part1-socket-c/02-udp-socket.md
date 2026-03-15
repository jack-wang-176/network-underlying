# 02 UDP Socket (UDP Communication)

Connectionless, unreliable data transmission protocol.

* **[01_socket](../../../01_socket_underlying_c//02_udp/01_socket.c)**
* Shows the function for creating a socket.
```c
int socket (int __domain, int __type, int __protocol)

```


* `domain` determines the IP type, while `type` determines the transmission type (TCP or UDP). `protocol` is the specific protocol format.
* `socket` is an `int` type; it relies on the abstraction of file descriptors to implement calls at the system level, demonstrating the "everything is a file" design philosophy in Linux.
* As a file descriptor, it must be closed at the end of the program. `close` means disconnecting during communication; in TCP, this becomes more complex.


* **[02_sendto](../../../01_socket_underlying_c//02_udp/02_sendto.c)**
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


* **[03_bind](../../../01_socket_underlying_c//02_udp/03_bind.c)**
* The `bind` function is mainly used to fix the IP and port number. Based on the orientation of this function, it is easy to understand that the information receiver needs this requirement more.
```c
int bind (int __fd, __CONST_SOCKADDR_ARG __addr, socklen_t __len)
    __THROW;

```


* In TCP/UDP programming, we generally simply refer to the side that needs binding as the server, but this is actually determined by the relative relationship between information receiving and sending. We can see further embodiment of this in multicast and group casting.


* **[04_recvfrom](../../../01_socket_underlying_c//02_udp/04_recvfrom.c)**
* `recvfrom` is the UDP receiving function.
```c
recvfrom (int __fd, void *__restrict __buf, size_t __n, int __flags,
    __SOCKADDR_ARG __addr, socklen_t *__restrict __addr_len)

```


* It is important to note here that `recvfrom` receives data from other hosts, so we need to pre-create an empty structure for it to fill in. Also, `addrlen` is changed to output the length of the received information. This design allows UDP to implement multi-threading very simply. The cost is that every client needs a corresponding structure. We will see later that because of the three-way handshake required by TCP, TCP's design takes a completely different path.


* **[05_server](../../../01_socket_underlying_c//02_udp/05_server.c)&&[06_client](../../../01_socket_underlying_c//02_udp/06_client.c)** * This section contains the specific running code for the UDP client and server. Essentially, it invokes the functions mentioned above to concretely implement the communication process.
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
