# 04 Broadcast & Multicast

* **background** * Here we first need to introduce a function `setsockopt`.
```c
extern int setsockopt (int __fd, int   __level, int __optname,
const void *__optval, socklen_t __optlen) __THROW;

```


* The role of this function is to apply further restrictions or specifications to the file descriptor.
* **Parameter Explanation**:
* `__fd`: The file descriptor of the socket.
* `__level`: The layer where the option is defined. Usually set to `SOL_SOCKET` (generic socket options) or `IPPROTO_IP` (IP layer options).
* `__optname`: The specific option name to set. For example, `SO_BROADCAST` (allow broadcast), `SO_REUSEADDR` (port reuse).
* `__optval`: A pointer to the buffer containing the option value. Usually a pointer to an `int`, where `1` enables and `0` disables.
* `__optlen`: The length of the `optval` buffer.


* In the server, we can also configure port reuse modes to facilitate debugging (or restarting), but for the sake of code simplicity, I did not add this part in the code instance.
* **Port Reuse Code Example**:
```c
int opt = 1;
// Allow reuse of local address and port, solving "Address already in use" error
setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

```




* **[01_broadcast_send](../../../01_socket_underlying_c//04_broadcast&&groupcast/01_broadcast_send.c)** * This file demonstrates the broadcast sender. Unlike TCP/UDP programming, in broadcast and multicast, there is no traditional Client-Server framework, but rather a relative relationship between information sending and receiving.
* Here `sendto` is used. Aside from needing to add extra functionality to the socket, the logic is basically consistent with the UDP client.


* **[02_broadcast_recv](../../../01_socket_underlying_c//04_broadcast&&groupcast/02_broadcast_recv.c)**
* The structure of this file is even simpler than `udp_server`, because here the broadcast address is fixed, and we only need to listen for corresponding data packets.
* What's interesting here is that `recv` doesn't need specific permissions set. This aligns with the design philosophy of broadcast: the sender needs extra verification, while the receiver only needs to judge if the packet is meant for it.


* **summary**
* Broadcast implementation is based on UDP because broadcast itself is a one-to-many unidirectional process. In actual networking, it often involves repeated broadcasts, so the importance of rapid data transmission far outweighs stable transmission here.
* **Broadcast Design Philosophy**: Broadcast is like "shouting with a loudspeaker." Because this behavior consumes the bandwidth of the entire subnet and may cause disturbance (broadcast storms), the kernel design requires the **sender** to explicitly call `setsockopt(SO_BROADCAST)` to request permission (turn on the switch). The receiver is passive and can hear it without special permissions.


* **adding (IP Class Knowledge)**
* To understand multicast, one must first learn IP classification knowledge:
* **Class A/B/C**: Used for Unicast (one-to-one communication).
* **Class D (224.0.0.0 ~ 239.255.255.255)**: **Dedicated to Multicast**. These IPs do not belong to any specific host but represent a "group". Sending data to this IP means all hosts that have joined this group will receive it.
* **Class E**: Reserved for research.


* **[03_groupcast_send.c](../../../01_socket_underlying_c//04_broadcast&&groupcast/03_groupcast_send.c)**
* Here, the multicast sender doesn't even need to use `setsockopt`. This is because Class D IP segments are inherently dedicated to multicast. So, `send` only needs to transmit data to these IP segments; when it sends, it has effectively already set the corresponding multicast group on that IP.


* **adding**
* **What is INADDR_ANY**: We often see `server_addr.sin_addr.s_addr = htonl(INADDR_ANY);` in code. Its value is actually `0.0.0.0`. It means "bind to all available local network interfaces". If you have both Wi-Fi and an Ethernet cable, using `INADDR_ANY` allows you to receive data from both network cards, without binding the program to a specific IP.


* **[04_groupcast_recv.c](../../../01_socket_underlying_c//04_broadcast&&groupcast/04_groupcast_recv.c)**
* `recv` needs to use `setsockopt` for configuration. As mentioned before, `_optval` in `setsockopt` is a `void*` type, which means we can construct a structure to pass data parameters; this is a common method in C. Here we need to use the `ip_mreq` structure, specifically designed for multicast groups, to set parameters:
```c
struct ip_mreq
{
  /* IP multicast address of group.  */
  struct in_addr imr_multiaddr; // Multicast group IP (e.g. 224.0.0.88)

  /* Local IP address of interface.  */
  struct in_addr imr_interface; // Interface IP to join the group with (usually INADDR_ANY)
};


```


* Here `imr_interface` is the local interface, and `imr_multiaddr` is the multicast IP. Both have `s_addr` members; like the design of `sockaddr_in`, this is due to historical reasons.
* **summary (Broadcast vs Multicast Philosophy)**
* A clear distinction must be made here from broadcast, reflecting the diametrically opposite underlying logic of the two:
* **Broadcast**: The **sender** needs `setsockopt`. Because broadcast is forceful/violent and disabled by default, the sender must actively request permission to "shout".
* **Multicast**: The **receiver** needs `setsockopt` (join group `IP_ADD_MEMBERSHIP`). Because multicast is precise, the sender just sends data to a Class D IP (anyone can send). The key is that the receiver must explicitly declare "I subscribe to this channel" before the kernel will fish out the corresponding data packets for you.