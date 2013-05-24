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
from netinfo import netinfo
from s2info import s2info
import csv

usage = """
          -h display help message
        """

def chk_date_range(tm, tmarray):
  low = 0
  ishit = False
  high = len(tmarray) - 1
  while low <= high:
    try:
      mid = (low + high)//2
      midtm = tmarray[mid]
      if midtm[1] < tm:
        low = mid + 1
      elif midtm[0] > tm:
        high = mid - 1
      else:
        ishit = True
        break
    except IndexError:
      print mid

  return ishit,mid
  
class pcapDB(object):
  def __init__(self, user):
    self.radio_up = []
    self.gap = 0
    self.tm = float(0.0)
    self.logfile = 0
    self.user = user
    self.conn = sqlite3.connect('/root/logdb/' + user + '.sqlite') 
    self.pcapconn = sqlite3.connect('/root/logdb/pcap-'+user+'.sqlite')
    c = self.pcapconn.cursor()
    cmd = 'CREATE TABLE pcaplog (sdate real, edate real, info text)'
    c.execute(cmd)
    self.pcapconn.commit()

  def setgap(self, sec):
    self.gap = sec

  def loadpcap(self, pcapfile, field, exp):
    self.tm = float(0.0)
    pcapinfo = pyshark.read(pcapfile, field, exp)
    stm = 0.0
    cmd = 'INSERT INTO pcaplog VALUES ('
    for pkt in pcapinfo:
      #print pkt
      tm = pkt['frame.time_epoch']
      if self.tm == 0:
        self.radio_up.append([pkt, pcapfile])
        stm = tm
      #elif tm < self.tm:
      #print 'ERROR:', pkt, pcapfile
      elif tm - self.tm > self.gap:
        self.radio_up.append([pkt, pcapfile])

      self.tm = tm
    cmd = cmd + str(stm) + ','+str(self.tm)+',"'+pcapfile+'")'
    try:
      c = self.pcapconn.cursor()
      c.execute(cmd)
      self.pcapconn.commit()
    except sqlite3.OperationalError:
      print cmd

  def dump(self, s0, s1):
    print 'pcapinfo dump:\n'
    s2cnt = 0
    apps2cnt = 0
    cnt = 0
    appcnt = 0
    infofd = open('out/'+self.user+'-detail.txt', 'w')
    c = self.conn.cursor()
    for o in self.radio_up:
      tm = o[0]['frame.time_epoch']
      is_wifi = False
      is_s2 = False
      if s0 != 0 and s1 != 0 and (tm < s0 or tm > s1):
        continue
      is_wifi,idx = chk_date_range(tm, netinfo)
      #for ni in netinfo:
      #  if tm >= ni[0] and tm <= ni[1]:
      #    is_wifi = True
      #    break
      if is_wifi:
        continue

      is_s2,idx = chk_date_range(tm, s2info)
      #for s2obj in s2info:
      #  if tm >= s2obj[0] and tm <= s2obj[1]:
      #    is_s2 = True
      #    break

      d = datetime.utcfromtimestamp(tm)
      #o[0]['frame.time_epoch'] = o[0]['frame.time_epoch'].strftime('%Y/%m/%d %H:%M:%S.%f')
      try:
        proto = o[0]['ip.proto']
      except KeyError:
        proto = 0

      if o[1] != self.logfile:
        infofd.write(o[1] + '\n')
        self.logfile = o[1]
      if proto == 6:
        if is_s2:
          infofd.write(str(o[0]) + ' TS2'+str(idx) + '\n')
        else:
          infofd.write(str(o[0]) + '\n')
        dstport = o[0]['tcp.dstport']
        srcport = o[0]['tcp.srcport']
        o[0]['frame.time_epoch'] = d.strftime('%Y/%m/%d %H:%M:%S.%f')
        cnt = cnt + 1
        if is_s2:
          s2cnt = s2cnt + 1
          s2info[idx][3] = s2info[idx][3] + 1
        #tm = get_crcs_sec(d)
        rows = c.execute("select * from crcslog where rport = '%d' and (date > %f and date <%f)" % (srcport, tm-60, tm + 60))
        #rows = c.execute("select * from crcslog where rport = '%d' " % (srcport))
        rn = 0
        for row in rows:
          #print '\n', row
          rn = rn + 1
        #print tm, srcport, dstport, rn
        if rn > 0:
          appcnt = appcnt + 1
          if is_s2:
            apps2cnt = apps2cnt + 1
            s2info[idx][4] = s2info[idx][4] + 1
          #print o[0], rn

    detail = 'TCP radio up:'+ str(s2cnt) + '/' + str(cnt) + ' App radio up:' +str(apps2cnt) + '/' + str(appcnt)
    print detail
    infofd.write(detail + '\n')

    s2cnt = 0
    dnss2cnt = 0
    cnt = 0
    dnscnt = 0
    for o in self.radio_up:
      tm = o[0]['frame.time_epoch']
      is_wifi = False
      is_s2 = False
      if s0 != 0 and s1 != 0 and (tm < s0 or tm > s1):
        continue

      is_wifi,idx = chk_date_range(tm, netinfo)
      #for ni in netinfo:
      #  if tm >= ni[0] and tm <= ni[1]:
      #    is_wifi = True
      #    break
      if is_wifi:
        continue

      is_s2,idx = chk_date_range(tm, s2info)
      #for s2obj in s2info:
      #  if tm >= s2obj[0] and tm <= s2obj[1]:
      #    is_s2 = True
      #    break

      try:
        proto = o[0]['ip.proto']
      except KeyError:
        proto = 0
      #o[0] = datetime.utcfromtimestamp(o[0]).strftime('%Y/%m/%d %H:%M:%S.%f')
      #o[3] = datetime.utcfromtimestamp(o[3]).strftime('%Y/%m/%d %H:%M:%S.%f')
      if o[1] != self.logfile:
        infofd.write(o[1] + '\n')
        self.logfile = o[1]

      if proto == 17:
        if is_s2:
          infofd.write(str(o[0]) + ' US2'+str(idx) + '\n')
        else:
          infofd.write(str(o[0]) + '\n')
        cnt = cnt + 1
        if is_s2:
          s2cnt = s2cnt + 1
          s2info[idx][5] = s2info[idx][5] + 1
        if o[0]['udp.dstport'] == 53:
          dnscnt = dnscnt + 1
          if is_s2:
            dnss2cnt = dnss2cnt + 1
            s2info[idx][6] = s2info[idx][6] + 1
    detail = 'UDP radio up: ' +str(s2cnt)+'/'+ str(cnt) + ' DNS radio up: '+str(dnss2cnt)+'/' + str(dnscnt)
    print detail
    infofd.write(detail + '\n')
    infofd.close()

  def out_s2info_csv(self):
    csvfd = open('out/'+self.user+'-s2info.csv', 'wb')
    wfd = csv.writer(csvfd)
    wfd.writerow(['SDATE', 'EDATE', 'TIME', 'TCP radio up', 'APP radio up', 'UDP radio up', 'DNS radio up', 'IDX'])
    idx = 0
    for si in s2info:
      tl = list(si)
      tl.append(idx)
      sd = datetime.utcfromtimestamp(tl[0])
      ed = datetime.utcfromtimestamp(tl[1])
      tl[2] = '%.3f sec' %(tl[1] - tl[0])
      tl[0] = sd.strftime('%Y/%m/%d %H:%M:%S.%f')
      tl[1] = ed.strftime('%Y/%m/%d %H:%M:%S.%f')
      wfd.writerow(tl)
      idx = idx + 1

    csvfd.close()

      
  def __del__(self):
    self.conn.close()
    self.pcapconn.close()

if __name__ == '__main__':
  pcapfield = []
  pcapfield.append('frame.time_epoch')
  pcapfield.append('frame.number')
  #pcapfield.append('ip.src')
  #pcapfield.append('ip.dst')
  pcapfield.append('ip.proto')
  pcapfield.append('tcp.srcport')
  pcapfield.append('tcp.dstport')
  pcapfield.append('udp.srcport')
  pcapfield.append('udp.dstport')
  #pcapfield.append('udp.length')
  #pcapfield.append('tcp.len')
  #pcapfield.append('sll.hatype')
  #exp = '!dns and (sll.hatype == 1 or sll.hatype==512)'
  exp = 'sll.etype != 0x886c and sll.hatype != 772'
  sec = 5
  user = ''
  dS0 = 0.0
  dS1 = 0.0

  os.environ['TZ'] = 'GMT'
  try:
    opt, args = getopt.gnu_getopt(sys.argv[1:], 'r:u:p:f:e:h')
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
    elif o == '-r':
      r = a.split(':')
      dS0 = float(r[0])
      dS1 = float(r[1])
      #print r[0], r[1], dS0, dS1

  os.system('rm -f /root/logdb/pcap-'+user+'.sqlite')
  obj = pcapDB(user)
  obj.setgap(sec)
  logs = glob.glob('logs/*.pcap')
  #print logs, pf
  for log in logs:
    #print log
    obj.loadpcap(log, pcapfield, exp)

  obj.dump(dS0, dS1)
  obj.out_s2info_csv()
