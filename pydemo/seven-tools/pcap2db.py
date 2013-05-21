#!/usr/bin/python2.7
# -*- coding: utf_8 -*-
import getopt
import sys
import os
import pyshark
import glob
import time
import sqlite3
from datetime import datetime

class pcapDB(object):
  def __init__(self, user):
    self.radio_up = []
    self.gap = 0
    self.tm = float(0.0)
    self.logfile = 0
    self.user = user
    self.conn = sqlite3.connect('logdb/' + user + '.sqlite') 

  def setgap(self, sec):
    self.gap = sec
    #print 'gap', self.gap

  def loadpcap(self, pcapfile, field, exp):
    self.tm = float(0.0)
    pcapinfo = pyshark.read(pcapfile, field, exp)
    for pkt in pcapinfo:
      #print pkt
      tm = pkt['frame.time_epoch']
      if self.tm == 0:
        self.radio_up.append([pkt, pcapfile])
      elif tm < self.tm:
        print 'ERROR:', pkt, pcapfile
      elif tm - self.tm > self.gap:
        self.radio_up.append([pkt, pcapfile])

      self.tm = tm

  def dump(self):
    print 'pcapinfo dump:\n'
    cnt = 0
    for o in self.radio_up:
      o[0]['frame.time_epoch'] = datetime.utcfromtimestamp(o[0]['frame.time_epoch']).strftime('%Y/%m/%d %H:%M:%S.%f')
      try:
        proto = o[0]['ip.proto']
      except KeyError:
        proto = 0

      if o[1] != self.logfile:
        print o[1]
        self.logfile = o[1]
      if proto == 6:
        print o[0]#, o[1]
        dstport = o[0]['tcp.dstport']
        srcport = o[0]['tcp.srcport']
        cnt = cnt + 1
    print 'TCP radio up:', cnt

    cnt = 0
    for o in self.radio_up:
      try:
        proto = o[0]['ip.proto']
      except KeyError:
        proto = 0
      #o[0] = datetime.utcfromtimestamp(o[0]).strftime('%Y/%m/%d %H:%M:%S.%f')
      #o[3] = datetime.utcfromtimestamp(o[3]).strftime('%Y/%m/%d %H:%M:%S.%f')
      if o[1] != self.logfile:
        print o[1]
        self.logfile = o[1]

      if proto == 17:
        print o[0]#, o[1]
        cnt = cnt + 1
    print 'UDP radio up:', cnt

    cnt = 0
    for o in self.radio_up:
      try:
        proto = o[0]['ip.proto']
      except KeyError:
        proto = 0

      if o[1] != self.logfile:
        #print o[1]
        self.logfile = o[1]

      if proto == 0:
        #print o
        cnt = cnt + 1
    print 'ARP radio up:', cnt

  def __del__(self):
    self.conn.close()

if __name__ == '__main__':
  pcapfield = []
  pcapfield.append('frame.time_epoch')
  #pcapfield.append('ip.src')
  #pcapfield.append('ip.dst')
  pcapfield.append('ip.proto')
  pcapfield.append('tcp.srcport')
  pcapfield.append('tcp.dstport')
  #pcapfield.append('udp.length')
  #pcapfield.append('tcp.len')
  #pcapfield.append('sll.hatype')
  exp = '!dns and (sll.hatype == 1 or sll.hatype==512)'
  sec = 10
  user = ''

  try:
    opt, args = getopt.gnu_getopt(sys.argv[1:], 'u:p:f:e:h')
  except getopt.GetoptError, err:
    print err
    sys.exit(1)

  for o, a in opt:
    if o == '-h':
      print usage
      sys.exit(1)
    elif o == '-f':
      pf = a
    elif o == '-e':
      exp = a
    elif o == '-u':
      user = a
    elif o == '-p':
      sec = int(a)

  obj = pcapDB(user)
  obj.setgap(sec)
  logs = glob.glob(pf)
  #print logs, pf
  for log in logs:
    #print log
    obj.loadpcap(log, pcapfield, exp)

  obj.dump()
