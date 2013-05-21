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

usage = """
          -h display help message
        """

def get_crcs_sec(d):
  YSEC = 365 * 24 * 60 * 60
  MSEC = 30 * 24 * 60 * 60
  DSEC = 24 * 60 * 60
  HSEC = 60 * 60
  MSEC = 60
  sec = 0.0

  sec = (d.year-2000)*YSEC + d.month * MSEC + d.day * DSEC + d.hour * HSEC + d.minute * MSEC + d.second  + d.microsecond * 0.000001
  return sec

class pcapDB(object):
  def __init__(self, user):
    self.radio_up = []
    self.gap = 0
    self.tm = float(0.0)
    self.logfile = 0
    self.user = user
    self.conn = sqlite3.connect('/root/logdb/' + user + '.sqlite') 

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
    appcnt = 0
    c = self.conn.cursor()
    for o in self.radio_up:
      tm = o[0]['frame.time_epoch']
      d = datetime.utcfromtimestamp(tm)
      o[0]['frame.time_epoch'] = d.strftime('%Y/%m/%d %H:%M:%S.%f')
      #o[0]['frame.time_epoch'] = o[0]['frame.time_epoch'].strftime('%Y/%m/%d %H:%M:%S.%f')
      try:
        proto = o[0]['ip.proto']
      except KeyError:
        proto = 0

      if o[1] != self.logfile:
        print o[1]
        self.logfile = o[1]
      if proto == 6:
        #print o[0]#, o[1]
        dstport = o[0]['tcp.dstport']
        srcport = o[0]['tcp.srcport']
        cnt = cnt + 1
        #tm = get_crcs_sec(d)
        rows = c.execute("select * from crcslog where rport = '%d' and (date > %f and date <%f)" % (srcport, tm-60, tm + 60))
        #rows = c.execute("select * from crcslog where rport = '%d' " % (srcport))
        rn = 0
        for row in rows:
          print '\n', row
          rn = rn + 1
        #print tm, srcport, dstport, rn
        if rn > 0:
          appcnt = appcnt + 1
          print o[0], rn

    print 'TCP radio up:', cnt, 'App radio up:', appcnt
    #sys.exit(1)
    cnt = 0
    for o in self.radio_up:
      try:
        proto = o[0]['ip.proto']
      except KeyError:
        proto = 0
      #o[0] = datetime.utcfromtimestamp(o[0]).strftime('%Y/%m/%d %H:%M:%S.%f')
      #o[3] = datetime.utcfromtimestamp(o[3]).strftime('%Y/%m/%d %H:%M:%S.%f')
      if o[1] != self.logfile:
        #print o[1]
        self.logfile = o[1]

      if proto == 17:
        #print o[0]#, o[1]
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
  sec = 5
  user = ''

  os.environ['TZ'] = 'GMT'
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
  logs = glob.glob('logs/*.pcap')
  #print logs, pf
  for log in logs:
    #print log
    obj.loadpcap(log, pcapfield, exp)

  obj.dump()
