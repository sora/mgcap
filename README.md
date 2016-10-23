# mgcap
A high-performance capturing device for traffic monitoring

## Features
* Support any Linux network device
* Qdisc bypass
* Multiple queue NIC
* PCAP-NG format
* 96 Byte snaplen
* High-resolution timestamp (e.g., Intel X550, etc)

## How to use

**Traffic capture**
```bash
$ cd kmod; make
$ sudo insmod kmod/mgcap.ko ifname="lo"
$ dmesg
$ ping 127.0.0.1 &
$ cd src/mgdump; make
$ sudo ./mgdump /dev/mgcap/lo 
$ tshark -r ./output.pcap
$ sudo rmmod mgcap
```
**Enable/Disable hardware timestamp**
```bash
$ cd src/tools; make
$ # enable HWTstamp
$ sudo ./mgcap_hwtstamp_config enp1s0f1 1
$ # disable HWTstamp
$ sudo ./mgcap_hwtstamp_config enp1s0f1 0
```

## Performance experience

### Intel X550
**Machines**
* Sender: FreeBSD 11 +x550 +netmap +pkt-gen
* Receiver: Linux 4.4 +x550 +mgcap +mgdump

**Setup**
```bash
$ cd kmod; make
$ sudo insmod kmod/mgcap.ko ifname="enp1s0f1"
$ cd src/mgdump; make
$ sudo ./mgdump /dev/mgcap/enp1s0f1 
$ # pkt-gen from the Sender
$ sudo rmmod mgcap
```

**Result (256B, MAX=4,528,985 pps)**
* Multiple-queue traffic with no-hwtstamp: 4,521,520 pps
* Single-queue traffic with no-hwtstamp: 4,383,530 pps
* Multiple-queue traffic with hwtstamp: 4,526,545 pps
* Single-queue traffic with hwtstamp: 2,920,175 pps

## Spec

**Packet format from mgcap kernel module**

```
  0                      15 (bit)
  +-----------------------+
  |    SKB_LEN(16 bit)    |
  +-----------------------+
  |                       |
  |      Hardware         |
  |      Timestamp        |
  |       (64 bit)        |
  +-----------------------+
  |                       |
  |    Ethernet frame     |
  |                       |
  |~~~~~~~~~~~~~~~~~~~~~~~|
  +-----------------------+
```
