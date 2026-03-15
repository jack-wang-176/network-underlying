<div align="center">

# network-underlying

**English Version** | [中文版本](./README.md)

</div>

Hello! This is my study notebook on computer networks. In this notebook, I try to organize the entire content according to the five-layer network model to help readers and myself build a fundamental struct.

👉 **[Click here to enter the Table of Contents & Reading Guide](./docs/en/SUMMARY.md)**

**Part 1**: Builds a basic network model using C, demonstrating the evolution of server concurrency models from `fork` to `epoll`. It focuses on the application of sockets at the network layer.

**Part 2**: Takes a deep dive into the Go Runtime source code to comprehensively deconstruct the `netpoll` network multiplexing architecture and Goroutine scheduling. It showcases the development and encapsulation of application-layer tools, as well as the networking foundation of the modern web.

**Part 3**: Bypasses the transport layer using C Raw Sockets to manually construct, send, and parse underlying IP, TCP, and UDP packets. This part handles receiving packets and progressively explains the encapsulation of packet headers and protocol implementations from the data link layer to the network layer.

---
*Note: This project is still being actively updated (Work in Progress). Feel free to Watch and Star to stay tuned for the latest updates.*