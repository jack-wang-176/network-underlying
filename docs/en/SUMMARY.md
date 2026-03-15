#  Table of Contents

## **English Version** | [中文版本](../zh/SUMMARY.md)

## Part 1: Socket-Underlying-C
Explore traditional C language system calls and OS-level network communication principles.
* [01 Basic (Fundamentals)](./part1-socket-c/01-basic.md)
* [02 UDP Socket](./part1-socket-c/02-udp-socket.md)
* [03 TFTP Implementation](./part1-socket-c/03-tftp.md)
* [04 Broadcast & Multicast](./part1-socket-c/04-broadcast-multicast.md)
* [05 TCP Socket](./part1-socket-c/05-tcp-socket.md)

## Part 2: Netpoll-Underlying-Go
Deep dive into the Go Runtime and the core mechanisms behind Go's high-concurrency network model.
* [01 The Goal of This Part & netpoll Internals Overview](./part2-netpoll-go/01-overview.md)
* [02 listen Function Internals](./part2-netpoll-go/02-listen-internal.md)
* [03 The Netpoll Architecture: Deep Dive into the Runtime](./part2-netpoll-go/03-runtime-architecture.md)
* [04 Underlying implementation of Accept](./part2-netpoll-go/04-accept-internal.md)
* [05 Data transmission, I/O models & Summary](./part2-netpoll-go/05-io-models-and-summary.md)

## Part 3: Rawsocket-Underlying-C
Bypass the standard OS transport layer and face raw data link layer frames.
* [01 Preparation for a Deep Dive & Pointer Casting/Decapsulation](./part3-rawsocket-c/01-prep-and-casting.md)
* [02 Sending TCP Handshake Packet](./part3-rawsocket-c/02-send-tcp-handshake.md)
* [03 Summary and Architectural Reflections](./part3-rawsocket-c/03-summary.md)


---
>  **[Back to Home](../../README_EN.md)**