Protocol Notes
==============

This file contains various notes about the ASHv3 protocol. It is not
intended to be a full documentation for the protocol, but if we end
up writing up such an explanation, we will put it in this file.

In addition to the example transactions presented below, there are
many more example transactions which are included in the unit tests.

Definitions
-----------

* "OFC" - Outgoing Frame Counter
* "AFC" - Acknowledged Frame Counter
* "RSTACK" - Reset Acknowledgement

Packet Truth Tables
-------------------

### Initialization from when both devices are reset ###

	TX info:   ASH, type: ASH_RESET,  header 0: , OFC: 1, AFC: 0, length: 0
	RX Packet: ASH, type: ASH_RSTACK, header 0: , OFC: 1, AFC: 1, length: 0
	TX info:   ASH, type: ASH_ACK,    header 0: , OFC: 2, AFC: 1, length: 7
	RX Packet: ASH, type: ASH_RESET,  header 0: , OFC: 1, AFC: 1, length: 0
	TX info:   ASH, type: ASH_RSTACK, header 0: , OFC: 2, AFC: 1, length: 0
	RX Packet: ASH, type: ASH_ACK,    header 0: , OFC: 2, AFC: 2, length: 57
	TX info:   ASH, type: ASH_ACK,    header 0: , OFC: 2, AFC: 2, length: 0
	RX Packet: ASH, type: ASH_ACK,    header 0: , OFC: 3, AFC: 2, length: 9
	TX info:   ASH, type: ASH_ACK,    header 0: , OFC: 2, AFC: 3, length: 0
	RX Packet: ASH, type: ASH_ACK,    header 0: , OFC: 4, AFC: 2, length: 8
	TX info:   ASH, type: ASH_ACK,    header 0: , OFC: 2, AFC: 4, length: 0
	RX Packet: ASH, type: ASH_ACK,    header 0: , OFC: 5, AFC: 2, length: 44
	TX info:   ASH, type: ASH_ACK,    header 0: , OFC: 2, AFC: 5, length: 0
	TX info:   ASH, type: ASH_ACK,    header 0: , OFC: 3, AFC: 5, length: 7
	RX Packet: ASH, type: ASH_ACK,    header 0: , OFC: 6, AFC: 3, length: 57
	TX info:   ASH, type: ASH_ACK,    header 0: , OFC: 3, AFC: 6, length: 0
	RX Packet: ASH, type: ASH_ACK,    header 0: , OFC: 7, AFC: 3, length: 9
	TX info:   ASH, type: ASH_ACK,    header 0: , OFC: 3, AFC: 7, length: 0

### Initialization from when both devices are reset (w/ missing RSTACK workaround) ###

DEVICE A                      |      | DEVICE B
:----------------------------:|:----:|:-----------------------------------:
RESET,OFC:1,AFC:0,L:0         |  ->  | `7E 00 08 00 69 86 00`
`7E 00 49 00 47 6B C0`        |  <-  | RSTACK,OFC:1,AFC:1,L:0
	  ***RUNNING***           |      |
ACK,OFC:2,AFC:1,L:7           |  ->  | `7E 00 91 07 5B 01 00 03 01 69 04 81 4D C0`
`7E 00 09 00 4A A7 C0`        |  <-  | RESET,OFC:1,AFC:1,L:0
RSTACK,OFC:2,AFC:1,L:0        |  ->  | `7E 00 51 00 CD A1 80`
							  |      | ***RUNNING***
`7E 00 92 08 5B 01 00 04 ...` |  <-  | ACK,OFC:2,AFC:2,L:8
ACK,OFC:2,AFC:2,L:0           |  ->  | `7E 00 92 00 8E A6 80`

Protocol Observations
---------------------

 *  `ACK`/`NACK` packets are ignored unless we are in the `RUNNING` state.
 *  `RESET` affects the OFC of the sender (always 1) and the AFC of the
	receiver (which is then set to 1)
 *  Sending an `RSTACK` reuses the current outbound tx buffer if it has
	not yet been ACK'd.
 *  `RSTACK` packets do not appear to actually carry data, contrary to
	the official documentation stating otherwise.
 *  Sending a packet without a payload does not cause the outbound
	frame coutner to increment, it will re-use the index of the
	previously sent packet
 *  Frame counters roll over to 1 instead of 0. In other words,
	the value of the next frame counter is defined as `(x<8)?x+1:1`,
	not `(x+1)%8`. This invalidates some clever optimization
	possibilities.
