# Notes about comms with NexStar Evo telescopes

This document describes additional info I have collected about the communication 
with the Evolution telescope.

## WiFi operation

In standard mode the WiFi module in the scope creates its own network:
unprotected, with very wired selection of IP range. The scope IP is 1.2.3.4
and the dhcp server in the scope handles out addresses in the /24 network
starting from 1.2.3.10, with default gateway and dns set to scope (!). This is
really *bad design*. The address range is a regular *internet* address (belongs
to google AFAIK). They should use one of the private unroutable networks
(10.x.x.x or others - RTFM engineers at Celestron). This makes it impossible to
have a "dual home" machine talking to the scope without some manual
configuration of the routing table - easy but would be unnecessary if the
default config would be sensible and easily editable.

The communication goes over TCP port 2000.  

The SkyQ module in the scope seems to be a Roving Networks WiFly wireless 
serial module. Which means we can get some info/config out of it by 
switching to command mode:
    250ms silence 
    send '$$$'
    250ms silence 

The docs for the command mode and the serial-to-wifi module are easy to find on
the web:

    http://www.tinkerfailure.com/2012/02/setting-up-the-wifly-rn-xv/
    http://ww1.microchip.com/downloads/en/DeviceDoc/50002230A.pdf
    
I have included two versions of the manual in the docs. 
There are numerous guides on setting up the module on the web. 
There is no point in repeating this here. I will try to identify the module
version when I get back to my scope.

The fact that this is just a wifi-to-serial module makes a whole thing easier 
to figure out. The whole thing probably just translates the data send from the 
network to the serial stream of the AUX connector. Thus we need to figure out 
the AUX command set and use it.

The AUX protocol has been partially documented in the excellent document by Andre Paquette (andre@paquettefamily.ca) located at 
http://www.paquettefamily.ca/nexstar/ . 
A copy of this document is included in the doc directory. It is very fortunate
that the main communication board broadcasts all communications over all
connected channels - this way it is easy to listen to the commands used by other
devices and learn from their interaction.


## AUX Protocol

The AUX protocol uses simple messages with the following structure:

    <preamble=0x3b>
    <packet len>
    <source device #>
    <destination device #>
    <message ID>
    <message data bytes>
    <checksum byte>
    
The checksum is calculated using following formula (in python):

    ((~sum([ord(c) for c in msg]) + 1) ) & 0xFF
    
where `msg` contains all bytes from `len` to the last data byte. This is just last byte of the two-complemented sum of all bytes.

## New targets

The targets list is extended by at least two new IDs:

    0x0d   The NexStar+ controller
    0x20   Network device
    
## Additional commands

    0x05    ??? unknown yet

