# The Underlying Implementation of Accept

In the previous two sections, we deeply explored the underlying system call implementation of `listen`. Before formally diving into `accept`, let's do a bottom-up review of this function's return value encapsulation to clarify how Go exposes underlying resources to the user layer.

First, the part that interfaces with our C-language perspective is the `socket` file descriptor. This raw file descriptor is encapsulated by the upper-level `listenTCPProto`, ultimately becoming the [TCPlistener](../../../02_go_sdk/go/src/net/tcpsock.go%23L291) struct. This struct is not just a container for the file descriptor; it also embeds the [listenConfig](../../../02_go_sdk/go/src/net/dial.go#L672) struct. `listenConfig` contains configurations like the `control` hook function and KeepAlive probe intervals. Its significance lies in **opening up the operating system's low-level network parameter settings to the user layer**, allowing developers to further restrict and specify the behavior of the underlying `socket` through configuration fields at the Go language level.

Ultimately, through struct definitions and the method encapsulation of `tcplistener`, Go tightly "couples" the underlying `listen` system call with the upper-level configuration struct. This relationship is already determined when we call `net.Listen` and pass in the `network` and `address` fields.

It is worth noting that the `listen` interface is primarily used for streaming and connection-oriented protocols (like `tcp`, `ssl`, etc.); parallel to it is the `PacketConn` interface, used for datagram protocols (like `udp`, `dns`, etc.). This design perfectly embodies Go's philosophy of **interface-oriented programming**, hiding the implementation details of different protocols behind unified interfaces, thereby achieving code modularity and componentization.

Now that we understand the construction process of the `Listener`, the `Accept` method we call in our business code is essentially linked to `func (ln *TCPListener) accept()` via interfaces. Next, we will start from the upper-level entry point of [accept](../../../02_go_sdk/go/src/net/tcpsock_posix.go#L158) and dissect its underlying implementation principles top-down.

## 1. The Entry to the Network Poller: fd_unix

* First, the code logic arrives at the [accept](../../../02_go_sdk/go/src/net/fd_unix.go#L171) function under the `internal/poll` package. This function is the critical hub for interactions between the network poller (`Netpoller`) and the socket. It primarily accomplishes the following four core functions:
1. **Invoke System Call**: Encapsulates and executes the underlying `accept` system call.
2. **Object Wrapping**: Wraps the newly returned file descriptor into a `netFD` (Network File Descriptor) object.
3. **Register for Polling**: Registers the new `netFD` into `epoll` (or the equivalent I/O multiplexer for the specific platform) to listen for subsequent read/write events.
4. **Populate Information**: Fills in the remote and local address information.


* Let's focus on the core logic of [accept](../../../02_go_sdk/go/src/internal/poll/fd_unix.go#L594) in `internal/poll/fd_unix.go`:
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


* The non-blocking I/O approach here is completely consistent with the logic we implemented in the C language [netpoll](../../../01_socket_underlying_c/05_tcp/06_server_epoll.c) section: when `accept` returns `EAGAIN`, do not put the thread to sleep; instead, yield the CPU.
* **Key Point**: The brilliance of Go lies in the fact that when encountering `EAGAIN`, it calls `fd.pd.waitRead` to suspend the current **Goroutine**, rather than blocking the operating system thread.

## 2. The Core of Encapsulation: newFD

* After the `accept` system call successfully returns and obtains the file descriptor, Go does not use this bare `int` handle directly. Instead, it wraps it into a feature-rich object via the [newFD](../../../02_go_sdk/go/src/net/fd_unix.go#L26) function.
* This function is more than just simple memory allocation; it defines how the Go runtime views this network connection:
* **poll.FD**: This is the absolute core, acting as the bridge between Go's I/O layer (user code) and the Runtime layer (scheduler).
* **Sysfd**: Saves the underlying socket handle; all operations ultimately boil down to this integer.
* **IsStream**: A flag indicating whether it is a stream socket.
* If it is TCP, this value is `true`.
* If it is UDP, this value is `false`.


* **ZeroReadIsEOF**: This is a critical rule for determining termination.
* As we mentioned in the [udp](../../../01_socket_underlying_c/02_udp/04_recvfrom.c) section, UDP allows sending 0-byte packets, which does not signify connection closure in UDP.
* However, in TCP, a `read` returning 0 bytes usually means the peer has sent a FIN packet (EOF), and the connection needs to be closed.
* `newFD` automatically sets this field based on the value of `IsStream`, ensuring the upper-level business logic can correctly handle the meaning of "reading 0 bytes."




* **Concurrency Safety**: The object initialized by `newFD` internally maintains a read/write lock (`fdMutex`). This is the underlying guarantee that allows Go to safely permit multiple Goroutines to perform concurrent operations on the same socket (although it is generally not recommended to do so).

## 3. Registration for Polling: init

* Immediately after wrapping is complete, the connection is "activated." This step is handled by [init](../../../02_go_sdk/go/src/internal/poll/fd_unix.go#L55).
* The logic of this function is entirely consistent with the approach we saw earlier in [listen](./02-listen-internal.md); you could say **all roads lead to Rome**:
* The **Listen** phase registers the listening socket (`listener`) into `epoll` to listen for the "new connection arrival" event.
* The **Accept** phase registers the newly established connection socket (`conn`) into `epoll` to listen for "data readable/writable" events.


* Under the hood, both ultimately call `poll.runtime_pollOpen` to add the `Sysfd` into the red-black tree of the `epoll` instance. Once this step is complete, the connection officially enters the management scope of Go's Network Poller (`Netpoller`), laying the foundation for subsequent asynchronous I/O.

## 4. Deep into Runtime: poll_runtime_pollWait

* Diving further into the `waitRead` function, we eventually cross from the `internal/poll` package into the `runtime` package's [poll_runtime_pollWait](../../../02_go_sdk/go/src/runtime/netpoll.go#L336) via the `//go:linkname` linking mechanism.
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


* Before truly suspending, the runtime first checks for timeout or closure errors via [netpollcheckerr](../../../02_go_sdk/go/src/runtime/netpoll.go%23L512).
* The `for` loop logic here handles edge cases (e.g., a timeout triggers but is then reset), preventing state inconsistencies caused by concurrent modifications, and ensuring that every suspension is valid.

## 5. Core Blocking Logic: netpollblock

* Next, we delve into [netpollblock](../../../02_go_sdk/go/src/runtime/netpoll.go%23L548), the decision center for Goroutine suspension.
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


* **State Machine Management**: `pd.rg` and `pd.wg` are atomic `uintptr`s. As mentioned earlier in the `[polldesc](./03-runtime-architecture.md#5-polldesc-memory-management-efficient-reuse-and-gc-isolation)` section, their default value is `pdNil` (0).
* **CAS (CompareAndSwap)**: This utilizes CAS to achieve lock-free state transitions. It integrates "value checking" and "value swapping" into one atomic step, ensuring safety in a multi-threaded environment.
* **Suspend and Wake Up**: `gopark` is the watershed. Before executing `gopark`, the current Goroutine is running; when `gopark` returns, it means the Goroutine has been woken up by an `epoll` event. At this point, it checks whether the return value is `pdReady` to confirm that it was woken up by a data packet and not by a timeout.

## 6. Scheduler Context Switch: gopark and the g0 Stack

* Finally, we analyze the lowest-level scheduler interface, [gopark](../../../02_go_sdk/go/src/runtime/proc.go#L443). This is the key to a Goroutine yielding CPU control.
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
* **Callback Function**: The `unlockf` here is the [netpollblockcommit](../../../02_go_sdk/go/src/runtime/netpoll.go#L529) that we passed in `netpollblock`.
* On the g0 stack, the system executes the [park_m](../../../02_go_sdk/go/src/runtime/proc.go#L4007) function:
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


* [casgstatus](../../../02_go_sdk/go/src/runtime/proc.go#L1105): Atomically updates the coroutine state from Running to Waiting.
* [dropg](../../../02_go_sdk/go/src/runtime/proc.go#L4001): Completely unbinds them by setting `mp.curg = nil` and `gp.m = nil`. At this point, G is "sleeping" on the heap, and M is set free.
* **Crucial Callback**: Executes [netpollblockcommit](../../../02_go_sdk/go/src/runtime/netpoll.go#L529), using `atomic.Store(gpp, gp)` to write the memory address of the current Goroutine into `pd.rg`. **This step is of paramount importance**; it essentially tells the netpoller: "When data arrives for this socket, please wake up the coroutine at address `gp`."
* [schedule](../../../02_go_sdk/go/src/runtime/proc.go#L3839): M does not rest. It immediately executes `schedule()` to search the global queue or local queue for the next pending G to execute. This is the core secret of Go's high concurrency—**I/O blocks the Goroutines, not the system threads.**

## Summary

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
