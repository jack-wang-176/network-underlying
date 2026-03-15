# 4. Summary

* Now, let's shift our perspective back to the content of Part 1 (we will ignore UDP here, as its structure is much simpler compared to TCP). Looking back at the [tcp_server](../../../01_socket_underlying_c/05_tcp/02_server.c) and [tcp_client](../../../01_socket_underlying_c/05_tcp/01_client.c) we wrote in Part 1, throughout that entire chapter, we focused solely on the construction and disassembly of individual data packets, but neglected the global vision of connection establishment under the C/S (Client/Server) architecture. Take the SYN handshake packet we manually crafted using a Raw Socket in the previous section: it indeed successfully passed inspections and was sent to the target router or server. However, when the target server returns a SYN-ACK packet, because we haven't recorded this connection in our operating system's TCP state machine, the kernel—to maintain the rigor of the protocol stack—will not hesitate to send an RST (Reset) packet to the other party, forcibly severing this inexplicable connection.
* The most dramatic point is that while we used the system's low-level APIs to perform unauthorized one-way communication, the system's own strict TCP specifications actually helped us resolve the subsequent problems. This brings us back to the C/S communication model we emphasized in Chapter 1. In Part 1, those standard Socket APIs that we treated as "black boxes" and called directly were actually silently building an extremely solid defensive line of contracts for us at the lowest level:
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


* When we call the `connect` function, the kernel is actually doing the heavy lifting of physical network operations on our behalf. It performs the following steps:
* **1. Resource Allocation:** Creates a TCB (Transmission Control Block) within the system kernel. This is equivalent to officially registering the context of this connection in the operating system's archives.
* **2. State Transition:** The kernel changes the state of this Socket from CLOSED to SYN_SENT.
* **3. Proxy Packet Sending:** The kernel automatically constructs a SYN packet and sends it out (which is exactly the binary array we manually spliced using Raw Sockets in Part 3).
* **4. Starting Timers:** The kernel starts a timer and suspends your program, silently waiting for the other party to reply with a SYN-ACK.


* Correspondingly, `accept` and `listen` are responsible for putting the server's port 80 into the LISTEN state. We only simply called them in Part 1, but right now, from a low-level perspective, the true mechanisms of these two functions are as follows:
* **`listen()`:** When the server calls `listen()`, the kernel does not send any packets into the network. Instead, it carves out two crucial structures in the system's kernel space for this Socket: the Half-connection Queue (SYN Queue) and the Full-connection Queue (Accept Queue). It declares to the network card and the underlying protocol stack that any SYN handshake request targeting port 80 will be taken over by it.
* **`accept()`:** `accept()` is actually a pure memory operation completely detached from the network packet sending logic. When the client's SYN arrives, the kernel automatically replies with a SYN-ACK, and the connection enters the half-connection queue; when the client's final ACK arrives, the three-way handshake is silently completed in kernel space, and this mature connection (TCB) is moved into the full-connection queue. The sole purpose of `accept()` is to block the current thread and keep a close eye on the full-connection queue. Once there is a connection that has completed the handshake in the queue, it "plucks" this connection out, wraps it into a new Socket file descriptor (fd), and throws it up to the application layer.


* Combined with the rigorous logic of this state machine, what you can clearly see here is that in the scenario of blindly sending a SYN using a Raw Socket, when the target machine's tcp_acp (i.e., the SYN-ACK return packet) data packet returns, the kernel will directly determine that this is an illegal ghost packet because there is no corresponding PCB in our own kernel (we never called `connect` to register a record), and it will immediately send an RST to cut off the TCP connection.
* Now, we can finally peer through the low-level details of C language function calls and clearly see how these details interact deeply with the operating system kernel to implement network communication protocols. From this perspective, the physical transmission and sending of data packets are merely a single cornerstone supporting the grand C/S architecture.
* But it goes far beyond just the C/S architecture. Having experienced the black-box calls of Part 1, the concurrency models of Part 2, and the dimensional-reduction dissection of Part 3 in this chapter, when we think about the flow of network packets again, **we should no longer be confined to merely focusing on tedious field details**. Although we had hardcore discussions in extreme detail about struct alignment, byte order, and checksums to demonstrate the specific implementation of protocols at the packet level, our real purpose is to elevate our perspective back up. We must turn our gaze to the complex network interaction architectures in real-world production, and profoundly understand exactly how those cold protocol specifications make field design compromises to achieve and conform to specific functional points in these higher-level architectures (such as security defense, connection multiplexing, and state synchronization).
* **We focus on details absolutely not to get entangled and stubbornly stuck on them, but so that, after peeling away the layers, we can accurately grasp the pulse of technological evolution from a more macroscopic perspective.** Only when we have personally spliced binary protocol headers, and truly experienced the fierce conflict between machine physical logic and human abstract logic, can we finally understand the layer-by-layer progression and interface-encapsulated computer architecture behind a simple `http.Get`.