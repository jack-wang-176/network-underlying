# Data Transmission and I/O Models

In the previous two sections, we explained the underlying mechanisms of `listen` and `accept`. Before diving into the low-level analysis of `read` and `write`, let's focus on a crucial detail: `go handleConnection(conn)`.

When we use the `go` keyword in our code, the compiler automatically translates it into a call to `newproc`. At this point, the pointer to the target function and the parent goroutine's PC (Program Counter, used for tracing its origin) are saved. Subsequently, the program calls the `systemstack` function to switch to the `g0` system stack and calls `newproc1` to create the execution stack for the new goroutine.

There is an incredibly elegant "fake stack" design in this function: the return address of the new stack is hardcoded to the address of the `goexit` function, ensuring resources are automatically reclaimed after the goroutine finishes executing. Meanwhile, the target execution function's address is also pushed onto the stack.

Therefore, the entity executing `handleConnection` here is a newly born and awakened goroutine. The original parent goroutine, on the other hand, continues its mission, continuously calling `accept` in an infinite loop until the next round of code executes and returns a value. Note that at this point, the file descriptor (FD) of the new connection has already been safely wrapped in the return value of `accept`.

Returning to the `accept` function, what we didn't specifically mention earlier is that the exact return type of this function is `TCPConn`. However, our focus here is not on the specific mechanisms exclusive to the `TCP` protocol, but rather on the universal underlying mechanisms for all network connections—namely, the `write` and `read` methods that implement the abstract `Conn` interface. At this stage, the return value has already perfectly encapsulated all the underlying information required for this connection.

---

## The Underlying Implementation of Read

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

## The Underlying Implementation of Write

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

# Summary

In this section, we conducted a step-by-step, top-down deep dive into the principles of our sample code. This exploration has led to a profound understanding of Go's network transmission model and the fundamental reasons behind its high performance. Simultaneously, we cross-referenced parts of the logic consistent with our previous [C implementation](../part1-socket-c/01-basic.md), completing the cognitive leap from traditional low-level C calls to modern Web network architecture. While we have spent the entire discussion dissecting the underlying implementation of Go’s communication, let us shift our perspective to examine the macro background of programming language evolution before we conduct a final global review of this network architecture.

## The Direction of Modern Languages and Go

* In my subpar university textbooks, there was a saying: "C generates all things." This phrase is based on a fantasy: that the foundation of the computing world is fixed and unchanging, with C and Assembly serving as the absolute bedrock. It suggests that while high-level languages flourish with their own unique characteristics, they must eventually return to the underlying logic of C. However, through this top-down research into Go's underlying mechanisms—and incorporating knowledge from YouTube’s [Core Dumped science videos](https://www.youtube.com/watch?v=7ge7u5VUSbE [[00:46](http://www.youtube.com/watch?v=7ge7u5VUSbE&t=46)])—we know that the "fixed" nature of these low-level languages is merely a byproduct of historical inertia (much like how most major websites still use IPv4 despite IPv6 being promoted for years).
* The article [C Is Not a Low-level Language](https://www.google.com/search?q=https://queue.acm.org/detail.cfm%3Fid%3D3212479) profoundly reveals the deep-seated contradiction between C and modern processor architectures, as well as the series of hardware compromises modern processors are forced to make to accommodate C’s legacy machine model. For modern CPUs, multi-threading, vector operations (SIMD), and multi-level cache structures are fundamental design cornerstones. Yet, C remains tethered to a flat memory model, a single-threaded execution perspective, and traditional programming ideologies. This not only fails to fully exploit modern hardware performance but also generates a vast array of difficult-to-trace memory safety and concurrency issues.
* Different modern programming languages attempt to resolve this core contradiction through their respective philosophies. **Rust** introduces a strict Ownership mechanism to solve memory management and utilizes a Borrow Checker to eliminate dangling pointers, effectively removing "Null" at the language level. These measures essentially solve safety issues through stricter compile-time language specifications. While Rust has made strides in performance (e.g., designing against the abuse of traditional linked lists to favor CPU caches), its starting point and design logic make it more of an ultimate "patch" for C’s safety flaws rather than a total replacement of underlying concurrency concepts.
* **Go**, on the other hand, took a completely different path. By introducing a powerful **Runtime** mechanism, Go takes over memory maintenance, freeing developers from the tedious manual handling of memory allocation and deallocation. More crucially, it achieves extremely high concurrency and performance elegantly through lightweight **Goroutines** combined with a scheduler. Of course, this Runtime comes at a cost: microsecond-level GC pauses (Stop-The-World) and scheduling overhead make Go feel inadequate for extremely rigorous, low-latency low-level requirements. Furthermore, highly automated memory management provides development convenience but strips developers of the ability to precisely control memory layout.
* Meanwhile, the **Zig** language attempts to find another balance: it returns memory management power to the developer via Explicit Allocators, utilizes a powerful `comptime` mechanism to replace traditional C macros, and handles coroutine calls cleverly through Colorless Functions, attempting to replace C’s ecological niche from the roots of systems programming.
* Nevertheless, we cannot deny that C remains the core language dominating mainstream computer science education today. It is still one of the best gateways to understanding the operating laws of the computing underworld. Rather than attempting an impossible "total abandonment" or falling into "religious superstition" regarding the language, the more sensible and scientific learning path is to use C to peer into the foundations of computing—and in that process, deeply understand the development trajectory and performance dilemmas of contemporary computer architecture.

## Go's Networking System

* Our summary discussion here is built upon the understanding of the [epoll](../../../01_socket_underlying_c/05_tcp/06_server_epoll.c) principles previously implemented in C.
* From a macro perspective, Go's networking system can be clearly divided into the underlying `runtime` layer and the encapsulation `internal` layer. The former is deeply bound to Go’s Garbage Collector (GC) and scheduler, directly handling assembly-level system calls and multi-goroutine concurrency states. The latter provides general I/O interface encapsulations for upper layers (such as the `net` package).
* During the service startup phase, the `listen` function first creates and explicitly configures the corresponding network connection interface (**Listener**), which encapsulates specific method implementations for different protocols. This creation process implicitly initializes the `netpoll` (network poller) object and the underlying socket file descriptor, eventually registering (writing) the socket into the global `netpoll` monitoring red-black tree. Simultaneously, it executes core system calls like `bind` at the `runtime` layer.
* Logically, the `accept` function can be viewed entirely as a unique `read` operation triggered by the arrival of a new connection. Here, we see the intersection of all the I/O functions we analyzed: [init](../../../02_go_sdk/go/src/internal/poll/fd_poll_runtime.go#L38). For `listen`, this `init` function is the business logic endpoint for its registration into the multiplexer. For `accept`, `read`, and `write`, this `init` is merely a prerequisite call for the upper-layer encapsulation to access the asynchronous I/O system. Go cleverly encapsulates the initialization of the underlying environment with the operation of writing the socket into the poller (using a double-checked lock), mechanically ensuring that no illegal read/write events are written before `netpoll` is fully initialized. At the source code level of the `net` package, the strict functional boundary between the upper `net.netFD` and the internal `poll.FD` is not our primary focus. We simply need to understand that the latter (`poll.FD`) is the general low-level business object that executes actual network calls and interfaces downward with the `runtime` state machine, while the former (`net.netFD`) is an abstract shell providing independent network protocol encapsulation and business-level error checking for the end-user.
* When handling a new connection via the `go` keyword, the scheduler creates a brand-new Goroutine and pushes the business function to be executed onto that coroutine's stack. At this point, we allow this new coroutine to focus on the send/receive logic of the specific network connection, while the original parent coroutine continues to block on the `accept` call within an infinite loop to detect the next incoming connection.
* As the code execution deepens, control flow enters the `runtime` layer. Here, the business-level structures transform (map) into the underlying `pollDesc` structure. These structures, spanning different layers, achieve safe and efficient indirect coupling by storing the underlying `runtime` address on the `internal` side and utilizing memory versioning via special embedding techniques.
* Tracing to the deepest point of network blocking, the execution paths of `accept`, `read`, and `write` converge into a single lane: [poll_runtime_pollWait](../../../02_go_sdk/go/src/runtime/netpoll.go#L336). This function is responsible for pre-execution error checks and initiating a critical `for` loop mechanism to ensure retry reliability. Here, the only underlying difference between these three operations emerges: based on the `mode` parameter passed by the caller, they precisely choose whether to store the current coroutine's address in the read-wait field `rg` (Read Goroutine, for `accept` and `read`) or the write-wait field `wg` (Write Goroutine, for `write`).
* Subsequently, the [netpollblock](../../../02_go_sdk/go/src/runtime/netpoll.go#L548) function appears, using CAS (Compare-And-Swap) atomic instructions to locklessly and safely exchange and confirm the internal state of the corresponding `pollDesc`. Once everything is ready, the ultimate call, [gopark](../../../02_go_sdk/go/src/runtime/proc.go#443), occurs. This function critically switches the execution flow to the `g0` system stack, safely saves the context of the current user coroutine, completely detaches the coroutine (G) from the underlying OS thread (M), and finally puts the coroutine to sleep in heap memory. This yields thread resources to schedule other tasks.

---

In this architecture, we see the core philosophy of Go’s design: at the lowest level, it resolutely designs and abstracts unified, general system call interfaces; at the highest level, it implements independent, elegant object-oriented encapsulations for specific protocols and functions. Structures at different architectural levels strictly implement business functions appropriate to their level. They are tightly coupled—either directly or indirectly—through pointer passing or version number validation.

The greatest innovation lies in the fact that Go did not treat network polling as a plug-in extension library. Instead, it **internalized the asynchronous multiplexing mechanism of network connections as the core driving force of the underlying Runtime architecture**, deeply binding it with the GC mechanism to power the operation of `netpoll`. It is this tight coordination across language boundary levels that ultimately creates Go's unshakable advantage in the fields of modern Web development and high-concurrency applications.
