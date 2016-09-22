# mgcap
A pcap capturing device for Monitoring Metrics

```bash
$ cd kmod; make
$ sudo insmod kmod/mgcap.ko ifname="lo"
$ dmesg
$ ping 127.0.0.1 &
$ cd src/mgdump; make
$ chmod 777 /dev/mgcap/lo
$ ./mgdump lo 
$ sudo rmmod mgcap
```
