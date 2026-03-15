# **The Goal of This Part**

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

# **Sample Code Dissection & Overview of the Underlying `netpoll` Mechanism**

Although this code appears to be written in a very simple "synchronous blocking" style, thanks to the encapsulation by the Go runtime, it actually operates as highly efficient **asynchronous non-blocking I/O** at the lowest level. Let's break it down step-by-step in conjunction with `netpoll`:

## **1. `net.Listen("tcp", ":8080")`: Initialization and Low-Level Registration**

* **Surface Logic**: Creates a TCP listener bound to port 8080.
* **Underlying Mechanism**: At this stage, the Go runtime does more than just call the low-level `socket()` and `bind()` system functions. More importantly, it sets this listening socket to **Non-blocking** mode and registers its File Descriptor (FD) with the operating system's event poller (e.g., `epoll` in Linux, `kqueue` in macOS).  This serves as the entry point for the `netpoll` mechanism in Go.

## **2. `listener.Accept()`: Suspending and Waking Up Goroutines**

* **Surface Logic**: The program "gets stuck" (blocks) here until a new client connection arrives.
* **Underlying Mechanism**: Because the underlying socket is non-blocking, if there are no new connections, the low-level `accept` system call will immediately return an error (such as `EAGAIN`). At this point, Go's `netpoll` mechanism steps in: it will **suspend (Park)** the current Goroutine, releasing the CPU thread to execute other tasks.  It is only when the underlying `epoll` detects a new connection arriving at this port that `netpoll` will **wake up (Ready)** this suspended Goroutine to continue execution. This design allows even a single-core CPU to support an extremely high concurrency of waiting connections.

## **3. `go handleConnection(conn)`: The Classic Goroutine-per-Connection Model**

* **Surface Logic**: After acquiring a new connection, a new Goroutine is launched to exclusively serve this client, while the main loop goes back to execute `Accept()` and wait for the next incoming connection.
* **Underlying Mechanism**: This is the most central design pattern in Go network programming. Compared to C/C++, where handling concurrency requires manually writing complex callback functions or state machines, Go achieves simple concurrency through extremely lightweight Goroutines (which initially consume only 2KB of memory). Thousands of connections correspond to thousands of Goroutines, all efficiently managed by the Go Scheduler.

## **4. `conn.Read(buf)` and `conn.Write()`: Synchronous Code, Asynchronous Soul**

* **Surface Logic**: Inside the independent Goroutine, it continuously loops to read data sent by the client. If no data is received, `Read` will block.
* **Underlying Mechanism**: The blocking logic here is exactly the same as in `Accept()`. When there is no data to read in the buffer, this read operation triggers the Go scheduler to suspend the current `handleConnection` Goroutine and registers this Socket into `netpoll`. Once the client actually sends network data and the operating system's network stack finishes receiving it, `epoll` triggers an event. Go's background network polling thread will then put this Goroutine back into the runnable queue, and the code immediately "wakes up" from `conn.Read` and continues execution.

**Summary**: This code perfectly demonstrates the ingenuity of Go's design—**writing a high-performance asynchronous non-blocking server driven by `epoll` + `Goroutines` under the hood, using the simplest synchronous code logic**. Developers do not need to worry about complex file descriptor polling or state machine switching. All the "dirty work" is encapsulated within Go's runtime network multiplexer (`netpoll`). In the following steps, we will deconstruct this entire logical architecture from top to bottom.

