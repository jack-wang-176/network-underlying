# 03 TFTP Implementation (TFTP Protocol Implementation)

*Trivial File Transfer Protocol*.

* **begin**
* Before we begin, I want to briefly introduce the core of understanding TFTP. As a small file transfer protocol based on UDP, the most annoying yet critical part of implementing TFTP in C is **manually constructing and analyzing binary packets**. You need to piece together every single byte.


* **[01_tftp_client](../../../01_socket_underlying_c//03_tftp/01_tftp_client.c)**
* **Part 1: Packet Construction Area**
* Specify the download filename `scanf("%s",filename);`
* The first difficulty is constructing the data packet; this isn't a string, it's compact binary.
* **TFTP Binary Packet Structure Diagram**:
```text
2 bytes     string    1 byte     string   1 byte
------------------------------------------------
| Opcode |  Filename  |   0  |    Mode    |   0  |
------------------------------------------------

```


* The code uses a clever operation `sprintf` to splice:
```c
packet_buf_len = sprintf((char*)packet_buf,"%c%c%s%c%s%c",0,1,filename,0,"octet",0);

```


* **Explain Byte Order**: Why do we not need to worry about big-endian and little-endian conversion here? Because `sprintf` writes single bytes sequentially. Writing 0 and then 1 results in `00 01` in memory, which happens to match the big-endian requirement of network byte order.


* **Part 2: Receive and Parse Loop (State Machine)**
* `packet_buf` is used to receive data sent from the server. Here, all data is sent using the `unsigned char` type. Storing this data means we can simply and directly use this array to parse the packet header.
```c
 // This is the transport level of the data packet
 unsigned char packet_buf[1024]= "";

```


```c
// Error message
if(packet_buf[1]== 5){...exit(1)}
// Received correct feedback request from server
if(packet_buf[1]==3){// Proceed to next step}

```


* It is important to note that we must first determine if the corresponding file exists; a `bool` or `int` type can be used as a flag. If not, the corresponding file needs to be created first.


* **Part 3: Verification and ACK (Manual Reliability)**
* Another difficulty is receiving and checking the block number in the data packet. Since UDP is an unreliable connection, we need to manually verify if data loss has occurred. We need to read from the packet header and compare it with the local record.
* **Data Verification Process**:
```c
if((num +1) == ntohs(*(packet_buf+2)))
// success -> send ACK packet
// fail    -> data lost, exit

```


* If the current data packet is fine, we need to construct an ACK packet and send it to the server to trigger sending the next block of data. Here, `ntohs` must be used because the sequence number in the header is in network order and must be converted to host order for comparison.
```c
packet_buf[1]= 4;

```


* Although file block data is sent here, the server only needs to verify the packet header.
* If the data block is smaller than 516 (meaning file data is less than 512), it indicates the writing is finished. However, the case where the file size is exactly a multiple of 512 is not considered here.
* **Summary**: The main challenge for such a client is the processing of binary packets. Since UDP itself is unreliable, we need to manually verify packet headers. As we can see, this verification logic is actually very similar to TCP's three-way handshake, which is why TCP usually handles file transfer tasks.


* **[02_tftp_server](../../../01_socket_underlying_c//03_tftp/02_tftp_server.c)**
* The server-side logic is relatively passive, mainly focusing on parsing and feedback:
1. Verify the data packet and check if the relevant file exists.
2. Define a block number and write it into the header, using the buffer as a relay for file data.
3. Wait for and parse the ACK packet.


* **Construct Error Packet (Helper Function)**
Note here a function for constructing error packets, extracted for code cleanliness:
```c
void senderr(int sockfd,struct sockaddr* clientaddr,char* err,int errcode,socklen_t addrlen){
  unsigned char buf[516] = "";
  // Construct error packet: [00] [05] [00] [ErrCode] [ErrMsg] [00]
  int buf_len = sprintf((char*)buf, "%c%c%c%c%s%c", 0, 5, 0, errcode, err, 0);
  sendto(sockfd,buf,buf_len,0,clientaddr,addrlen);
}

```


* Such a function allows us to quickly send relevant information at logical terminal nodes (e.g., file cannot be opened).
* **Summary**: The design philosophy exhibited by the server here is consistent with the client. It should be noted that both sides store block numbers locally, and both sides parse the data packets.

