### Reset everything
Reset TUXes
On TUX2 connect to Switch, Router
/system reset-configuration
login: root

### IPS
TUX2: ipconfig eth0 172.16.11.1/24
TUX3: ifconfig eth0 172.16.10.1/24
TUX4: ifconfig eth0 172.16.10.254/24
      ifconfig eth1 172.16.11.253/24

### Bridges on switch
/interface bridge add name=bridge10
/interface bridge add name=bridge11
Remove now ports that are connected
/interface bridge port remove [find interface =etherZZZ]
/interface bridge port remove [find interface =etherZZZ]
/interface bridge port remove [find interface =etherZZZ]
/interface bridge port remove [find interface =etherZZZ]
Add ports to bridges
/interface bridge port add bridge=bridge10 interface=etherZZZ #tux3
/interface bridge port add bridge=bridge10 interface=etherZZZ #tux4
/interface bridge port add bridge=bridge11 interface=etherZZZ #tux4
/interface bridge port add bridge=bridge11 interface=etherZZZ #tux2
/interface bridge port add bridge=bridge11 interface=etherZZZ #RC

### Configure tux4 as a router
TUX4: echo 1 > /proc/sys/net/ipv4/ip_forward
      echo 0 > /proc/sys/net/ipv4/icmp_echo_ignore_broadcasts

### Add routes
TUX3: route add -net 172.16.11.0/24 gw 172.16.10.254
TUX2: route add -net 172.16.10.0/24 gw 172.16.11.253

### Ping everything

### NAT
ether1 of router Connects to PY.1
ether 2 goes goes to bridge Y1
Configure ips of RC
RC:   /ip address add address=172.16.1.19/24 interface=ether1
      /ip address add address=172.16.11.254/24 interface=ether2
      /ip route add dst-address=172.16.10.0/24 gateway=172.16.11.253
      */ip route add dst-address=0.0.0.0/0 gateway=172.16.1.254
TUX3: route add default gw 172.16.10.254
TUX4: route add default gw 172.16.11.254
TUX2: route add default gw 172.16.11.254

### Ping everything

### DNS

TUX2/3/4:          nano /etc/resolv.conf 
Clean and add:     nameserver 172.16.1.1

### TCP 
Compile and test code in TUX3