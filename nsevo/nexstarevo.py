#!env python
# -*- coding: utf-8 -*-
# NexStar Evolution communication library
# (L) by Paweł T. Jochym <jochym@gmail.com>
# This code is under GPL 3.0 license

from __future__ import division

import socket
import sys
import struct
import time
import asynchat


# ID tables
targets={'ANY':0x00,
         'MB' :0x01,
         'HC' :0x04,
         'HC+':0x0d,
         'AZM':0x10,
         'ALT':0x11,
         'APP':0x20,
         'GPS':0xb0,
         'WiFi':0xb5,
         'BAT':0xb6,
         'CHG':0xb7,
         'LIGHT':0xbf
        }
        
control={
         'HC' :0x04,
         'HC+':0x0d,
         'APP':0x20,
        }

commands={
          'MC_GET_POSITION':0x01,
          'MC_GOTO_FAST':0x02,
          'MC_SET_POSITION':0x04,
          'MC_SET_POS_GUIDERATE':0x06,
          'MC_SET_NEG_GUIDERATE':0x07,
          'MC_LEVEL_START':0x0b,
          'MC_GOTO_SLOW':0x17,
          'MC_SEEK_INDEX':0x19,
          'MC_MOVE_POS':0x24,
          'MC_MOVE_NEG':0x25,
          'GET_VER':0xfe,
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

# Utility functions
def checksum(msg):
    return ((~sum([ord(c) for c in msg]) + 1) ) & 0xFF

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
    for c in m:
        if ord(c)==0x3b :
            print
        print "0x%02x" %ord(c),
    print

def split_msgs(r,debug=False):
    '''
    Split the buffer into separate messages of the AUX protocol.
    '''
    l=r.find('\x3b')+1
    p=l
    ml=[]
    while p>-1:
        p=r.find('\x3b',l)
        #if debug : print l, p
        ml.append(r[l:p]+r[p])
        l=p+1
    if debug : print ml
    return ml

def parse_msg(m, debug=False):
    '''
    Parse bytes byond 0x3b. 
    Do not pass the message with preamble!
    '''
    l=ord(m[0])+1
    msg=m[:l]
    if debug :
        print 'Parse:',
        dprint(msg)
    if  chr(checksum(msg)) != m[l] :
        print 'Checksum error: %x vs. %02x' % (checksum(msg) , ord(m[l]))
        dprint(m)
    l,src,dst,mid=struct.unpack('4B',msg[:4])
    dat=msg[4:l+1]
    #print 'len:', l
    return l, src, dst, mid, dat



def parse_pos(d):
    '''
    Parse first three bytes into the DMS string
    '''
    if len(d)>=3 :
        pos=struct.unpack('!i','\x00'+d[:3])[0]/2**24
        return u'%03d°%02d\'%04.1f"' % f2dms(pos)
    else :
        return u''


# Target classes

class NSEScope(asynchat.async_chat):
    
    def __init__(self, addr='1.2.3.4', port=2000):
        asynchat.async_chat.__init__(self)
        self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
        self.connect((addr, port))
        self.ibuffer = []
        self.obuffer = ''
        self.reading_headers = True
        self.handling = False
        self.set_terminator('*HELLO*')
        self.startup=True
        self.me=targets['APP']
        
    # grab some more data from the socket,
    # throw it to the collector method,
    # check for the terminator,
    # if found, transition to the next state.
    # Modified from stdlib - we need empty 
    # strings passed to collect data.
    def handle_read (self):

        try:
            data = self.recv (self.ac_in_buffer_size)
        except socket.error, why:
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
            print 'A',
        try :
            return MsgType[trg](l,s,d,mid,dat)
        except KeyError :
            return nse_msg(l,s,d,mid,dat)
    
    def collect_incoming_data(self, data):
        '''Buffer the data'''
        self.ibuffer.append(data)
    
    def found_terminator(self):
        if self.startup :
            self.set_terminator('\x3b')
            self.startup=False
            self.ibuffer=[]
            print 'Hello!'
            sys.stdout.flush()
            return
        if self.reading_headers :
            print 'msg:',
            self.reading_headers=False
            self.ibuffer=[]
            self.set_terminator(1)
        elif self.handling :
            self.handling=False
            self.reading_headers=True
            self.set_terminator('\x3b')
            self.msg=''.join(self.ibuffer)
            #dprint(self.msg)
            print self.handle_msg(self.msg)
        else :
            self.handling=True
            self.msglen=ord(self.ibuffer[0][0])
            #print '(%2d)' % self.msglen,
            self.set_terminator(self.msglen+1)
        sys.stdout.flush()


# Message classes
class nse_msg:
    
    def __init__(self, l, s, d, i, data=None):
        self.l=l
        self.src=s
        self.dst=d
        self.mid=i 
        self.data=data
    
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
        return r+' '.join(['0x%02x' % ord(c) for c in self.data])

class nse_mc_msg(nse_msg):
    
    
    def __init__(self, l, s, d, i, data=None):
        nse_msg.__init__(self, l, s, d, i, data)
    
    def __repr__(self):
        r=self._repr_head()
        return (r+parse_pos(self.data)).encode('utf-8')

class nse_pos_msg(nse_mc_msg):

    def __init__(self, l, s, d, i, data=None):
        nse_mc_msg.__init__(self, l, s, d, i, data)


MsgType={
        0x10: nse_mc_msg,
        0x11: nse_mc_msg,
        }


