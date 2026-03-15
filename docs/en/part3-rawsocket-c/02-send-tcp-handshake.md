# 3. Sending the TCP Handshake Packet

* In the previous packet dissection section, we only needed to perform pointer offsets on an already received complete data packet and apply the corresponding protocol struct shell to read the data. In the sending part of this section, we need to delve into previously ignored technical details and manually assemble a data packet from scratch, to truly experience how the computer protocol stack ensures the correctness and rigor of data transmission at the lowest level.
* First, let's look at the memory layout of protocol structs. Taking the data link layer's `ethhdr` as an example, if you try to write this struct by hand in your code:
```c
  struct my_ethhdr {
    unsigned char  h_dest[6];
    unsigned char  h_source[6];
    unsigned short h_proto;
};
```


* Calculating literally, you would naturally think the size of this struct is 6 + 6 + 2 = 14 bytes. However, if you try to print it using `sizeof()` with `printf`, you might find the compiler telling you it's 16 bytes! This is because when the CPU addresses through the memory bus, to improve reading efficiency, it prefers to process data aligned to 4-byte or 8-byte boundaries. Therefore, the compiler takes the liberty during the compilation phase to add two bytes of blank padding at the end (or in the middle) of the struct.

This is exactly why when system header files like `<linux/if_ether.h>` encapsulate network protocol structs, they always include an `__attribute__((packed))` at the end. Only by adding this compiler modifier can we forcibly disable byte alignment optimization and truly map the continuous physical memory transmitted over the network cable flawlessly using C language structs.

* The second key detail is the **Checksum**. This field is primarily used to verify whether the data packet has suffered content corruption during physical transmission. When writing in high-level languages, we often take it for granted that data transmission is absolutely reliable; but in real physical networks, electrical signals attenuate, cosmic rays might cause bit flips in memory, and routers might corrupt packets due to internal congestion or hardware failures. Facing such an uncontrollable physical network, the kernel's solution is: **trust no data that hasn't mathematically proven itself**. The TCP/IP protocol stack ensures the reliability of logical connections through a "16-bit one's complement sum" checksum mechanism. When sending a packet, the sender must pre-calculate and fill in the checksum; during receiver parsing, the kernel extracts the message, recalculates, and compares it. If they don't match, the packet is immediately discarded.
```c
  unsigned short checksum(void* b,int len){
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;
    for(sum = 0; len > 1 ; len-=2){
      sum += *buf++;
    }
    if (len == 1){
      sum += *(unsigned char*)buf;
    }
    sum = (sum >> 16)+(sum & 0xFFFF);
    sum +=(sum >> 16);
    result = ~sum;
    return result;
  }
```


* In the IP protocol, the checksum is calculated solely over the **protocol header**. We first need to understand the core algorithmic logic of this classic code: it treats the contiguous memory data as a sequence of 16-bit (2-byte) numbers and adds them all up; if an overflow (carry) occurs during the addition, the carry is added back to the lowest bit; finally, the total sum is bitwise inverted. This design philosophy was largely a compromise with the extremely limited computing power of CPUs in the early days of the internet: addition instructions execute extremely fast, the final result can concisely fit into a 2-byte space, and this algorithm has excellent mathematical properties—the checksum logic is perfectly compatible regardless of whether the machine uses big-endian or little-endian storage.
* It is important to note that data validation in the network is not solely the responsibility of the network layer. It is a process of layered collaboration: the underlying **Data Link Layer** achieves strict bit-level error correction through CRC/FCS algorithms; the topmost **Application Layer** prevents deliberate malicious tampering through cryptographic means like TLS/SSL certificates. Caught in the middle, the core principle of network validation for the IP/TCP layers is: **exchange the fewest CPU clock cycles for a "good enough" error interception rate**.
* Let's deeply analyze this seemingly simple, yet incredibly clever C code. It uses an `unsigned short` pointer to step through memory in two-byte increments, while using an `unsigned int` (32-bit) to store the addition result. The essence lies in **temporarily storing the carried-over data intact after an overflow**. Since the finally returned checksum field must be 16 bits, the function return value remains an `unsigned short`.
* After the code splits and directly adds values via the `for` loop, the subsequent `sum = (sum >> 16) + (sum & 0xFFFF);` is the finishing touch: `(sum >> 16)` extracts the carry data that overflowed into the upper 16 bits, and `(sum & 0xFFFF)` isolates the valid result of the lower 16 bits. Adding the two achieves "carry wrap-around". The subsequent `sum += (sum >> 16);` handles the extremely rare secondary overflow that might have been triggered by the previous addition. Finally, `result = ~sum;` completes the bitwise inversion, and the job is done.
* So far, we have successfully clarified the checksum calculation involving the IP header. However, when you set out to calculate the **TCP field checksum**, things get a bit tricky. The TCP protocol specification mandates that when calculating the checksum, some key information from the IP header (such as the source and destination IPs) must also be included in the calculation. When you become familiar with the top-down assembly process of protocols, you will notice a chronological contradiction: **when building the TCP header, the underlying IP header has not yet been attached to the data packet!**
* Why does the TCP protocol insist on this design? As we discussed earlier, the IP header contains a TTL field. Every time a packet goes through a router hop, the TTL is automatically decremented by one, and the router subsequently recalculates the IP checksum. In extremely rare cases, if a hardware failure in a router causes a bit flip in the "Destination IP Address" within the IP header, and the recalculated IP checksum coincidentally passes, this packet would be wrongly delivered to an unrelated host. To prevent this fatal error of ending up poles apart, TCP decided to double-insure itself: it forcibly pulls the IP addresses into its own checksum calculation. This way, even if the underlying IP address is accidentally altered in transit, the TCP checksum will absolutely fail to match upon reaching the destination, thereby avoiding the reception of erroneous data.
* To solve the unauthorized calculation problem of "using IP addresses to validate a not-yet-fully-encapsulated IP packet," we must define inside our code a **"pseudo-header" that does not exist in the actual network cable but serves merely as a temporary mold during host memory calculations**:
```c
  // Pseudo-header: An extra guarantee from the transport layer crossing over into the network layer
struct pseudo_header {
    unsigned int source_address; // Source IP Address (4 Bytes)
    unsigned int dest_address;   // Destination IP Address (4 Bytes)
    unsigned char placeholder;   // Must be filled with 0 (1 Byte)
    unsigned char protocol;      // Protocol number, TCP is 6 (1 Byte)
    unsigned short tcp_length;   // TCP Header Length + Payload Length (2 Bytes)
} __attribute__((packed));       // Echoing the previous section: Absolutely do not allow the compiler to insert even a single byte of Padding here!

```


* Pay special attention to the `placeholder` field. Without this field, the entire pseudo-header length would be an odd 11 bytes. To conform to the underlying habit of modern CPUs processing data in units of 4-byte (or 8-byte) even boundaries, and to facilitate the subsequent 16-bit checksum accumulation, protocol designers deliberately filled a `0` here to round it up to 12 bytes.
* In the accompanying code [send_syn_package](/03_rawsocket_underlying_c/03_Send_Package_Of_Tcp/sendtcp.c), we detail how to assemble and send a TCP SYN handshake packet from scratch using `raw_socket`. Interestingly, you can directly run the [parse_tcp](../../../03_rawsocket_underlying_c/02_Pointer_Casting%2526%2526Decapsulation/04_parse_TCP.c) packet analysis program we wrote in the second part of this chapter simultaneously to monitor the packet you just sent in real-time. Of course, you need to replace the macro definitions or global variables at the beginning of the code with your local machine's actual MAC address and IP address, depending on your network environment. If you are in WSL or a Linux environment, you can also use the following command to turn on the system's built-in geek packet capture tool for monitoring:
```bash
  sudo tcpdump -i eth0 -n -vvv -X dst host <your_target_ip> and tcp port 80

```


* Reviewing the logic flow of the sending code: we first allocated a contiguous byte array on the stack as buffer memory, and then used corresponding struct pointers (`ethhdr`, `iphdr`, `tcphdr`, etc.) to perform gridded partitioning and encapsulation on the header of this array (since we know the total volume of the handshake packet in advance, allocating this memory can be relatively casual). The next job is to assign values to each field just like filling out a form; we won't belabor the details here. What truly warrants vigilance is: **any data structure larger than a single byte must be converted to network big-endian byte order via `htons()` / `htonl()` before being filled in**; and dotted-decimal IP strings must be translated into 32-bit unsigned integers via `inet_addr()`. To accommodate readers with less exposure to low-level languages, let's briefly review the powerful C memory manipulation tools that frequently appear in these codes.
* Before we begin, we need a consensus: from the low-level perspective of C, various advanced data structures and arrays are essentially just a contiguous block of physical memory, and strings rely on `\0` to mark the end of data. Based on this, you can see through the essence of the conversions between arrays, pointers, and memory operations in the code:
* **`memcpy`**: Ignores types and directly clones a memory block of a specific size as a whole. When concatenating the pseudo-header and calculating the checksum, we precisely control the boundary of data copying by specifying explicit byte sizes, preventing memory overflows.
* `strcpy` and `strncpy`: Born specifically for string copying. Under the hood, `strcpy` utilizes pure pointer displacement to fill characters one by one into the target address until it hits `\0`. Unlike the fixed-length copying of `memcpy`, `strncpy` adds a maximum length threshold to prevent overflows. Here, an easily confused low-level cognitive illusion must be clarified. When writing code, we often comfortably **"treat a pointer exactly as a safe contiguous memory array for operations"**, which is largely a "privilege" illusion granted to us by Stack memory. When you declare `char buf[1024]` on the stack, the compiler clearly grasps the physical boundaries of this territory during the allocation phase; the array name as a whole represents a defined space, and it only decays into a pointer when passed as an argument. However, once you step out of the greenhouse of the stack—such as facing a pure raw pointer, or space carved out by `malloc` in Heap memory—you must constantly stay alert: **a pointer is merely a signpost pointing to a starting point; it carries absolutely no information about the "capacity limit"**. If you lose the natural sense of boundaries of a stack array and blindly call `strcpy` to write with the inertial thinking that "it is a safe array" (or just writing data to a raw pointer via `strcpy`), it is highly likely you will cross that invisible boundary, triggering catastrophic Memory Corruption.
* **`strlen`**: Beginners often mistakenly think it's a tool for "measuring the physical size of an array," which is inaccurate. `strlen`'s only logic is to start from the pointer address you give it, check backward byte by byte until it hits the first `\0`, and return the number of steps taken. Therefore, after using `strcpy` to write a paragraph of text, passing the starting address to `strlen` again will precisely measure the dynamic payload length we just wrote.
* Mastering these, you can thoroughly understand the thrilling art of memory manipulation in this sending code: how C allows us to directly control physical memory at the lowest level, completing data writing at the absolute coordinates required by the specification through pure pointer offsets and nested protocol molds; while simultaneously elegantly calling the host network byte order conversion functions to bridge the hardware gap between "big-endian machines" and "little-endian machines" in the vast heterogeneous internet.
* Up to this point, we have successfully reverse-engineered the entire puzzle of the protocol stack using Raw Sockets, manually crafting and sending a valid TCP SYN handshake packet from scratch. In this process, we pierced through the encapsulation illusion of high-level network libraries, experienced the pitfalls of handling struct memory alignment, manually calculated the dual checksums for IP and TCP, and solved the cross-layer validation puzzle using pseudo-headers.
Mastering this low-level ability to directly "doodle on the network cable," your horizons will absolutely no longer be limited to sending a simple handshake packet—as long as you can follow the memory layout rules of the corresponding protocol, you can freely modify the control flags (Flags) and Payload in the code to freely construct RST attack packets, forge source IP datagrams, or even implement a custom network protocol exclusively your own.

