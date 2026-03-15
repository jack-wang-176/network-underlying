
# 01 Basic (Fundamentals)
The cornerstone of network communication, primarily addressing data representation differences across layers.

* **[01_endian](../../../01_socket_underlying_c/01_basic/01_endian.c)**
    * Demonstrates the difference between **Little-Endian** and **Big-Endian**.
    * **Why the difference?** This is a historical legacy of CPU architectures (e.g., Intel x86 chose Little-Endian, while early Motorola chose Big-Endian). To prevent chaos, network protocols mandate **Big-Endian** as the standard "Network Byte Order." Therefore, we must convert host Little-Endian data before sending packets.
  
* **[02_htol_htons](../../../01_socket_underlying_c/01_basic/02_htol_htons.c)**
    * Based on `<arpa/inet.h>`.
      ```c
      extern uint16_t htons (uint16_t __hostshort)
      __THROW __attribute__ ((__const__));
      ```
    * Implements conversion from **Host Byte Order** to **Network Byte Order** (e.g., `htonl`, `htons`).
    * **Note on `__THROW` and `__attribute__`**: These are hints for the compiler. `__THROW` tells the compiler the function won't throw exceptions, and `__const__` indicates it's a "pure function" (depends only on input, no side effects), allowing the compiler to optimize safely.

* **[03_inet_pton](../../../01_socket_underlying_c/01_basic/03_inet_pton.c)**
    * *Presentation to Numeric*.
    * Converts dotted-decimal strings (e.g., "192.168.1.1") into 32-bit unsigned integers for network transmission.
      ```c
      int inet_pton (int __af, const char *__restrict __cp,
      void *__restrict __buf) __THROW;
      ```
    * **Why `void *`?** This is a clever design. IPv4 uses `struct in_addr` (4 bytes), while IPv6 uses `struct in6_addr` (16 bytes). Using `void*` acts as a universal adapter to accept binary data for either protocol.

* **[04_inet_ntop](../../../01_socket_underlying_c/01_basic/04_inet_ntop.c)**
    * *Numeric to Presentation*.
    * Restores 32-bit network integers back to human-readable IP strings.
    * `__len` is included to prevent buffer overflows—a classic memory safety issue in C.