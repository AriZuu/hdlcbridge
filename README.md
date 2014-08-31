hdlcbridge
=========

Utility for creating virtual ethernet interface by passing packets between
Unix TAP interface and embedded system with RFC 1662 HDLC-like framing.

It allows using low-cost OpenWrt Wifi modules like Hi-Link HLK-RM04 as
Wifi modules for embedded systems. System running uIP or lwIP can pass
encapsulated ethernet frames to hdlcbridge running on OpenWrt.

RFC 1662 framing on USART or SPI on embedded system is typically
very easy to implement, at least when comparing required efforts
to writing a full Wifi stack.
