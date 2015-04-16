# NexStar-Evo

Routines for Celestron NexStar Evolution telescopes.

For now this is only testing code and docs. It may grow into full indi driver for wifi control of the scope in the future. Depending on the completness of the docs and required effort. 

The docs are in the docs dir (duh!).
The progress in decoding the communication and structure of the scope is described in the [notes.md](doc/notes.md) file.
The code will land in nsevo directory. The analysis contains some communication dumps and extracts.

## The INDIlib driver for Celestron NexStar Evolution

**This is alpha-quality software!**

*Use at your own risk. Never leave the scope unattended when under control of the driver!*
*I am not responsible for any damage to your equipment due to the use of this software.*

The indi-nexstarevo directory contains an [INDI](http://www.indilib.org/) driver for the telescope 
using AUX command set and wireless communication. 
It should work for NexStar Evolution and other telescopes using AUX command set
through PC/AUX ports or SkyQlink wifi adapters. This is **not** a driver for standard
connection through Celestron Hand Controller. 
For this you need to use a standard Celestron indi driver.

The driver *actually works* - while being **very** alpha-level. 
Even the alignment works to the point. There is a huge amount of work before it can 
reach anything like production use but the results are encouraging. 

At this moment the basic functionality works: 

* pointing - the slow goto approach needs to be implemented
* tracking - the interaction with goto is not perfect and the tracking rate 
  calculation needs to be refined. It seems to be good enough for visual observations.
* syncing/alignment - the basic functionality seems to be working. It needs to be tested
  under the sky (!). 
  
The most important positions on the TODO list:

* fixing the issues with goto-tracking
* better goto with fast-slow-approach two phase slewing.
* tracking optimization
* support for monitoring/control of telescope peripherals (battery/lights/etc)
* code clean-up and refactoring (two classes are probably an overkill)
* maybe switch to asynchronous communication with the scope

The code is developed against current trunk of libindi. 
If you do not know how to compile it - it is probably not for you yet.
If you want to be an alpha tester/developer for the code - I will gladly help 
you to set it up. But if you just want to use it - trust me - it is too early.

I will make packages for debian/ubuntu as soon as soft is in beta-shape.
And I hope for it to be included in the third-party drivers package for indi.

Contributions are welcome!
