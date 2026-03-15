# 1. Preparation for a Deep Dive into the Protocol Stack

* **Cognitive Shift from "Stream" to "Frame"**:
In the C language TCP communication of [Part 1](../part1-socket-c/01-basic.md) and the Go language `netpoll` parsing of [Part 2](../part2-netpoll-go/01-overview.md), when we called `send()` or `conn.Write()`, we were always dealing with a smooth, continuous **data stream**. The reason we can treat network communication as simply as reading and writing local files is entirely due to the heavy encapsulation by the operating system kernel and the Go Runtime at the bottom layer.
However, "streams" do not exist in the real physical network. What travels through the network cables are discrete **data frames** that have been strictly sliced and packaged.
```text
[5. Application Layer]   --- Message  [HTTP, FTP] 
    |
[4. Transport Layer]     --- Segment  [TCP, UDP]   <-- Main operational boundary of Part 1 & 2
    |
[3. Network Layer]       --- Packet   [IP, ICMP]
    |
[2. Data Link Layer]     --- Frame    [Ethernet, MAC] <-- Takeover boundary of Raw Sockets in this section
    |
[1. Physical Layer]      --- Bit      [Cable, Fiber]
```


* **Why Do We Need Raw Sockets?**
When we use the conventional `AF_INET` to create a socket, the operating system takes full care of ARP resolution, routing lookups, and the concatenation of MAC/IP/TCP headers. To see these underlying binary bytes with our own eyes, or even to splice them manually, the conventional Socket API can no longer meet our needs. Therefore, in [01_rawsocket.c](../../../03_rawsocket_underlying_c/01_Preparation%2520for%2520a%2520Deep%2520Dive%2520into%2520the%2520Protocol%2520Stack/01_rawsocket.c), we turn to the highly destructive and controlling **Raw Socket**:
```c
int raw_sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

```


* **Parameter 1: From `AF_INET` to `AF_PACKET**`: This is a deep dive into the bottom layer. `AF_INET` is limited to the network layer and above, while `AF_PACKET` acts directly on the data link layer. The OS no longer strips the Ethernet frame header for you; instead, it converts the raw electrical signals received by the network card into a byte stream and throws it to you intact.
* **Parameter 2: `SOCK_RAW**`: Explicitly declares that we need raw message data that bypasses the protocol stack.
* **Parameter 3: `htons(ETH_P_ALL)**`: In previous chapters, we set this parameter to `0`, meaning "let the OS automatically infer the protocol." But at the data link layer, the network card only recognizes binary bits, and there is no default protocol to speak of. `ETH_P_ALL` is an ultimate command to process network frames of all protocols without filtering.
*(Note: Data transmitted in the network is in big-endian byte order, so it must be wrapped with the `htons` conversion. Otherwise, hardware matching at the network card will fail due to byte order confusion. This is also because multiple bytes are being compared here. If it were a single byte, this conversion would not be needed, as you will see in later examples.)*
* **Promiscuous Mode**
After obtaining raw socket permissions, we still face a physical obstacle: network card hardware is extremely selfish by default. It automatically discards unicast packets whose destination MAC address is not its own. To intercept broadcast, multicast, or even packets from other devices on the LAN (to expand our data samples), we must strip away the network card's filtering mechanism and force it into **promiscuous mode**.
In [02_promiscuous.c](../../../03_rawsocket_underlying_c/01_Preparation%2520for%2520a%2520Deep%2520Dive%2520into%2520the%2520Protocol%2520Stack/02_promiscuous.c), we demonstrate how to call the underlying hardware through the classic "read-modify-write" mechanism:
```c
extern int ioctl (int __fd, unsigned long int __request, ...) __THROW;
```


* **`__fd`**: Linux adheres to the philosophy that "everything is a file." You cannot issue commands directly to the string `"eth0"`; you must pass in a valid file descriptor. When we pass in the `raw_sock` we just created, the kernel sees that it's a network handle and immediately forwards this `ioctl` request to the Network Device Subsystem. *(Actually, passing in any ordinary TCP/UDP socket can achieve the same routing effect; using `raw_sock` here is purely out of convenience.)* Furthermore, the role of `ioctl` is not limited to the network subsystem. It is a backdoor channel designed by Linux developers to bypass conventional data streams and issue exclusive commands (Magic Codes) directly to hardware device drivers. By passing in other types of file descriptors, we can also operate other subsystems.
* **`__request`**: `SIOCGIFFLAGS` (Get) is used to read out the current state of the network card; `SIOCSIFFLAGS` (Set) is used to hard-write our modified state into the hardware.
* **Extreme Memory Reuse: `struct ifreq**`
* The last parameter of `ioctl` is a variadic parameter. In the network subsystem, we use the `ifreq` struct as a "standard briefcase" to communicate with the kernel:
```c
// This is a simplified version
struct ifreq {
    char ifr_name[IFNAMSIZ]; /* Interface name, e.g. "eth0" */
    union {
        struct sockaddr ifru_addr;    // Used to hold IP address (16 bytes)
        struct sockaddr ifru_hwaddr;  // Used to hold MAC address (16 bytes)
        short           ifru_flags;   // Used to hold status flags (2 bytes)
        int             ifru_mtu;     // Used to hold MTU (4 bytes)
    } ifr_ifru;
};
#define ifr_flags   ifr_ifru.ifru_flags
#define ifr_hwaddr  ifr_ifru.ifru_hwaddr
```


* This perfectly demonstrates kernel developers' extreme squeezing of memory: through a `union`, dozens of completely different parameters such as IP, MAC, and flags are forced to share the same 16-byte block of memory. How does the kernel distinguish what is currently in this memory block? It relies on the command macro you pass to `ioctl`. At the same time, the underlying `#define` syntactic sugar cleverly hides the complex internal union nesting, providing the application layer with a clean, struct-like calling experience. (The comprehensive implementation of `ifreq` is found in [ifreq](../../../03_rawsocket_underlying_c/01_Preparation%2520for%2520a%2520Deep%2520Dive%2520into%2520the%2520Protocol%2520Stack/03_ifreq.c)).
* Now we have successfully enabled promiscuous mode, but in actual operation, we face another problem: the `raw_sock` we created is bound to all network cards of this machine by default, such as `eth0`, `wlan0`, and the `lo` loopback interface. However, the `lo` interface here is only used for internal computer communication (such as local loopback testing) and will not send data packets through a real physical network card. Therefore, to discard ghost data packets that do not have a real physical MAC address (usually manifested as a forged MAC address of all 0s) from the captured data packets, we also need to cooperate with `ioctl` operations to firmly bind the socket to a network card with a specific physical mapping (such as `eth0`).
* The [bind](../../../03_rawsocket_underlying_c/01_Preparation_for_a_Deep_Dive_into_the_Protocol_Stack/04_bind.c) file demonstrates how to accurately bind to the `eth0` network card through an `ioctl` operation. In Linux systems, not only files but also hardware is abstracted relying on an underlying handle (Index). Therefore, we first obtain the real physical network card index mapped by the `eth0` network card through `ioctl`, and then perform a hard binding through the underlying `bind` operation. It is important to note that here we use the `sockaddr_ll` (Link-Layer) struct dedicated to the data link layer to package the data. This operational logic is completely consistent with our previous operation of binding IP and port numbers using the network layer's `sockaddr_in`, except the dimension has sunk to the physical link layer. *(Note that due to the restrictions of the `bind` function signature, we need to convert `sockaddr_ll` into `sockaddr` before passing it in.)*
* **At this point, we have successfully bypassed the layers of encapsulation in the operating system's protocol stack and obtained the highest physical control over the underlying network card. In the following sections, we will directly face the raw byte stream thrown up by the network card and experience the most brutal "pointer casting" parsing method in the C language.**

# 2. Pointer Casting and Decapsulation

Before we begin, we need a deeper analysis of network packets. In previous examples, we only needed to simply use `sendto` and `recvfrom` for network communication, directly sending and receiving the data we wanted.

But if you have some web development experience, you know that packet transmission involves not only data payload but also data headers. For example, a token is passed and verified through the packet header. Although we are not discussing network applications built on mature protocol stacks here, the operating system and web developers share a very similar goal during these operations: **to make the user unaware of the existence of data headers**. Now that we have a `raw_socket`, the packets we process are bare strings (or more accurately, raw byte streams) containing both headers and data payloads. The first thing we need to understand is how these bare strings are meticulously assembled and encapsulated step-by-step as they flow through the five-layer network architecture.

```text
+-----------------------------------------------------------------------+
|           Physical Layer: Electrical/Optical signals in the cable     |
+-----------------------------------------------------------------------+
                              | (Received by NIC and converted to binary byte stream)
                              V
+=======================================================================+
| L2 Link Header | L3 Net Header | L4 Trans Header |   L5 App Payload   |
|   (Ethernet)   |     (IP)      |   (TCP/UDP)     |(HTTP, FTP, or text)|
+=======================================================================+
|<- 14 Bytes --->|<- 20 Bytes -->|<- 20 Bytes ---->|<- Rest is all app payload area ------->|
|                                |                 |                                        |
|<-(Our current position)        |                 |                                        |


```

We have adopted the most intuitive way to represent the composition of data headers here. In our first part's example, when application layer data is sent, it is gradually encapsulated downwards.

The following diagram shows how application layer data is wrapped step-by-step into an Ethernet physical frame:

```text
================================================================================
The "Russian Nesting Doll" Encapsulation Process of Data Packets
================================================================================

1. Application Layer (L5)
   Generates pure business data.
   +-------------------------------------------------------------------------+
   |                       Application Data (HTTP / FTP / Custom Msg)        |
   +-------------------------------------------------------------------------+
                                 |
                                 V (Passed down)

2. Transport Layer (L4)
   Adds source/destination port numbers to determine which process to deliver to.
   +-------------------+-----------------------------------------------------+
   | TCP/UDP Header    |               Application Data                      |
   +-------------------+-----------------------------------------------------+
                                 |
                                 V (Passed down)

3. Network Layer (L3)
   Adds source/destination IP addresses to determine which host globally to deliver to.
   +-------------------+-------------------+---------------------------------+
   |    IP Header      | TCP/UDP Header    |         Application Data        |
   +-------------------+-------------------+---------------------------------+
                                 |
                                 V (Passed down)

4. Data Link Layer (L2)
   Adds source/destination MAC addresses to determine which NIC in the LAN to deliver to.
   +-------------------+-------------------+-------------------+-------------+
   | Ethernet Header   |    IP Header      | TCP/UDP Header    | Payload ... |
   +-------------------+-------------------+-------------------+-------------+


```

Now we understand that the five-layer network model is not a purely academic abstraction, but an abstraction of the actual operational process during network transmission. Each layer corresponds to protocols that implement specific functions, providing a design foundation for the upper layers (and acting as the upper layer's design foundation). Ultimately, the topmost layer forms the practical interface of the computer network. These interfaces interconnect to build the massive system of computer networks.

In this section, we will parse the fully-formed data packet we captured step-by-step using pointer casting and decapsulation.

## 1. Parsing the Ethernet II Header

* Ethernet is the header of the data link layer. It is the header present at the very beginning of all data flows in an Ethernet network.

Let's first look at the composition of this packet:

  `text   +---------------------------------------------------------------+   |             Ethernet II Header (Ethernet Frame Header) - 14 Bytes        |   +-----------------------+-----------------------+---------------+   |    Destination MAC    |      Source MAC       |   EtherType   |   |       (6 Bytes)       |       (6 Bytes)       |   (2 Bytes)   |   +-----------------------+-----------------------+---------------+   `

* When data is transmitted as optical/electrical signals through physical cables, the underlying switches and network cards have no idea what an IP address is; they only recognize the unique burned-in identifier of the physical hardware—the MAC address (Media Access Control address). The IP address is responsible for "logical addressing" across the global network, while the MAC address is responsible for accurately "transporting" data point-to-point from one physical port to another within the same local area network (subnet). As the outermost defense and the "shipping box" of a network packet, the Ethernet frame header naturally bears the physical network card's hardware address.
* The front-loading design of the MAC address is a brilliant piece of hardware-level thinking. In the early days of computer network design, hardware buffers in network cards and switches were extremely expensive and limited. When a data frame is converted into electrical signals and flows sequentially into the network card like a stream, the network card must decide whether to accept or drop the packet **in the shortest possible time**. By placing the destination MAC address at the very beginning (bytes 0-5), the network card only needs to read the first 6 bytes to arrive and immediately compare them with its own hardware MAC address. If it finds the packet isn't addressed to it (and isn't a broadcast packet), the network card can make a prompt decision to drop all subsequent incoming signals directly at the hardware level, greatly saving precious memory buffers and CPU interrupt resources.
* To facilitate learning and understanding during header parsing, we will print out the key parts. When sniffing packets, we create a storage array of size 65536. This size can accommodate any IP packet; the specific reason for this size will be explained in detail in the IP header section.
* When processing this header, the `<linux/if_ether.h>` header file provides us with the corresponding struct for convenient operation.
 
  `c   #if __UAPI_DEF_ETHHDR   struct ethhdr {       unsigned char h_dest[ETH_ALEN];   /* destination eth addr */       unsigned char h_source[ETH_ALEN]; /* source ether addr  */       __be16        h_proto;            /* packet type ID field */   } __attribute__((packed));   #endif   `
* In fact, using this struct achieves the exact same effect as extracting data from the corresponding positions using array subscripts. Adopting this header is merely for convenience; essentially, it gives a name to a physical segment of a corresponding size within actual continuous memory space, with no fundamental difference between the two methods. This is the charm of C language systems programming—we do not allocate new memory to copy the data; we simply place a 14-byte transparent "mold" over this raw byte stream.
* In the actual code example, you can print the MAC address using the following method:
  `c         printf("Dest Mac : %02x %02x %02x %02x %02x %02x\n", eth->h_dest[0], eth->h_dest[1], eth->h_dest[2],eth->h_dest[3], eth->h_dest[4], eth->h_dest[5]);   `
* Because we are operating byte-by-byte here, we don't need to worry about byte order. However, if you want to print multiple bytes or process the EtherType information (i.e., the `h_proto` field), byte order conversion is required. This is because the default transmission over network cables uses big-endian (network byte order), while our hosts typically use little-endian. We must use `ntohs(eth->h_proto)` to translate it, converting it to the host's byte order to correctly determine the protocol type.
* The specific code example can be found in [parse_Ethernet](../../../03_rawsocket_underlying_c/02_Pointer_Casting%2526%2526Decapsulation/01_parse_Ethernet.c).
* In this section, we successfully parsed the information in the Ethernet header, understood its hardware-smart field layout, and accurately extracted the corresponding MAC addresses and underlying protocol type using struct pointers. This declares that we have completely taken over the first 14 bytes on this long ribbon of memory bytes. Next, what we need to do is push the pointer forward in the memory space by 14 physical increments (`buffer + sizeof(struct ethhdr)`), officially crossing the boundary of the data link layer and stepping into the structurally more complex **L3 Network Layer (IP Header)**, which governs global routing and addressing.

## 2. Parsing IP && ARP Data Headers

* IP (Internet Protocol) is the cornerstone of today's internet, carrying the majority of data transmission in the modern web. The introduction of IP addresses officially allowed the flow of data packets to cross from the Local Area Network (LAN) level into the vast expanse of the global Wide Area Network (WAN). Simply understanding an IP address as a network mapping of a MAC address is slightly biased: a MAC address is a globally unique, burned-in identifier of the underlying physical hardware (specifically, the network card); whereas an IP address is essentially a **logical addressing point**, typically assigned dynamically or statically by a router within a LAN. Routers determine the routing direction of data by dividing specific **IP segments (subnets)**. In this LAN, outbound data packets must first pass through the router (gateway), which then relays them to the host in the target LAN.
* **Summary of the core functions of IP subnets:**

1. **Dividing Broadcast Domains and Routing Boundaries:** Through the Subnet Mask, an IP address is logically divided into a "Network ID" and a "Host ID". Routers rely on the Network ID to determine whether the destination is in the same LAN. If it is not, the packet is handed over to the gateway for forwarding. This effectively isolates broadcast storms within the LAN.
2. **Special Purposes of Specific Subnets (Loop Prevention and Intranets):** Our efforts to avoid (or deliberately utilize in some scenarios) loopback data packet transmission are implemented based on specific IP subnets. For example, `127.0.0.0/8` is a specially reserved local Loopback subnet; data packets sent to this subnet will never touch the physical network card, but will "turn back" directly within the system kernel's network stack, dedicated entirely to inter-process communication on the local machine. Additionally, private subnets like `192.168.x.x` are dedicated to internal LANs and, combined with NAT technology, do not directly participate in public network routing.

* At the data link layer, the Ethernet frame structure is the mandatory underlying carrier for the vast majority of modern computer network communications. However, once the data is decapsulated and enters the Network Layer (L3), protocol headers begin to diversify richly based on different application scenarios and requirements. The five network layer protocol families we introduce here collectively build the basic skeleton of the computer network world:
1. **IP (Internet Protocol):** The foundation of the underlying network and the most core and primary method of communication. It is responsible for routing data packets from source to destination, providing a connectionless, best-effort datagram delivery service.
2. **ICMP (Internet Control Message Protocol):** Responsible for diagnostics, error reporting, and network status probing during IP protocol communication. It is directly encapsulated within the IP packet's payload and does not require support from a higher-layer transport protocol; instead, it is identified by the "Protocol" field in the IP header (with a value of 1). The `ping` command (Echo Request/Reply) we use daily for troubleshooting networks is implemented based on this protocol.
3. **ARP (Address Resolution Protocol):** Strictly speaking, ARP is a critical protocol operating between the network layer and the data link layer (often called layer 2.5). Its sole mission is to facilitate the mapping and translation between IP and MAC addresses: when the target IP is known, it broadcasts a query on the LAN to find its physical MAC address, thereby binding the logical address to the physical address.
4. **IGMP (Internet Group Management Protocol):** A multicast management protocol. This is an efficient one-to-many packet distribution mechanism that allows routers to know which hosts on the LAN have joined a specific "multicast group". It is commonly used for IPTV, live video streaming, or online meetings, significantly saving network bandwidth.
5. **IPsec (Internet Protocol Security):** Encrypts and authenticates IP packets directly at the network layer. It ensures the confidentiality and integrity of data transmitted over public networks, and is commonly used for establishing secure tunnels in enterprise-level VPNs (Virtual Private Networks).


* To ensure the logical coherence and focus of this document, we will concentrate on parsing the **ARP** and **IP** protocols in this section. The former is responsible for mapping IP addresses to MAC addresses, bridging the logical and physical worlds; the latter truly opens the door to wide-area network communication. Since protocol headers become increasingly complex as we move up the layers, for the sake of readability, we will use the RFC standard ASCII art format to display packet headers from here on out, focusing only on their most core fields.

**1. ARP (Address Resolution Protocol)**


```text
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Hardware Type (2 Bytes)     |   Protocol Type (2 Bytes)     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| HW Size (1 B) |Proto Size(1 B)|     Opcode (2 Bytes)          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                Sender MAC Address (First 4 Bytes)             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Sender MAC (Last 2 Bytes)      | Sender IP Address (First 2 B)|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Sender IP (Last 2 Bytes)       | Target MAC Address (First 2 B)|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                Target MAC Address (Last 4 Bytes)              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                Target IP Address (4 Bytes)                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```


* **Hardware Type:** Usually a value of 1, representing an Ethernet header.
* **Protocol Type:** Acting as the translator between MAC and IP addresses, this refers to the protocol type that needs mapping. For example, `0x0800` represents IPv4.
* **HW / Proto Size:** Defines the lengths of the physical hardware address (MAC, usually 6 bytes) and the logical protocol address (IP, usually 4 bytes), respectively.
* **Opcode:** Represents the current ARP message operation type. A value of `1` represents an ARP Request (broadcast), and `2` represents an ARP Reply (unicast response) from the host with the corresponding IP address.
* **Sender / Target Addresses:** The combinations of sender and receiver addresses (MAC + IP).
* In [parse_ARP](../../../03_rawsocket_underlying_c/02_Pointer_Casting%26%26Decapsulation/02_parse_ARP.c), we demonstrate how to dissect the core parameters of ARP. When handling protocol parsing here, we will rely entirely on the underlying memory layout. On one hand, this intuitively shows you the continuity of underlying network packet encapsulation; on the other hand, it highlights the powerful operations of the C language in data anchoring through **pointer casting and arithmetic**.

```c
// 'buf' is the complete Ethernet frame data we captured via the Raw Socket
// Move the pointer forward by the size of an Ethernet header to accurately locate the starting memory position of the ARP protocol header
struct ether_arp *arp = (struct ether_arp*)(buf + sizeof(struct ethhdr));
```


* Just like the previous section, and something we will continue to use in subsequent code: we directly map and parse specific protocol headers by defining wrapper structs (such as `struct ether_arp`). As you can see from the graphical message display at the beginning of this section, the headers of various network protocol layers are strictly and continuously arranged in actual physical memory (or the send buffer). Therefore, we only need to offset the character array pointer pointing to the start of the packet (the pre-decayed array `buf`) forward by the size of the previous network layer header.
* This precisely reflects the greatest advantage of using **struct casting**: we can directly use syntax like `sizeof(struct ethhdr)` to calculate the offset, without having to hardcode the exact number of bytes to shift (which is 14 bytes for the Ethernet header in this example). This not only greatly reduces the probability of errors when calculating offsets but also significantly improves the readability and maintainability of low-level network C code.
* Regarding the subsequent extraction of data inside the ARP header, the logic is actually very straightforward (even arguably mundane). The core is simply to extract the encapsulated data from the struct and format it for printing. However, in low-level C network programming, there is a highly error-prone critical point to note when extracting data: **network byte order conversion**. To solve this problem, we use network byte order conversion when extracting the ARP `opcode`, while the `ip` and `mac` addresses are printed byte by byte (consistent with the logic used for processing the Ethernet II header), just as you can see in [parse_ARP](../../../03_rawsocket_underlying_c/02_Pointer_Casting%26%26Decapsulation/02_parse_ARP.c).

**2. IP (Internet Protocol)**

* Now we have resolved the mapping relationship between MAC addresses and IP addresses (ARP protocol). From this moment on, isolated LANs in the computer world are connected, and two hosts across the public internet can finally locate each other and communicate. The core protocol shouldering this cross-subnet communication is none other than the **IP protocol (Internet Protocol)**.
```text
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Version|  IHL  |Type of Service|          Total Length         |
| (4 bit) (4 bit)     (8 bit)   |            (16 bit)           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Identification        |Flags|      Fragment Offset    |
|             (16 bit)          |(3 b)|          (13 bit)       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Time to Live |    Protocol   |         Header Checksum       |
|     (8 bit)   |     (8 bit)   |            (16 bit)           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Source IP Address                       |
|                             (32 bit)                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Destination IP Address                     |
|                             (32 bit)                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Options                    |    Padding    |
|               (Optional, usually absent)      |               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

```


* **IHL (Internet Header Length):** Occupies the lower four bits of the first byte and is used to identify the length of the entire IP header. Because the IP header contains fields like Options, its actual length is not fixed (typically 20 bytes, maximum up to 60 bytes). Since a 4-bit binary can only represent up to 15, the unit of measurement here is **4 bytes (32-bit words)**. That is to say, actual header length = `IHL value * 4`. This dynamically calculated length is also the absolute basis for us to step over the IP header and perform pointer displacement to find the starting point of the upper-layer payload later.
* **Total Length:** This unit records the total length of the entire IP data packet (including IP Header + L4 Transport Header + Business Data). Because data is encapsulated from top to bottom when the system sends a packet, and at this stage the packet has not yet reached the data link layer, this field only includes the length of the network layer and above. However, this is harmless, since the size of the Ethernet frame header is always fixed. This unit has 16 bits, and 2 to the power of 16 is 65536. This is also the fundamental reason why we set the receive buffer size to 65536 in the code for this chapter: **to ensure that the largest possible IP packet can be completely accommodated in memory all at once**.
* **Time to Live (TTL):** Despite the name "Time", it actually describes not physical time, but a **"hop count" limit** for the data packet. To prevent routing misconfigurations from causing ghost packets to fall into infinite loops in the network, this 8-bit value is automatically decremented by 1 every time the packet passes through a router. When it reaches 0, the router will directly drop the packet and send an ICMP timeout packet back to the source host (this is the underlying principle behind using `traceroute` to track routing nodes).
* **Protocol (Upper-layer Protocol Type):** Serving a similar purpose to `EtherType` in the Ethernet header, this is the dividing line for the handover from L3 (Network Layer) to L4 (Transport Layer). It determines whose data the payload behind the IP header actually belongs to: `1` represents ICMP, `6` represents TCP, and `17` represents UDP. Since this is represented by only a single byte (unlike the 2-byte Ethernet type, and because the number of core upper-layer protocols here is small), there is no need for network byte order conversion.
* **Source / Destination IP Address:** Located at bytes 12 through 19. They store the 32-bit binary machine code IP addresses corresponding to the source IP and destination IP, respectively.
* In the code [parse_ip](../../../03_rawsocket_underlying_c/02_Pointer_Casting%2526%2526Decapsulation/03_parse_IP.c), we use the `struct iphdr` struct provided by the Linux kernel to map the IP header. Similar to parsing the ARP protocol, we need to shift the pointer forward during decapsulation. What requires **special attention** here is that we must extract the `ihl` field and multiply it by 4 to calculate the offset. Although the base size of `struct iphdr` in C is 20 bytes, actual incoming IP packets may contain Options. Only by relying on the dynamically calculated `ihl * 4` length to stride across the entire IP header can the pointer accurately land on the starting position of the next stage (such as the TCP header).
* You might notice an interesting detail in the code: our handling methods for printing IP addresses are completely inconsistent between the IP protocol and the ARP protocol. Fundamentally, this is due to differences in the underlying C language design of their packet structures:
* In `struct ether_arp`, the IP is stored as `unsigned char arp_spa[4]` (a single-byte character array).
* In `struct iphdr`, the IP is stored as `__be32 saddr` (a 32-bit unsigned big-endian integer).
Although both occupy 4 bytes in the underlying physical memory, this difference in C language data structure encapsulation leads to us calling different APIs. For a character array, we simply iterate and print; but for a 32-bit integer, due to historical API limitations, if we want to call `inet_ntoa()` to translate it into a human-readable string, we must wrap it in a `struct in_addr` struct before passing it in for printing.


* The fundamental reason for this design discrepancy lies in the different original intentions and purposes of the two protocols. The ARP protocol must handle not only IP addresses but also hardware addresses with dynamically changing lengths (like MAC). When facing such dynamically changing data, the most universal approach is to use a single-byte array. On the other hand, the IP header structure is highly customized specifically for the IP protocol. Directly using a 32-bit integer (`__be32`) is not only compact in memory structure but, more importantly: **this design allows the CPU to quickly compare the IP address against a subnet mask using bitwise operations in just a single clock cycle.** In the routing and addressing process where extreme efficiency is pursued, the efficiency of such integer operations far exceeds that of array traversal.
```c
// 1. Get the starting physical memory address of the 32-bit integer
// 2. Forcefully wrap it with a "single-byte pointer" wrapper
unsigned char *src_ip_bytes = (unsigned char *)&iph->saddr;
unsigned char *dst_ip_bytes = (unsigned char *)&iph->daddr;

// 3. Avoid big/little-endian conversion and static buffer traps, reading physically byte-by-byte directly
printf("    Source IP        : %d.%d.%d.%d\n", 
        src_ip_bytes[0], src_ip_bytes[1], src_ip_bytes[2], src_ip_bytes[3]);

printf("    Destination IP   : %d.%d.%d.%d\n", 
        dst_ip_bytes[0], dst_ip_bytes[1], dst_ip_bytes[2], dst_ip_bytes[3]);
```


* Of course, as I emphasized before, there is no difference between the two in the bottom-most physical memory. Therefore, we can completely adopt the "pointer casting" method mentioned above to directly read this 32-bit integer like an array and complete the IP printing.

By parsing `struct iphdr`, we have unveiled the internet's "express delivery system". The IP protocol is essentially a **connectionless, best-effort** datagram service. It does not care whether packets are lost, nor is it responsible for the order in which data arrives. Its sole mission is to use the destination IP address to try its utmost to throw the data packet to the doorstep of the other party's LAN through the intricate global routing nodes. As for what to do if data is lost? Or if the order is messed up? That is entirely left to the next layer—the TCP protocol—to worry about.

At this point, we have successfully completed our core exploration of the Network Layer, and our perspective has shifted from pure physical MAC addressing within a LAN to the vast global internet IP routing system. In subsequent chapters, we will stand on the shoulders of the IP protocol to dive deeply into the specific end-to-end communication establishment process and packet sending mechanisms.

## 3. Parsing the TCP Data Header

* In the previous section on the IP protocol, we solved the problem of how a data packet finds the target **Host** in the vast internet. But this is not enough. A modern computer runs hundreds or thousands of processes simultaneously (such as your browser, WeChat, and background SSH services). When the network card receives an IP packet, which process should the operating system hand it to?
* This highlights the necessity of the **Port** design. If the IP address is the "building number" of a building, then the port is the "room number" for each company inside that building.
* **The connection between Ports and PCBs (Process Control Blocks):** In the Linux kernel, every running program is managed by a **PCB (Process Control Block)**, which records the process's unique identifier PID (Process ID) and all the file descriptors (including Sockets) it has opened. When a process calls the `bind()` function to bind to a certain port (like port 80), the kernel establishes a mapping record in an internal network hash table: `[Port 80] <---> [Socket Struct] <---> [Process PCB / PID]`. Therefore, a port is essentially a "mailbox" provided by the kernel to the application layer. When a TCP message arrives carrying a destination port number, the kernel can follow the clues to accurately wake up the process belonging to the corresponding PCB, telling it: "Your letter has arrived, hurry up and read the Socket to receive the data!"

```text
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Source Port          |       Destination Port        |
|            (16 bit)           |            (16 bit)           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Sequence Number                        |
|                  (32 bit - Ensures ordered delivery)          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     Acknowledgment Number                     |
|                  (32 bit - Tells the other side I received it)|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Data |           |U|A|P|R|S|F|                               |
| Offset| Reserved  |R|C|S|S|Y|I|           Window Size         |
| (4 b) |  (6 b)    |G|K|H|T|N|N|  (16 bit - Flow control/Window)|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|            Checksum           |         Urgent Pointer        |
|            (16 bit)           |            (16 bit)           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Options                    |    Padding    |
|               (Optional, variable length)     |               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

```

* **Source / Destination Port:** Each occupies **16 bits (2 bytes)**, therefore the maximum value for a port is $2^{16}-1 = 65535$. This is the unique identifier the system uses to distinguish different network communication processes.
* **Data Offset (Header Length):** Its function is exactly the same as the `IHL` field in the IP header. It tells us exactly how long the TCP header is, and the unit is also **4 bytes (32-bit words)**. Because TCP also supports Options fields, the header length is dynamic (usually 20 bytes, maximum 60 bytes). We must rely on this field to accurately skip over the TCP header to find the real application layer data.
* **Flags (Control Flags):** These are core fields used to construct the TCP three-way handshake, four-way teardown, and state transitions:
* **SYN (Synchronize):** Initiates a connection and synchronizes sequence numbers.
* **ACK (Acknowledgment):** Confirms receipt of data.
* **FIN (Finish):** The sender has finished sending data; gracefully requests to close the connection.
* **RST (Reset):** A serious error occurred; forcibly resets/rejects the connection.
* **PSH (Push):** Urges the receiving end's kernel to quickly "push" the buffered data up to the application layer process, rather than waiting for the buffer to fill up.


* Now let's look at the code parsing part [tcp_parse](../../../03_rawsocket_underlying_c/02_Pointer_Casting%2526%2526Decapsulation/03_parse_TCP.c). After moving the pointer to the start of the TCP header by parsing the IP header's `IHL` field, we extract the port numbers and Flags. Next, we need to calculate the starting position and length of the real **application layer Payload**, to then extract the substantive content of the transmission (such as HTTP text):
```c
  // Payload offset = Ethernet Header (14) + IP Header Length (dynamic) + TCP Header Length (dynamic)
  int payload_offset = (sizeof(struct ethhdr) + ip_header_len + tcp_header_len);
  // Actual payload length = Total captured packet length - Total length of all protocol headers
  int payload_len = data_size - payload_offset;

```


* **Why use `isprint()` and `putchar()` when extracting the payload?**
* In C, when we usually use `printf("%s")` to print a string, the underlying implementation relies on the convention that it ends with a `\0` (NULL byte). However, **the raw data transmitted over the network is a pure binary stream and does not come with a built-in terminator**. If you print directly with `printf`, the program might crash due to out-of-bounds access, or the output might be prematurely truncated if it encounters a randomly appearing `\0` in the middle of the packet.
* Furthermore, the network payload is not necessarily pure text (like HTML source code); it is often mixed with pure binary content such as encrypted data (TLS), compressed files, or images. These binary data contain a large number of **invisible control characters** (such as the bell character `\x07` or backspace `\x08`). Forcing them to print to the terminal will cause garbled text or even formatting crashes on the terminal display.
* Therefore, we must use a definitive `payload_len` combined with a `for` loop to iterate byte by byte. Use the `isprint()` function to determine whether the byte is a human-readable ASCII character (or manually check for `\r`, `\n` newline characters). If it is, output it normally with `putchar()`; if not, uniformly output a dot `.` as a safe placeholder. This "Hexdump-style" safe output method is a standard convention for all low-level packet sniffing software (like Wireshark, tcpdump) when displaying raw messages.

At this point, we have completed the full "decapsulation" journey from the physical network card (MAC), to network routing (IP), to process dispatching (TCP port). Above the unreliable IP layer, the TCP protocol forcibly constructs a **reliable, connection-oriented byte stream communication pipeline** through sequence numbers, acknowledgment responses, and sliding window mechanisms. Through precise pointer offsets and forced type casting, we peeled away the layers of protocol packaging, ultimately touching the soul of computer network communication—the application layer payload.

Through the deep disassembly in this section, we have completely broken away from the greenhouse created by high-level APIs (like `send` / `recv`). From the perspective of a low-level systems programmer, holding the C language's "scalpel" (pointer casting and offsets), we performed a precise, outside-in dissection of the raw network byte stream. The entire decapsulation process is essentially a pointer relay continuously pushing forward in a contiguous memory space. Having experienced the layer-by-layer disassembly of data headers, we verified the nested structure of the five-layer network model in actual physical memory, transforming the abstract network architecture into a profound insight into the bottom layers of computers. In the following chapters, we will turn passive into active, attempting to personally construct and send a TCP message based on `raw_socket`.

