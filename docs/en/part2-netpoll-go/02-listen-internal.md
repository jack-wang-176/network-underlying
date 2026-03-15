## 1. Creating the Socket

* If we trace down the `listen` function all the way, we will find that the core entry point is the [socket](../../../02_go_sdk/go/src/net/sock_posix.go#L18) function. This function is the instance where Go creates a socket at the lowest level. Similar to the `socket()` system call we used in C, it also returns a corresponding File Descriptor.
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

## 2. Building Stream Sockets: `listenStream`

* Inside the function, the creation of stream sockets (TCP) and datagram sockets (UDP) are encapsulated into different paths. Taking the stream socket we focus on as an example, let's see what [listenStream](../../../02_go_sdk/go/src/net/sock_posix.go#L150) does:
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

## 3. Entering Netpoll: `fd.init`

* The previous steps are no different from the logic we implemented in C, but starting from [fd.init](../../../02_go_sdk/go/src/internal/poll/fd_unix.go#L55), we will enter Go's unique realm of magic—**Netpoll (Network Poller)**.
* The main function of this method is to determine whether the current file descriptor belongs to a network file (i.e., not a regular file). If it is, it initializes the network polling mechanism for it:
  ```go
  // Initialize pollDesc (poll descriptor)
  fd.pd.init(fd)
  ```



## 4. The Intersection of Runtime and Network: `pd.init`

* The [init](../../../02_go_sdk/go/src/internal/poll/fd_poll_runtime.go#L38) function is the critical intersection between Go's standard `net` library and the `runtime` package, serving as the cornerstone for "synchronous code, asynchronous execution."
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


