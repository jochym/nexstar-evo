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
(10.x.x.x or others - RTFM engineers at Celestron!). This makes it impossible to
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

The `get everything` command shows all config strings from the module. I got 
[RN-171](http://www.microchip.com/wwwproducts/Devices.aspx?product=RN171) 
as a device type and [WiFly-EZX](http://www.arexx.com/rp6/downloads/M256_WIFI_Datasheets/WiFly-RN-UM-2.31-v-0.1r.pdf)
as a device ID. The current command reference (50002230A.pdf) and 
older manual (WiFly-RN-UM-2.31-v-0.1r.pdf) is in the doc directory.
The simple setup guide are easy to find on the web for example at 
[tinkerfailure](http://www.tinkerfailure.com/2012/02/setting-up-the-wifly-rn-xv/).

I think we can conclude that this is simply RN-171, perheps with a slightly
modified firmware - it shows `<2.40-CEL>` as a version number instead of standard `<X.XX>` described in the manual. Thus I would **not** upgrade the firmware in the module (current firmware level is 4.41 on the microchip site).

Unfortunately I was unable to make it work in access point mode. No tweaking of config 
could make it work. Even celestron tools for the SkyQLink did not make it connect.
I would like to at least make the scope network protected instead of open. 
Any ideas?

**Update:** Progress on the AP mode front! You can program the params of the FiFly module from the tablet app when the scope is **disconnected** from the app but the tablet is, obviously, connected to the SkyQlink wifi network. The procedure is as follows - it is non-intuitive but actually easier/simpler then it seems:

1. Switch the stand-alone (standard) mode on the scope
2. Switch on the scope
3. Connect to the SkyQLink network
4. Lounch the app *without* connecting to the scope
5. Go to settings and fill in the details of your AP
6. Change mode in the App
7. Switch the scope off
8. Switch the scope to AP mode
9. Switch the scope on and wait for it to connect (the falshing changes)
10. Reconnect the tablet to your network and reconnect the App to the scope.

The fact that this is just a wifi-to-serial module makes a whole thing easier 
to figure out. The whole thing probably just translates the data send from the 
network to the serial stream of the AUX connector. Thus we need to figure out 
the AUX command set and use it.



## AUX Protocol

The AUX protocol has been partially documented in the excellent document by Andre Paquette (andre@paquettefamily.ca) located at 
http://www.paquettefamily.ca/nexstar/ . 
A copy of this document is included in the doc directory. It is very fortunate
that the main communication board broadcasts all communications over all
connected channels - this way it is easy to listen to the commands used by other
devices and learn from their interaction.


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

The targets list is extended by some new IDs:

* 0x0d - The NexStar+ controller
* 0xb4 - ???
* 0xb5 - WiFly WiFi controller. 
  - Responds to  0x10 - get status. Probably to enable/disable commands as well (cannot test these!).
* 0xb6 - Battery/Power controller
  - Responds to 0x10 with battery status bytes:
    * charging (0x00/0x01)
    * status (0x00/0x01/0x02 LOW/MEDIUM/HIGH)
    * voltage (32bit integer in network order representing voltage in microvolts)
  - Responds to 0x18 command (set/get external power limit) wich is a 16bit int representin current limit in mA.
* 0xb7 - Charge port. 
  - Responds to 0x10 - Get status/Set status
    * 0x00 automatic 
    * 0x01 allways on
* 0xbf - Mount Lights controller
  - Responds to 0x10 : Get/Set status  + selector byte with two bytes:
    * Selector:
      - 0x00 Tray
      - 0x01 WiFi
      - 0x02 Logo
    * Value: 0x00-0xff = 0-100%
* 0x20 - Network device (skyPortal app)

## Additional commands

|   ID      |   Label   |   Tx data     |   Rx data     |   Notes   |
|:---------:|:----------|:-------------:|:-------------:|-----------|
|   0x05    |           |     N/A       |   24bit       |           |
|   0x20    |           |     16bit     |    Ack        |           |
|   0x21    |           |     N/A       |   32bit       |           |
|   0x22    |           |     8bit      |    Ack        |           |
|   0x23    |           |     N/A       |    8bit       |           |
|   0xf0    |           |     N/A       |    8bit       |           |

* 0x05 - Gets some angle data from the ALT/AZM MC. It is executed by the HC at the start.
  
* The 0x20 - 0x23 commands activate speed limit for MC in the conversation with App.
* 0x20, VALUE - set max slew rate. 16bit int 1e-3 deg/s (?)
* 0x21, Get max slew rates as two 16bit ints. Corresponds to ALT, AZM max rates in 1e-3 deg/s (?) 
* 0x22, VALUE - enable max slew rate, one byte, 0x01 = Active, 0x00 = Inactive
* 0x23, returns status of the max rate setting (enabled=0x01, disabled=0x00)
  
* 0xf0 - Sends one byte (0x47) from ALT and AZM in reply to cmd 0x47.
  Seems to be problematic - the messages are:

        3b 03 20 10 47 86
        3b 04 10 20 f0 47 95
        3b 03 20 11 47 85
        3b 04 11 20 f0 47 94       

  One would think this should be 0x47f0 not 0xf047 ?
  Maybe it is just a bug in the firmware and it is a autoguide rate 
  of 93.75% of sideral rate (someone flipped the bytes)?
  From further experiments it looks like a bug in the firmware.
  At least in the AltAz mode. It is also possible that due to the fact
  that this mount does not have autoguide port the firmware breaks
  if you try to program it.

* 0x26 = MTR_AUX_GUIDE (Pulse guiding using AUX communication)
        Command is followed by a signed char: velocity [% sidereal]; and
        unsigned char: duration [10 ms units].

* 0x27 = MTR_IS_AUX_GUIDE_ACTIVE 
        MC returns TRUE(1) if last AUX_GUIDE command/s is still in effect.
        MC returns FALSE(0) if last AUX_GUIDE command has expired.

* 0x46, 0x47 = set/get the autoguide rates as a percentage of sidereal
        (Described in the main AUX commands document)
        MC_SET_AUTOGUIDE_RATE = 0x46, // 8    n/a     Set percentage of sidereal
        MC_GET_AUTOGUIDE_RATE = 0x47, // 8    n/a     % = 100, * val/256


## Lights commands

* 0x10 = GET/SET_LEVEL, NUM, VAL 
    Tx: Number, Value - Two bytes, 0 - Tray, 1 - Logo, 2 - WiFi ; Val 0-255
    Rx: ACK or one byte of level if the command was send with just NUM

## Charging Port (BAT target)


* 0x10 = GET_VOLTAGE
    Tx: None
    Rx: two 24bit ints(?). Second is (?) voltage in uV (microvolts) the first 
       may be the general level of the bat (low/medium/high) (?)

* 0x18 = SET_CURRENT, VAL (to BAT target)
    Tx: Value - 16bit int - current in mA
    Rx: Repeat actual value in the same format

## Charging controller

* 0x10 = SET_MODE
    Tx: 1 byte - 0x00 = Auto, 0x01 = On

## Possible errors in the Andre Paquette document

It seems that the angles (ALT/AZM) communicated by the MCs are *unsigned* 24bit
integers. It is consistent with the way the Celestron Z/z commands work 
(described in the 'NexStar Communication Protocol' document by Celestron).
This is different from the info in the Andre's document.

## Firmware update under Linux

**Never connect anything to your scope when it is charging! The charger is not properly grounded and the scope has 80-100V voltage on the chassis. It is enough to fry your ports and the electronics in the scope! Disconnect the charger before connecting anything.**

You can use the CFM Celestron Firmware Manager in linux with openjdk if you change the settings for your serial port.
Execute following commands for your serial port device (`/dev/ttyUSB0` in this case) before running CFM:

        stty -F /dev/ttyUSB0 -icrnl
        stty -F /dev/ttyUSB0 -ocrnl

I have checked the procedure myself. No problems with update.
