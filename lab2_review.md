## Experiment 1

### What are the ARP packets and what are they used for?
arp packets are used to get the link layer address (MAC address) of the device with a specific ip address. 
When computer need to send a packet to specific ip address, it first checks the mac address of the destination with arp packet.

### What are the MAC and IP addresses of ARP packets and why?
Tux3 when pinging tux4, has to firstly ask to the network for the MAC address of the device, that has the IP 172.16.10.254. Than gets an answer from tux4 with the MAC address ending with 2f:24.
ARP packet has information of the IP of the computer that sends the message (172.16.10.1), with the sender MAC address ending with 2d:ef.
This process happens again with tux4 wnating to know whats the MAC address of tux3.

### What packets does the ping command generate?
ICMP packets, that are used for network layer inforamtion sharing. 

### What are the MAC and IP addresses of the ping packets?
Since we are pinging from tu3 to tux4, the ICMP packets that are sent have the IP of destination (172.16.10.254) and the MAC of destination ending with 2f:24, while the received ones have the destination IP 172.16.10.1 and MAC 2d:ef.

### How to determine if a receiving Ethernet frame is ARP, IP, ICMP?
Each protocol work on different network layer and have different header. ARP works on network layer so it does not have ip header. IP and ICMP work both on the network layer. To tell the differenc we need to analyze the IP header of the network layer.

### How to determine the length of a receiving frame?
The ethernet packets that were collected did not have any information on the lenght. However, the overhead of the ethernet header is known, so one case use the IP header to get the lenght of the payload carried by the frame, and then just add the known overhead of the ethernet header which is 14 bytes.

### What is the loopback interface and why is it important?
The loopback interface is a virtual network interface that allows the computed to communicate with itself (even if it is not connected to a network), like when doing a ping to itself, or hosting a server. The packets,this way, if are meant to the same computer don't need to travel to the network, and are sent directly to the corresponding socket.

## Experiment 2

### How to configure bridgeY0?
After connecting to the switch via serial port we firstly remove the ports which we wants to conncet our machines to from the default bridge that connects all the ports of the switch. Then we create a new bridge with command in the presentation of the experiment. After that we only have to add the ports to the bridge we created.

### How many broadcast domains are there? How can you conclude it from the logs?
There are 2 broadcast domains, We boroadcast this IP adresses 172.16.10.255 and 172.16.11.255 but only got response from the first one. This happens because there are 2 computers on the first network connected trough bridge10, so we get the answers from the other computer. In the second network we don't see replies because there is only one computer in network. Example in images. If we ping the broadcast from the other network, we don't get answers obviosly. In conclusion there are only 2 broadcast domains.

## Experiment 3

### What routes are there in the tuxes? What are their meaning?
In tux3 and tux2 there 2 routes in the routing table after the experiment 3. The first entry forward everything to gateway 0.0.0.0 (default), and the second one, added by us, forward everything that should go to 172.16.11.0 to the gateway 172.16.10.254, which is our router in tux3. Tux2 has the same configuration but pointing to 172.16.10.0 using the gateway 172.16.11.253.

### What information does an entry of the forwarding table contain?
It contains information of the destination; the gateway that computer should use to send packets to the destination; to the genmask, which is the netmask; some flags; metric wich is the number of hops to the destination; ref,  which is not used in linux kernel; use, which is number of lookups; and Iface which is iterface of network card.

### What ARP messages, and associated MAC addresses, are observed and why?
On a ping from TUX3 to TUX2, on the other network, we see the following packets.
On the interface eth0 from the TUX4, it is seen an ARP packet from 172.16.10.1 (TUX3, with the MAC ending with 2d:ef) requesting the MAC address from 172.16.10.254 (TUX4). The TUX4 then answerws this request stating that it´s MAC address is ....:2f:24. A few ms later, TUX4 does the same request, but to TUX3.
On the interface eth1, we see a similar behaviour, but 172.16.11.253 (MAC ...:20:99), which is TUX4, asks for the MAC related to IP 172.16.11.1, which is TUX2, and then TUX2 answers back it´s MAC (..2e:c3). We see again this dialog, but starting with TUX2, and TUX4 answers back, a few milliseconds later.

###What ICMP packets are observed and why?
In the ping from TUX 3 to TUX 2, we see he ICMP packets going from 172.16.10.1 to 172.16.11.1. The IP header provides no information about the path the packet has to followm since this information is attached to the Ethernet hearder. Firstly, in eth0 (from tux4), we see the packet arriving with the destination MAC being the TUX4, and then this device acting as router sends the packet again to the wire using eth1, but now with the MAC address changed. So, the ICMP packets and their IP addresses do not bring the information on the IP layer of the path, but rather in the Ethernet field. This happens because we have a router in the middle connecting both networks. If we did not, we would see the ICMP packets with the MAC destination being the TUX2 itself, instead of the middle machine.

###What are the IP and MAC addresses associated to ICMP packets and why?
Ping from TUX 3 to TUX 2. In eth0, we see:
ICMP packet from 176.16.10.1 to 176.16.11.1 as request.
The MAC from source is 2d:ef and it goes to 2f:24 (TUX 4).
Later, in eth1 (other network), we see the same packet coming out:
ICMP packet from 176.16.10.1 to 176.16.11.1 as request.
The MAC from source is 2e:c3 (TUX4) and it goes to 20:99 (TUX2).
