# mcast

```
bash$ ./mcast -h
Usage: ./mcast
    [-g multicast_group]
    [-p port]
    [-l|-c]      # mode: listen (default)|client
    [-m ip_mtu]  # including headers; client mode only
    [-t ttl]     # default: 1; client mode only

Examples:
    -g 224.0.0.251 -p 5353       # IPv4 mDNS
    -g ff02::fb -p 5353          # IPv6 mDNS
    -g 239.255.255.251 -p 10101  # google cast debug
```
