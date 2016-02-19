#!/usr/bin/env python
# -*- coding: utf-8 -*-

# You must initialize the gobject/dbus support for threading
# before doing anything.
import gobject
import os
import time

gobject.threads_init()

from dbus import glib
glib.init_threads()

# Create a session bus.
import dbus
bus = dbus.SessionBus()

# Create an object that will proxy for a particular remote object.
remote_object = bus.get_object("org.kde.kstars", # Connection name
                               "/KStars/INDI" # Object's path
                              )

# Introspection returns an XML document containing information
# about the methods supported by an interface.
print ("Introspection data:\n")
print remote_object.Introspect()

# Get INDI interface
iface = dbus.Interface(remote_object, 'org.kde.kstars.INDI')

myDevices = [ "indi_nexstarevo_telescope", "indi_simulator_ccd" ]

# Start INDI devices
iface.start("7624", myDevices)

print "Waiting for INDI devices..."

# Create array for received devices
devices = []

while True:
    devices = iface.getDevices()
    if (len(devices) < len(myDevices)):
        time.sleep(1)
    else:
        break;

print "We received the following devices:"
for device in devices:
    print device

print "Establishing connection to Telescope and CCD..."

# Set connect switch to ON to connect the devices
iface.setSwitch("NexStar Evolution", "CONNECTION", "CONNECT", "On")
# Send the switch to INDI server so that it gets processed by the driver
iface.sendProperty("NexStar Evolution", "CONNECTION")
# Same thing for CCD Simulator
iface.setSwitch("CCD Simulator", "CONNECTION", "CONNECT", "On")
iface.sendProperty("CCD Simulator", "CONNECTION")

telescopeState = "Busy"
ccdState       = "Busy"

# Wait until devices are connected
while True:
    telescopeState = iface.getPropertyState("NexStar Evolution", "CONNECTION")
    ccdState       = iface.getPropertyState("CCD Simulator", "CONNECTION")
    if (telescopeState != "Ok" or ccdState != "Ok"):
        time.sleep(1)
    else:
        break

print "Connected to Telescope and CCD is established."
print "Commanding telescope to slew to coordinates of star Caph..."

# Set Telescope RA,DEC coords in JNOW
iface.setNumber("NexStar Evolution", "EQUATORIAL_EOD_COORD", "RA", 0.166)
iface.setNumber("NexStar Evolution", "EQUATORIAL_EOD_COORD", "DEC", 59.239)
iface.sendProperty("NexStar Evolution", "EQUATORIAL_EOD_COORD")

# Wait until slew is done
telescopeState = "Busy"
while True:
    telescopeState = iface.getPropertyState("NexStar Evolution", "EQUATORIAL_EOD_COORD")
    if (telescopeState != "Ok"):
        time.sleep(1)
    else:
        break

print "Telescope slew is complete, tracking..."
print "Taking a 5 second CCD exposure..."

# Take 5 second exposure
iface.setNumber("CCD Simulator", "CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 5.0)
iface.sendProperty("CCD Simulator", "CCD_EXPOSURE")

# Wait until exposure is done
ccdState       = "Busy"
while True:
    ccdState = iface.getPropertyState("CCD Simulator", "CCD_EXPOSURE")
    if (ccdState != "Ok"):
        time.sleep(1)
    else:
        break

print "Exposure complete"

# Get image file name and open it in external fv tool
fileinfo = iface.getBLOBFile("CCD Simulator", "CCD1", "CCD1")
print "We received file: ", fileinfo[0], " with format ", fileinfo[1], " and size ", fileinfo[2]

print "Invoking fv tool to view the received FITS file..."
# run external fits viewer
command = "fv " + fileinfo[0]
os.system(command)

print "Shutting down INDI server..."
# Stop INDI server
iface.stop("7624")
