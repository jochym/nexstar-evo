#!env python3
# -*- coding: utf-8 -*-
# NexStar Evolution communication library
# (L) by Paweł T. Jochym <jochym@gmail.com>
# This code is under GPL 3.0 license

from __future__ import division, print_function

import asyncio
import signal
import socket
import sys
from socket import SOL_SOCKET, SO_BROADCAST, SO_REUSEADDR
import struct
import time

import skyfield.api
from skyfield.api import Topos, EarthSatellite


# ID tables
targets={'ANY':0x00,
         'MB' :0x01,
         'HC' :0x04,
         'UKN1':0x05,
         'HC+':0x0d,
         'AZM':0x10,
         'ALT':0x11,
         'APP':0x20,
         'GPS':0xb0,
         'UKN2': 0xb4,
         'WiFi':0xb5,
         'BAT':0xb6,
         'CHG':0xb7,
         'LIGHT':0xbf
        }
trg_names={value:key for key, value in targets.items()}
        
control={
         'HC' :0x04,
         'HC+':0x0d,
         'APP':0x20,
        }

commands={
          'MC_GET_POSITION':0x01,
          'MC_GOTO_FAST':0x02,
          'MC_SET_POSITION':0x04,
          'MC_GET_???':0x05,
          'MC_SET_POS_GUIDERATE':0x06,
          'MC_SET_NEG_GUIDERATE':0x07,
          'MC_LEVEL_START':0x0b,
          'MC_SET_POS_BACKLASH':0x10,
          'MC_SET_NEG_BACKLASH':0x11,
          'MC_SLEW_DONE':0x13,
          'MC_GOTO_SLOW':0x17,
          'MC_AT_INDEX':0x18,
          'MC_SEEK_INDEX':0x19,
          'MC_SET_MAXRATE':0x20,
          'MC_GET_MAXRATE':0x21,
          'MC_ENABLE_MAXRATE':0x22,
          'MC_MAXRATE_ENABLED':0x23,
          'MC_MOVE_POS':0x24,
          'MC_MOVE_NEG':0x25,
          'MC_ENABLE_CORDWRAP':0x38,
          'MC_DISABLE_CORDWRAP':0x39,
          'MC_SET_CORDWRAP_POS':0x3a,
          'MC_POLL_CORDWRAP':0x3b,
          'MC_GET_CORDWRAP_POS':0x3c,
          'MC_GET_POS_BACKLASH':0x40,
          'MC_GET_NEG_BACKLASH':0x41,
          'MC_GET_AUTOGUIDE_RATE':0x47,
          'MC_GET_APPROACH':0xfc,
          'MC_SET_APPROACH':0xfd,
          'GET_VER':0xfe,
         }
cmd_names={value:key for key, value in commands.items()}

ACK_CMDS=[0x02,0x04,0x06,0x24,]

MC_ALT=0x11
MC_AZM=0x10

trg_cmds = {
    'BAT': {
        0x10:'GET_VOLTAGE',
        0x18:'GET_SET_CURRENT',
    },
    'CHG': {
        0x10: 'GET_SET_MODE',
    },
    'LIGHT': {
        0x10:'GET_SET_LEVEL',
    },
    'ALT': commands,
    'AZM': commands,
}

for trg in list(trg_cmds.keys()):
    for i in list(trg_cmds[trg].keys()):
        trg_cmds[trg][trg_cmds[trg][i]]=i

RATES = {
    0 : 0.0,
    1 : 1/(360*60),
    2 : 2/(360*60),
    3 : 5/(360*60),
    4 : 15/(360*60),
    5 : 30/(360*60),
    6 : 1/360,
    7 : 2/360,
    8 : 5/360,
    9 : 10/360
}


trgid={}
for k in targets.keys():
    trgid[targets[k]]=k

ctrlid={}
for k in control.keys():
    ctrlid[control[k]]=k

cmdid={}
for k in commands.keys():
    cmdid[commands[k]]=k


def print_command(cmd):
    if cmd[2] in (0x10, 0x20):
        try :
            return 'Command: %s->%s [%s] len:%d: data:%s' % (
                        trg_names[cmd[1]], 
                        trg_names[cmd[2]], 
                        cmd_names[cmd[3]], cmd[0], cmd[4:-1].hex())
        except KeyError :
            pass
    try :
        return 'Command: %s->%s [%02x] len:%d: data:%s' % (
                    trg_names[cmd[1]], 
                    trg_names[cmd[2]], 
                    cmd[3], cmd[0], cmd[4:-1].hex())
    except KeyError :
        pass
        
    return 'Command: %02x->%02x [%02x] len:%d: data:%s' % (
                cmd[1], cmd[2], cmd[3], cmd[0], cmd[4:-1].hex())


def decode_command(cmd):
    return (cmd[3], cmd[1], cmd[2], cmd[0], cmd[4:-1], cmd[-1])


def split_cmds(data):
    # split the data to commands
    # the initial byte b'\03b' is removed from commands
    if data.find(b';') == -1 :
        # No aux header (0x3b) in the stream. Just eat it.
        return []
    else :
        # There are some potential commands here
        # Skip the first commad - it is either noise or empty
        return [cmd for cmd in data.split(b';')][1:]


# Utility functions
def checksum(msg):
    return ((~sum([c for c in bytes(msg)]) + 1) ) & 0xFF


def f2dms(f):
    '''
    Convert fraction of the full rotation to DMS triple (degrees).
    '''
    s= 1 if f>0 else -1
    d=360*abs(f)
    dd=int(d)
    mm=int((d-dd)*60)
    ss=(d-dd-mm/60)*3600
    return s*dd,mm,ss

def parse_pos(d):
    '''
    Parse first three bytes into the DMS string
    '''
    if len(d)>=3 :
        pos=struct.unpack('!i',b'\x00'+d[:3])[0]/2**24
        return u'%03d°%02d\'%04.1f"' % f2dms(pos)
    else :
        return u''


def repr_pos(alt,azm):
    return u'(%03d°%02d\'%04.1f", %03d°%02d\'%04.1f")' % (f2dms(alt) + f2dms(azm))

def repr_angle(a):
    return u'%03d°%02d\'%04.1f"' % f2dms(a)

def unpack_int3(d):
    return struct.unpack('!i',b'\x00'+d[:3])[0]/2**24

def pack_int3(f):
    return struct.pack('!i',int(f*(2**24)))[1:]
    
def unpack_int2(d):
    return struct.unpack('!i',b'\x00\x00'+d[:2])[0]

def pack_int2(v):
    return struct.pack('!i',int(v))[-2:]



def dprint(m):
    m=bytes(m)
    for c in m:
        if c==0x3b :
            print()
        print("%02x" % c, end=':')
    print()


def parse_msg(m, debug=False):
    '''
    Parse bytes byond 0x3b. 
    Do not pass the message with preamble!
    '''
    m=bytes(m)
    if len(m)<4 :
        raise ValueError
    msg=m[:-1]
    if debug :
        print('Parse:',end='')
        dprint(msg)
    if  checksum(msg) != m[-1] :
        print('Checksum error: %x vs. %02x' % (checksum(msg) , m[-1]))
        dprint(m)
    l,src,dst,mid=struct.unpack('4B',msg[:4])
    dat=msg[4:]
    #print 'len:', l
    return src, dst, mid, dat



def detect_scope(verbose=False):
    '''
    NexStar Evolution detector. This function listens for the
    telescope (SkyFi dongle really) signature on the 55555 udp
    port send from the port 2000 and containing 110 bytes of payload.
    This could be easily adapted for different ports/signatures.
    
    Returns: scope_ip:string, scope_port:int
    '''
    class DetectScope:
        def __init__(self, loop, verbose=False):
            self.loop = loop
            self.transport = None
            self.scope_ip = None
            self.scope_port = None
            self.verbose = verbose

        def connection_made(self, transport):
            self.transport = transport
            if self.verbose: print('Looking for scope ...')

        def datagram_received(self, data, addr):
            if addr[1]==2000 and len(data)==110:
                if self.verbose: print('Got signature from the scope at: %s' % (addr[0],))
                self.scope_ip, self.scope_port = addr[0], addr[1]
                self.transport.close()

        def error_received(self, exc):
            print('Error received:', exc)

        def connection_lost(self, exc):
            if self.verbose: print("Socket closed")
            loop = asyncio.get_event_loop()
            loop.stop()
    
    loop = asyncio.get_event_loop()
    detector=DetectScope(loop,verbose)
    connect = loop.create_datagram_endpoint(lambda: detector, local_addr=('0.0.0.0', 55555))
    transport, protocol = loop.run_until_complete(connect)
    try :
        loop.run_forever()
    except KeyboardInterrupt :
        pass
    transport.close()
    
    return detector.scope_ip, detector.scope_port


class NexStarScope:
    '''
    The controller object. This class represents the scope to the client 
    software. It handles all communications and abstracts the scope to
    the simple API.
    '''
    #TODO: Describe the API
    
    def __init__(self, addr=None, port=2000, verbose=False):
        if addr is None:
            self.ip, self.port = detect_scope(verbose)
        else :
            self.ip, self.port = addr, port
        self.reading_headers = True
        self.handling = False
        #self.set_terminator('*HELLO*')
        self.startup=True
        self.me=targets['APP']
        self.verb=verbose
        self.connected=False
        self.slew_azm=False
        self.slew_alt=False
        self.guiding=False
        self.alt=0.0
        self.azm=0.0
        self.trg_alt = 0.0
        self.trg_azm = 0.0
        self.voltage=0.0
        self.ISS='INI'
        self.stations= skyfield.api.load.tle('http://celestrak.com/NORAD/elements/stations.txt', reload=True)
        self.oq = asyncio.Queue()
        self.iq = asyncio.Queue()
        self._mc_handlers = {
            0x01 : NexStarScope.get_position,
            0x13 : NexStarScope.slew_done,
        }
        self.handlers = {
            MC_ALT : self._mc_handlers,
            MC_AZM : self._mc_handlers,
            targets['BAT'] : {
                0x10 : NexStarScope.get_voltage,
            }
            
        }


#        self._other_handlers = {
#            0x10: NexStarScope.cmd_0x10,
#            0x18: NexStarScope.cmd_0x18,
#            0xfe: NexStarScope.send_ack,
#        }

#        self._mc_handlers = {
#          0x01 : NexStarScope.get_position,
#          0x02 : NexStarScope.goto_fast,
#          0x04 : NexStarScope.set_position,
#          0x05 : NexStarScope.cmd_0x05,
#          0x06 : NexStarScope.set_pos_guiderate,
#          0x07 : NexStarScope.set_neg_guiderate,
#          0x0b : NexStarScope.level_start,
#          0x10 : NexStarScope.set_pos_backlash,
#          0x11 : NexStarScope.set_neg_backlash,
#          0x13 : NexStarScope.slew_done,
#          0x17 : NexStarScope.goto_slow,
#          0x18 : NexStarScope.at_index,
#          0x19 : NexStarScope.seek_index,
#          0x20 : NexStarScope.set_maxrate,
#          0x21 : NexStarScope.get_maxrate,
#          0x22 : NexStarScope.enable_maxrate,
#          0x23 : NexStarScope.maxrate_enabled,
#          0x24 : NexStarScope.move_pos,
#          0x25 : NexStarScope.move_neg,
#          0x38 : NexStarScope.send_ack,
#          0x39 : NexStarScope.send_ack,
#          0x3a : NexStarScope.set_cordwrap_pos,
#          0x40 : NexStarScope.get_pos_backlash,
#          0x41 : NexStarScope.get_neg_backlash,
#          0x47 : NexStarScope.get_autoguide_rate,
#          0xfc : NexStarScope.get_approach,
#          0xfd : NexStarScope.set_approach,
#          0xfe : NexStarScope.fw_version,
#        }


    def get_position(self, data, src, dst):
        if len(data)<3 : return
        if src == MC_ALT :
            self.alt = unpack_int3(data)
        if src == MC_AZM :
            self.azm = unpack_int3(data)
        self.dbg(repr_pos(self.alt, self.azm))
        
        
    def slew_done(self, data, src, dst):
        if src == MC_ALT :
            self.slew_alt = data==b'\x00'
        if src == MC_AZM :
            self.slew_azm = data==b'\x00'


    def get_voltage(self, data, src, dst):
        if dst==self.me :
            self.voltage=(struct.unpack('!i',data[2:]))[0]/1e6
        self.dbg('BAT: {}V'.format(self.voltage))


    def dbg(self, *args, **kwargs):
        if self.verb:
            if 'file' in kwargs:
                print(*args, **kwargs)
            else :
                print(*args, file=sys.stderr, **kwargs)

    async def process_buffer(self, data, verb=False):
        for m in split_cmds(data):
            try :
                self.handle_msg(m)
            except ValueError:
                self.dbg('Parse Error:', m.hex())


    async def handle_read(self, rd):
        while self.connected :
            data = await rd.read(1024)
            if not data :
                break;
            #self.dbg(data.hex())
            await self.process_buffer(data, self.verb)
        self.connected = False
        self.dbg('>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>(read)')


    async def handle_write(self, wr):
        # Make sure the scope is in transparent mode
        # By forcing command mode and going out of it
        await asyncio.sleep(1)
        wr.write(b'$$$')
        await wr.drain()
        await asyncio.sleep(1)
        wr.write(b'exit\r\n')
        await wr.drain()
        await asyncio.sleep(1)
        
        # Send commands from the queue
        while self.connected :
            cmd = await self.oq.get()
            
            # Someone requested connection close
            if cmd is None : break
            #self.dbg('SND:',cmd)
            wr.write(cmd.encode())
            await wr.drain()
            await asyncio.sleep(0.05)
        self.connected = False
        # Signal to reader we are closing
        await self.iq.put(None)
        wr.close()
        self.dbg('>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>(write)')


    async def open_connection(self, loop):
        self.dbg('Connecting...',end='')
        rd, wr = await asyncio.open_connection(self.ip, self.port, loop=loop)
        self.dbg('OK')
        self.connected = True
        return rd, wr


    async def ctrl(self):
        self.dbg('Asking ... ')
        for trg in 'ALT', 'AZM' :
            await self.queue_cmd(dst=trg, cmd='GET_VER')
        await self.queue_cmd('AZM', 'MC_GET_???')
        for trg in 'ALT', 'AZM' :
            await self.queue_cmd(dst=trg, cmd='MC_MOVE_POS', data=b'\x00')
            await self.queue_cmd(dst=trg, cmd='MC_GET_APPROACH')
            await self.queue_cmd(dst=trg, cmd='MC_GET_POS_BACKLASH')
            await self.queue_cmd(dst=trg, cmd='MC_GET_MAXRATE')
            await self.queue_cmd(dst=trg, cmd='MC_MAXRATE_ENABLED')
            await self.queue_cmd(dst=trg, cmd='MC_GET_AUTOGUIDE_RATE')
            await self.queue_cmd(dst=trg, cmd='MC_SET_POS_GUIDERATE', data=b'\x00\x00\x00')

        await self.queue_cmd(dst='LIGHT', cmd='GET_SET_LEVEL', data=b'\x02')
        await self.queue_cmd(dst='LIGHT', cmd='GET_SET_LEVEL', data=b'\x00')
        await self.queue_cmd(dst='CHG', cmd='GET_SET_MODE')
        await self.queue_cmd(dst='BAT', cmd='GET_SET_CURRENT')
        await self.queue_cmd(dst='BAT', cmd='GET_VOLTAGE')
        await self.queue_cmd(dst='AZM', cmd='MC_ENABLE_CORDWRAP')
        await self.queue_cmd(dst='AZM', cmd='MC_SET_CORDWRAP_POS', data=b'\x7f\xff\xff')
        
        self.dbg('>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>(ctrl) Init finished')


    async def consume(self):
        self.dbg('Waiting for answers')
        while True:
            cmd = await self.iq.get()
            if cmd is None : 
                print('Closing consumer.')
                break
            print('RCV:', cmd)
            if not self.connected : break
        self.dbg('>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>(consume) Finished consumer')


    async def move(self):
        from math import sin, cos, pi
        await asyncio.sleep(15)
        self.dbg('Moving ... ')

        await self.goto(0.2,0.1)
        await self.goto(0.21,0.11,False)
        await asyncio.sleep(15)

        self.dbg('Moving back ... ')
        await self.goto(0.01,0.01)
        await self.goto(0,0,False)

        await asyncio.sleep(15)

        self.dbg('Guiding ... ')

        for i in range(0,360,5):
            await self.guide( sin(pi*i/180), cos(pi*i/180) )
            await asyncio.sleep(2)

        await self.goto(0.01,0.01,False)
        await self.goto(0.0,0.0,False)
        self.dbg('Finito')
        await self.oq.put(None)

        self.dbg('>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>(move)Finished move')


    async def trackISS(self, sleep=0.2):

        self.ISS='LD'

        ts = skyfield.api.load.timescale()
        place = Topos('50.0833 N', '20.0333 E')
        iss = self.stations['ISS (ZARYA)'] - place
        
        self.ISS='CAL'
        #t0=ts.utc(2017, 2, 14, 16, 27, 20)
        t0 = ts.now()
        p0 = iss.at(t0)
        alt, azm, dist = p0.altaz()
        self.ISS='GT1'
        await self.goto(alt.degrees/360, azm.degrees/360)
        t0 = ts.now()
        p0 = iss.at(t0)
        alt, azm, dist = p0.altaz()
        self.ISS='GT2'
        await self.goto(alt.degrees/360, azm.degrees/360)
        t0 = ts.now()
        p0 = iss.at(t0)
        alt, azm, dist = p0.altaz()
        self.ISS='GT3'
        await self.goto(alt.degrees/360, azm.degrees/360, False)
        self.ISS='SLP'
        await asyncio.sleep(sleep)

        #dt= ts.now().tt - t0.tt
        dt= 0/24/60/60
        scale = 3
        self.ISS='TRK'
        k=0
        while self.connected :
            t = ts.now()
            t.tt -= dt
            p_now = iss.at(t)
            t.tt += (sleep/24/60/60)
            p_next = iss.at(t)
            alt, azm, dist = p_next.altaz()
            s_alt = self.alt
            if s_alt > 0.5 :
                s_alt -= 1
            a = (alt.degrees/360 - s_alt)
            if abs(a) > 0.5 :
                if a>0 : a-=1 
                else : a+=1
            self.alt_gr = scale*a/sleep
            a = (azm.degrees/360 - self.azm)
            if abs(a) > 0.5 :
                if a>0 : a-=1 
                else : a+=1
            self.azm_gr = scale*a/sleep
            self.ISS='GD%s' % ('|/-\\'[k])
            await self.guide(self.alt_gr, self.azm_gr)
            k+=1 ; k%=4
            await asyncio.sleep(sleep)


    async def get_status(self, sleep=1):
        while not self.connected:
            await asyncio.sleep(1)
            print('.', end='')
        self.dbg('>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>(status)Finished pre-connect')
        k=0
        while self.connected :
            if k == 0 :
                await self.queue_cmd(dst='BAT', cmd='GET_VOLTAGE')
                k = 15
            else : k-=1
            for trg in 'ALT', 'AZM' :
                await self.queue_cmd(dst=trg, cmd='MC_GET_POSITION')
            if self.slew_alt:
                await self.queue_cmd(dst='ALT', cmd='MC_SLEW_DONE')
            if self.slew_azm:
                await self.queue_cmd(dst='AZM', cmd='MC_SLEW_DONE')
            await asyncio.sleep(sleep)
            
        self.dbg('>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>(get_status)Finished status')
                

    async def show_status(self, sleep=1):
        
        while self.connected :
            alt=self.alt
            if alt > 0.5 :
                alt-=1
            print('{:3s} Batt.: {:.2f}V  Az: {}({})  Alt: {} ({})   TRG: {} {}'.format(
                self.ISS,
                self.voltage,
                repr_angle(self.azm), 'S' if self.slew_azm else 'G' if self.guiding else 'I',
                repr_angle(alt), 'S' if self.slew_alt else 'G' if self.guiding else 'I',
                repr_angle(self.trg_azm), repr_angle(self.trg_alt),
            ), end='\r')
            sys.stdout.flush()
            await asyncio.sleep(sleep)
        print()
        self.dbg('>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>(show_status)Finished status')

    async def queue_cmd(self, dst, cmd, src='APP', data=b''):
        s=targets[src]
        d=targets[dst]
        try :
            i=commands[cmd]
        except KeyError :
            i=trg_cmds[dst][cmd]
        await self.oq.put(nse_msg(s=s, d=d, i=i, data=bytes(data)))

    def connect(self):
        '''
        Activate the connection to the scope.
        '''
        loop = asyncio.get_event_loop()

        rd, wr = loop.run_until_complete(self.open_connection(loop))

        loop.run_until_complete(asyncio.wait({
            self.handle_read(rd),
            self.handle_write(wr),
            self.get_status(),
            self.show_status(),
            self.trackISS(),
        }))
        loop.stop()

    def handle_msg(self,msg):
        s,d,mid,dat=parse_msg(msg)
        trg=s if d in ctrlid else d
        if  d!=self.me:
            # Ignore this is just an echo
            self.dbg('I',end='')
        else :
            try :
                return self.handlers[trg][mid](self,dat,s,d)
            except KeyError :
                self.dbg('No handler for:', print_command(msg))


    async def goto(self, alt, azm, fast=True, wait=True):
        cmd='MC_GOTO_FAST' if fast else 'MC_GOTO_SLOW'
        self.slew_alt=True
        self.slew_azm=True
        self.trg_alt, self.trg_azm = alt, azm
        await self.queue_cmd(dst='ALT', cmd=cmd, data=pack_int3(alt))
        await self.queue_cmd(dst='AZM', cmd=cmd, data=pack_int3(azm))
        if wait :
            while self.slew_alt or self.slew_azm:
                await asyncio.sleep(1)


    async def set_axis_guide(self, ax, gr):
        cmd='MC_SET_NEG_GUIDERATE' if gr<0 else 'MC_SET_POS_GUIDERATE'
        await self.queue_cmd(dst=ax, cmd=cmd, data=pack_int3(abs(gr)))


    async def guide(self, alt_gr, azm_gr, wait=True):
        self.guiding=True
        await self.set_axis_guide('ALT', alt_gr)
        await self.set_axis_guide('AZM', azm_gr)



# Message classes

class nse_msg:
    def __init__(self, s, d, i, data=b''):
        self.l=len(data)+3
        self.src=s
        self.dst=d
        self.mid=i 
        self.data=bytes(data)
    
    def _repr_head(self):
        try :
            other = self.dst if not self.dst == 0x20 else self.src
            return u'[%d] %4s => %4s (%12s): ' % (
                    self.l,
                    trgid[self.src],trgid[self.dst],
                    trg_cmds[trgid[other]][self.mid] )
        except KeyError :
            return u'[%d] %02x => %02x (%02x): ' % (
                    self.l,self.src,self.dst,self.mid)

    def __repr__(self):
        r=self._repr_head()
        return r+' '.join(['0x%02x' % c for c in self.data])
        
    def encode(self):
        s='3B{:02x}{:02x}{:02x}{:02x}'.format(len(self.data)+3,self.src,self.dst,self.mid)
        b = bytearray.fromhex(s)+self.data
        b += bytearray([checksum(b[1:])])
        #print('CMD:', b.hex(),file=sys.stderr)
        return b

class nse_mc_msg(nse_msg):

#          'MC_GET_POSITION':0x01,
#          'MC_GOTO_FAST':0x02,
#          'MC_SET_POSITION':0x04,
#          'MC_GET_???':0x05,
#          'MC_SET_POS_GUIDERATE':0x06,
#          'MC_SET_NEG_GUIDERATE':0x07,
#          'MC_LEVEL_START':0x0b,
#          'MC_GOTO_SLOW':0x17,
#          'MC_SEEK_INDEX':0x19,
#          'MC_MOVE_POS':0x24,
#          'MC_MOVE_NEG':0x25,
#          'MC_GET_APPROACH':0xfc,
#          'GET_VER':0xfe,
 
    PosCmd=[0x01,0x02,0x04,0x06,0x07]
    
    
    def __init__(self, l, s, d, i, data=None):
        nse_msg.__init__(self, l, s, d, i, data)
    
    def __repr__(self):
        if self.mid in nse_mc_msg.PosCmd :
            r=self._repr_head()
            return (r+parse_pos(self.data)).encode('utf-8')
        else :
            return nse_msg.__repr__(self)

class nse_pos_msg(nse_mc_msg):

    def __init__(self, l, s, d, i, data=None):
        nse_mc_msg.__init__(self, l, s, d, i, data)


MsgType={
        0x10: nse_mc_msg,
        0x11: nse_mc_msg,
        }




            
            
if __name__ == '__main__':
    ip, port = detect_scope(verbose=False)
    if ip is not None :
        print('Scope detected at {}:{}'.format(ip,port))
    else :
        print('Scope not detected')
    scope = NexStarScope(ip,port,False)
    scope.connect()
    asyncio.get_event_loop().close()

