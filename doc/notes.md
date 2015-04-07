# Notes about comms with NexStar Evo telescopes

This document describes additional info I have collected about the communication 
with the Evolution telescope.

## WiFi operation

In standard mode the WiFi module in the scope creates its own network unprotected (!) 
with very stupid selection of IP range. The scope IP is 1.2.3.4 and the dhcp server 
in the scope handles out addresses in the /24 network starting from 1.2.3.10, with 
default gateway and dns set to scope (!). This is really *bad design*. The address
range is a regular *internet* address (belongs to google AFAIK). They should use 
one of the private unroutable networks (10.x.x.x or others - RTFM engeneers at Celestron).
This makes it impossible to have a "dual home" machine talking to the scope without 
some manual configuration of the routing table - easy but would be unnecessary if 
the default config would be sensible and editable.

The communication goes over TCP port 2000 and seems to be using AUX protocol partially
documented in the excelent document by Andre Paquette (andre@paquettefamily.ca) located at
http://www.paquettefamily.ca/nexstar/ . A copy of this document is included in the doc directory.

It is very fortunate that the main communication board broadcasts all communications over 
all connected channels - this way it is easy to listen to the commands used by other devices 
and learn from their interaction.


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

None yet