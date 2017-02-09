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

MC_ALT=0x10
MC_AZM=0x11

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
            return 'Command: %s->%s [%s] len:%d: data:%r' % (
                        trg_names[cmd[1]], 
                        trg_names[cmd[2]], 
                        cmd_names[cmd[3]], cmd[0], cmd[4:-1])
        except KeyError :
            pass
    try :
        return 'Command: %s->%s [%02x] len:%d: data:%r' % (
                    trg_names[cmd[1]], 
                    trg_names[cmd[2]], 
                    cmd[3], cmd[0], cmd[4:-1])
    except KeyError :
        pass
        
    return 'Command: %02x->%02x [%02x] len:%d: data:%r' % (
                cmd[1], cmd[2], cmd[3], cmd[0], cmd[4:-1])
            

def decode_command(cmd):
    return (cmd[3], cmd[1], cmd[2], cmd[0], cmd[4:-1], cmd[-1])

def split_cmds(data):
    # split the data to commands
    # the initial byte b'\03b' is removed from commands
    if data.find(b';') == -1 :
        return []
    else :
        return [cmd for cmd in data.split(b';') if cmd]

# Utility functions
def checksum(msg):
    return ((~sum([c for c in bytes(msg)]) + 1) ) & 0xFF

def f2dms(f):
    '''
    Convert fraction of the full rotation to DMS triple (degrees).
    '''
    d=360*f
    dd=int(d)
    mm=int((d-dd)*60)
    ss=(d-dd-mm/60)*3600
    return dd,mm,ss

def dprint(m):
    m=bytes(m)
    for c in m:
        if c==0x3b :
            print()
        print("%02x" % c, end=':')
    print()

def split_msgs(r,debug=False):
    '''
    Split the buffer into separate messages of the AUX protocol.
    '''
    l=r.find(b'\x3b')+1
    p=l
    ml=[]
    while p>-1:
        p=r.find(b'\x3b',l)
        #if debug : print l, p
        ml.append(r[l:p]+bytes(r[p]))
        l=p+1
    if debug : print(ml)
    return ml, p

def parse_msg(m, debug=False):
    '''
    Parse bytes byond 0x3b. 
    Do not pass the message with preamble!
    '''
    m=bytes(m)
    msg=m[:-1]
    if debug :
        print('Parse:',end='')
        dprint(msg)
    if  checksum(msg) != m[-1] :
        print('Checksum error: %x vs. %02x' % (checksum(msg) , m[-1]))
        dprint(m)
    l,src,dst,mid=struct.unpack('4B',msg[:4])
    dat=msg[4:-1]
    #print 'len:', l
    return src, dst, mid, dat



def parse_pos(d):
    '''
    Parse first three bytes into the DMS string
    '''
    if len(d)>=3 :
        pos=struct.unpack('!i','\x00'+d[:3])[0]/2**24
        return u'%03d°%02d\'%04.1f"' % f2dms(pos)
    else :
        return u''

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

# Target classes

class NSEScope:
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
        self.ibuffer = b''
        self.obuffer = b''
        self.reading_headers = True
        self.handling = False
        #self.set_terminator('*HELLO*')
        self.startup=True
        self.me=targets['APP']
        self.verb=verbose
        self.connected=False
        self.oq = asyncio.Queue()
        self.iq = asyncio.Queue()
        
    def dbg(self, *args, **kwargs):
        if self.verb:
            if 'file' in kwargs:
                print(*args, **kwargs)
            else :
                print(*args, file=sys.stderr, **kwargs)

    async def process_buffer(self, verb):
        for m in split_cmds(self.ibuffer):
            await self.iq.put(nse_msg(*parse_msg(m,verb)))
        self.ibuffer = b''

    async def handle_comm(self, loop):
        self.dbg('Connecting...',end='')
        rd, wr = await asyncio.open_connection(self.ip, self.port, loop=loop)
        self.dbg('OK')
        self.connected = True
        wr.write(b'exit\r\n')
        while self.connected :
            cmd = await self.oq.get()
            self.dbg('SND:',cmd)
            wr.write(cmd.encode())
            self.ibuffer += await rd.read(128)
            self.dbg(self.ibuffer.hex(), ' => ', end='')
            await self.process_buffer(self.verb)
            self.dbg(self.ibuffer.hex())
            self.dbg('MQ:',self.iq.qsize())
        self.dbg('>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>Finished connection')

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
        
        self.dbg('>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>Init finished')

    async def consume(self):
        self.dbg('Waiting for answers')
        while True:
            cmd = await self.iq.get()
            print('RCV:', cmd)
            if not self.connected : break
        self.dbg('>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>Finished consumer')

    async def move(self):
        await asyncio.sleep(15)
        self.dbg('Moving ... ')
        for trg in 'ALT', 'AZM' :
            await self.queue_cmd(dst=trg, cmd='MC_GOTO_FAST', data=b'\x0F\x00')
        await asyncio.sleep(15)
        self.dbg('Moving back ... ')
        for trg in 'ALT', 'AZM' :
            await self.queue_cmd(dst=trg, cmd='MC_GOTO_FAST', data=b'\x00\x00')
        await asyncio.sleep(15)
        self.dbg('Finito')
        self.connected = False
        await self.queue_cmd(dst='ALT', cmd='GET_VER')
        self.dbg('>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>Finished move')
    
    async def get_status(self):
        while not self.connected:
            await asyncio.sleep(10)
            print('.', end='')
        self.dbg('>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>Finished pre-connect')
        while self.connected :
            await asyncio.sleep(10)
            await self.queue_cmd(dst='BAT', cmd='GET_VOLTAGE')
            for trg in 'ALT', 'AZM' :
                await self.queue_cmd(dst=trg, cmd='MC_GET_POSITION')
        self.dbg('>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>Finished status')
                

    async def queue_cmd(self, dst, cmd, src='APP', data=b''):
        s=targets[src]
        d=targets[dst]
        try :
            i=commands[cmd]
        except KeyError :
            i=trg_cmds[dst][cmd]
        await self.oq.put(nse_msg( s=s, d=d, i=i, data=bytes(data)))

    def connect(self):
        '''
        Activate the connection to the scope.
        '''
        loop = asyncio.get_event_loop()

        loop.run_until_complete(asyncio.wait({
            self.handle_comm(loop),
            self.ctrl(),
            self.move(),
            self.consume(),
            self.get_status()
        }))
        loop.stop()
        loop.close()

    def connection_made(self, transport):
        self.dbg('Connected.')
        for src, ver in get_fw_version('ANY'):
            self.dbg('TRG: {}  FW_VER: {}'.format(trg_names[src], ver))
#        azm=self.get_fw_version('AZM')
#        alt=self.get_fw_version('ALT')
#        hc=self.get_fw_version('HC')
#        mb=self.get_fw_version('MB')

    def data_received(self, data):
        '''Buffer the data'''
        self.ibuffer.append(data)
        
        
    def connection_lost(self, exc):
        self.dbg('The server closed the connection')
        self.print('Stop the event loop')
        self.loop.stop()

        
    # grab some more data from the socket,
    # throw it to the collector method,
    # check for the terminator,
    # if found, transition to the next state.
    # Modified from stdlib - we need empty 
    # strings passed to collect data.
    def datagram_received(self, data):

        try:
            data = self.recv (self.ac_in_buffer_size)
        except socket.error() as why:
            if why.args[0] in _BLOCKING_IO_ERRORS:
                return
            self.handle_error()
            return

        self.ac_in_buffer = self.ac_in_buffer + data

        # Continue to search for self.terminator in self.ac_in_buffer,
        # while calling self.collect_incoming_data.  The while loop
        # is necessary because we might read several data+terminator
        # combos with a single recv(4096).

        while self.ac_in_buffer:
            lb = len(self.ac_in_buffer)
            terminator = self.get_terminator()
            if not terminator:
                # no terminator, collect it all
                self.collect_incoming_data (self.ac_in_buffer)
                self.ac_in_buffer = ''
            elif isinstance(terminator, int) or isinstance(terminator, long):
                # numeric terminator
                n = terminator
                if lb < n:
                    self.collect_incoming_data (self.ac_in_buffer)
                    self.ac_in_buffer = ''
                    self.terminator = self.terminator - lb
                else:
                    self.collect_incoming_data (self.ac_in_buffer[:n])
                    self.ac_in_buffer = self.ac_in_buffer[n:]
                    self.terminator = 0
                    self.found_terminator()
            else:
                # 3 cases:
                # 1) end of buffer matches terminator exactly:
                #    collect data, transition
                # 2) end of buffer matches some prefix:
                #    collect data to the prefix
                # 3) end of buffer does not match any prefix:
                #    collect data
                terminator_len = len(terminator)
                index = self.ac_in_buffer.find(terminator)
                if index != -1:
                    # we found the terminator
                    if index > 0:
                        # don't bother reporting the empty string (source of subtle bugs)
                        # Sometimes one needs empty strings!
                        pass
                    sys.stdout.flush()
                    self.collect_incoming_data (self.ac_in_buffer[:index])
                    self.ac_in_buffer = self.ac_in_buffer[index+terminator_len:]
                    # This does the Right Thing if the terminator is changed here.
                    self.found_terminator()
                else:
                    # check for a prefix of the terminator
                    index = asynchat.find_prefix_at_end (self.ac_in_buffer, terminator)
                    if index:
                        if index != lb:
                            # we found a prefix, collect up to the prefix
                            self.collect_incoming_data (self.ac_in_buffer[:-index])
                            self.ac_in_buffer = self.ac_in_buffer[-index:]
                        break
                    else:
                        # no prefix, collect it all
                        self.collect_incoming_data (self.ac_in_buffer)
                        self.ac_in_buffer = ''

    def handle_msg(self,msg):
        l,s,d,mid,dat=parse_msg(msg)
        trg=s if d in ctrlid else d
        if  d!=self.me:
            print('A',end='')
        try :
            return MsgType[trg](l,s,d,mid,dat)
        except KeyError :
            return nse_msg(l,s,d,mid,dat)
    
    def found_terminator(self):
        if self.startup :
            self.set_terminator('\x3b')
            self.startup=False
            self.ibuffer=[]
            print('Hello!')
            sys.stdout.flush()
            return
        if self.reading_headers :
            print('msg:',end='')
            self.reading_headers=False
            self.ibuffer=[]
            self.set_terminator(1)
        elif self.handling :
            self.handling=False
            self.reading_headers=True
            self.set_terminator('\x3b')
            self.msg=''.join(self.ibuffer)
            #dprint(self.msg)
            print(self.handle_msg(self.msg))
        else :
            self.handling=True
            self.msglen=ord(self.ibuffer[0][0])
            #print '(%2d)' % self.msglen,
            self.set_terminator(self.msglen+1)
        sys.stdout.flush()


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
            return u'[%d] %4s => %4s (%12s): ' % (
                    self.l,
                    trgid[self.src],trgid[self.dst],
                    cmdid[self.mid] )
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
        print('CMD:', b.hex(),file=sys.stderr)
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
    ip, port = detect_scope(verbose=True)
    if ip is not None :
        print('Scope detected at {}:{}'.format(ip,port))
    else :
        print('Scope not detected')
    scope = NSEScope(ip,port,True)
    scope.connect()
    asyncio.get_event_loop().close()

