#!/usr/bin/python2.7
# -*- coding: utf_8 -*-
import getopt
import sys
import os
import getopt
import glob
from datetime import datetime
from datetime import tzinfo
from datetime import timedelta
import time
import sqlite3

usage = """
        -h display help message
        -u specify the user id
       """

class crcsDB(object):
  def __init__(self, user):
    self.conn = sqlite3.connect('logdb/' + user+'.sqlite')
    c = self.conn.cursor()
    cmd = 'CREATE TABLE crcslog (date real, lport INTEGER, rport INTEGER, app text, info text)'
    c.execute(cmd)
    self.conn.commit()
    self.user = user
    self.sortcrcslog = {}
    self.s2total_s0 = 0.0
    self.s2total_s2 = 0.0
    self.nitotal_s0 = 0.0
    self.nitotal_s2 = 0.0

    s0 = os.environ['S0']
    s1 = os.environ['S1']
    s2 = os.environ['S2']
    s3 = os.environ['S3']
    tmvS0 = datetime.strptime(s0, '%Y-%m-%d  %H:%M:%S.%f')
    tmvS1 = datetime.strptime(s1, '%Y-%m-%d  %H:%M:%S.%f')
    tmsS0 = tmvS0.strftime('%s.%f')
    tmsS1 = tmvS1.strftime('%s.%f')
    self.tms0 = float(tmsS0)
    self.tms1 = float(tmsS1)

    tmvS0 = datetime.strptime(s2, '%Y-%m-%d  %H:%M:%S.%f')
    tmvS1 = datetime.strptime(s3, '%Y-%m-%d  %H:%M:%S.%f')
    tmsS0 = tmvS0.strftime('%s.%f')
    tmsS1 = tmvS1.strftime('%s.%f')
    self.tms2 = float(tmsS0)
    self.tms3 = float(tmsS1)

  def dump_timeinfo(self):
    s0 = os.environ['S0']
    s1 = os.environ['S1']
    s2 = os.environ['S2']
    s3 = os.environ['S3']
    r0 = s0.find(' ')
    r1 = s1.find(' ')
    r2 = s2.find(' ')
    r3 = s3.find(' ')

    print 'S2 time statistic:'
    print '\t %s %s %.3f sec(%.3f days)' %(s0[:r0], s1[:r1], self.s2total_s0, self.s2total_s0/(60*60*24))
    print '\t %s %s %.3f sec(%.3f days)' %(s2[:r2], s3[:r3], self.s2total_s2, self.s2total_s2/(60*60*24))
#    print '\t', s0, '--', s1, self.s2total_s0, 'sec (', self.s2total_s0/(60*60*24),'days)'
#    print '\t', s2, '--', s3, self.s2total_s2, 'sec (', self.s2total_s2/(60*60*24),'days)'

    print 'WIFI time statistic:'
    print '\t %s %s %.3f sec(%.3f days)' %(s0[:r0], s1[:r1], self.nitotal_s0, self.nitotal_s0/(60*60*24))
    print '\t %s %s %.3f sec(%.3f days)' %(s2[:r2], s3[:r3], self.nitotal_s2, self.nitotal_s2/(60*60*24))
#    print '\t', s0, '--', s1, self.nitotal_s0, 'sec (', self.nitotal_s0/(60*60*24),'days)'
#    print '\t', s2, '--', s3, self.nitotal_s2, 'sec (', self.nitotal_s2/(60*60*24),'days)'

  def sort_netinfo(self, netinfo, nipy):
    sortnetinfo = {}
    fd = open(netinfo)
    for line in fd:
      info = line.split(',')
      nline = fd.next()
      sortnetinfo[info[1]] = (line, nline)
    fd.close()

    fd = open(netinfo+'-sorted', 'w')
    for key in sorted(sortnetinfo.iterkeys()):
      fd.write(sortnetinfo[key][0])
      fd.write(sortnetinfo[key][1])
    fd.close()

  def load_crcslog(self, crcslog):
    fd = open(crcslog)
    for line in fd:
      info = line.split(',')
      self.sortcrcslog[info[0]] = line
    fd.close()
      
  def out_netlog(self):
    fd = open('logdb/' + self.user+'.netlog', 'w')
    for key in sorted(self.sortcrcslog.iterkeys()):
      line = self.sortcrcslog[key]
      info = line.split(',')
      if info[2] == 'netlog':
        fd.write(line)
    fd.close()

    fd = open('logdb/' + self.user+'.netlog.wow', 'w')
    for key in sorted(self.sortcrcslog.iterkeys()):
      line = self.sortcrcslog[key]
      info = line.split(',')
      if info[2] == 'netlog':
        if info[16] != 'wifi' and info[16] != 'sms':
          fd.write(line)
    fd.close()

  def out_s2info(self):
    cnt = 0
    s2line = ''
    is_S2 = False
    prev_is_S2 = False
    fd = open('logdb/' + self.user+'.netlog.wow')
    s2fd = open('logdb/' + self.user+'.s2info', 'w')
    for line in fd:
      r = line.find(']FS[')
      if r > 0:
        if line[r+4:].find('S2') > 0:
          is_S2 = True
          s2line = line
          cnt = cnt + 1
        else:
          is_S2 = False
      else:
        is_S2 = False

      if prev_is_S2 != is_S2:
        prev_is_S2 = is_S2
        if is_S2:
          sline = line
        else:
          if cnt > 1:
            s2fd.write('S,'+sline)
            s2fd.write('E,'+s2line)
          cnt = 0

    if is_S2 and cnt > 1:
      s2fd.write('S,'+sline)
      s2fd.write('E,'+s2line)

    fd.close()
    s2fd.close()

  def out_netinfo(self):
    cnt = 0
    is_wifi = False
    prev_is_wifi = False
    fd = open('logdb/' + self.user+'.netlog')
    nifd = open('logdb/' + self.user + '.netinfo', 'w')
    for line in fd:
      info = line.split(',')
      nettype = info[16]

      if nettype == 'sms':
        continue
      if nettype == 'wifi':
        is_wifi = True
        wifiline = line
        cnt = cnt + 1
      else:
        is_wifi = False

      if prev_is_wifi != is_wifi:
        prev_is_wifi = is_wifi
        if is_wifi:
          sline = line
        else:
          if cnt > 1:
            nifd.write('S,'+sline)
            nifd.write('E,'+wifiline)
          cnt = 0

    if is_wifi and cnt > 1:
      nifd.write('S,'+sline)
      nifd.write('E,'+wifiline)

    fd.close()
    nifd.close()

  def chk_date(self, stms, etms, is_s2):
    ishit = False
    if self.tms0 == 0 or self.tms1 == 0 or self.tms2 == 0 or self.tms3 == 0:
      return True

    if (stms >= self.tms0 and stms <= self.tms1) or (etms >= self.tms0 and etms <= self.tms1):
      if etms > self.tms1:
        etms = self.tms1
      if stms < self.tms0:
        stms = self.tms0
      ishit = True
      if is_s2:
        self.s2total_s0 = self.s2total_s0 + (etms - stms)
      else:
        self.nitotal_s0 = self.nitotal_s0 + (etms - stms)

    if (stms >= self.tms2 and stms <= self.tms3) or (etms >= self.tms2 and etms <= self.tms3):
      if etms > self.tms3:
        etms = self.tms3
      if stms < self.tms2:
        stms = self.tms2
      ishit = True
      if is_s2:
        self.s2total_s2 = self.s2total_s2 + (etms - stms)
      else:
        print self.nitotal_s2
        self.nitotal_s2 = self.nitotal_s2 + (etms - stms)

    return ishit

  def load_s2info(self, s2info, s2py):
    fd = open(s2info)
    for line in fd:
      info = line.split(',')
      nline = fd.next()
      ninfo = nline.split(',')
      if info[0] == 'S' and ninfo[0] == 'E':
        stmv = datetime.strptime(info[1].strip(' '), '%Y-%m-%d  %H:%M:%S.%f')
        stms = stmv.strftime('%s.%f')
        etmv = datetime.strptime(ninfo[1].strip(' '), '%Y-%m-%d  %H:%M:%S.%f')
        etms = etmv.strftime('%s.%f')
        if float(stms) > float(etms):
          print s2info, line
          print s2info, nline
        else:
          if self.chk_date(float(stms), float(etms), True):
            s2py.write('['+stms+','+etms+',0,0,0,0,0],')

    fd.close()

  def load_netinfo(self, netinfo, nipy):
    fd = open(netinfo)
    for line in fd:
      info = line.split(',')
      nline = fd.next()
      ninfo = nline.split(',')
      if info[0] == 'S' and ninfo[0] == 'E':
        stmv = datetime.strptime(info[1].strip(' '), '%Y-%m-%d  %H:%M:%S.%f')
        stms = stmv.strftime('%s.%f')
        etmv = datetime.strptime(ninfo[1].strip(' '), '%Y-%m-%d  %H:%M:%S.%f')
        etms = etmv.strftime('%s.%f')
        if float(stms) > float(etms):
          print netinfo, line
          print netinfo, nline
        else:
          #print self.nitotal_s2, stms, etms
          if self.chk_date(float(stms), float(etms), False):
            nipy.write('('+stms+','+etms+'),')
      else:
        print line
        print nline

    fd.close()

  def out_appdb(self, app):
    fd = open('logdb/'+self.user+'.netlog')
    c = self.conn.cursor()
    for line in fd:
      #print line
      if line.find(app) < 0:
        continue

      info = line.split(',')
      r0 = line.find('N[')
      if r0 > 0:
        r1 = line[r0:].find(']')
        portInfo = line[r0+3:r0+r1]
        ports = portInfo.split('/')
        if ports[0] == '':
          ports[0] = "0"
        if ports[1] == '':
          ports[1] = "0"
        if ports[0] == '0' and ports[1] == '0':
          print line

        tmv = datetime.strptime(info[0].strip(' '), '%Y-%m-%d  %H:%M:%S.%f')
        tms = tmv.strftime('%s.%f')
        cmd = 'INSERT INTO crcslog VALUES (' + tms +', '+ports[0]+','+ports[1]+',"'+info[11]+'","' + line + '")'
#        print cmd
        try:
          c.execute(cmd)
        except sqlite3.OperationalError:
          print cmd

    self.conn.commit()
    fd.close()

  def __del__(self):
    self.conn.close()

if __name__ == '__main__':
  nd = '1.0'
  app = ''
  minsec = 0
  maxsec = 0
  logfiles = []
  user = ''

  os.environ['TZ'] = 'GMT'
#  os.environ['S0'] = '2013-5-10 7:00:00.000'
#  os.environ['S1'] = '2013-5-16 6:59:59.999'
#  os.environ['S2'] = '2013-5-16 7:00:00.000'
#  os.environ['S3'] = '2013-5-18 6:59:59.999'

  try:
    optlist, args = getopt.gnu_getopt(sys.argv[1:], 'a:u:h')
  except getopt.GetoptError, err:
    print usage
    sys.exit(1)
  for o, a in optlist:
    if o == '-h':
      print usage
      sys.exit(1)
    elif o == '-u':
      user = a 
    elif o == '-a':
      app = a

  crcsdb = crcsDB(user)
  
  logs = glob.glob('logdb/*.crcs')
  for log in logs:
    #print log
    crcsdb.load_crcslog(log)

  crcsdb.out_netlog()
  crcsdb.out_netinfo()
  crcsdb.out_appdb(app)
  crcsdb.out_s2info()

  nipy = open('logdb/'+user+'.netinfo.py', 'w')
  nipy.write('netinfo = [')
  crcsdb.load_netinfo('logdb/'+user+'.netinfo', nipy)
  nipy.write(']')
  nipy.close()

  s2py = open('logdb/'+user+'.s2info.py', 'w')
  s2py.write('s2info = [')
  crcsdb.load_s2info('logdb/'+user+'.s2info', s2py)
  s2py.write(']')
  s2py.close()

  crcsdb.dump_timeinfo()
#  logs = glob.glob('logdb/*.netlog')
#  for log in logs:
#    crcsdb.load_netinfo(log, nipy)
#    crcsdb.sort_netinfo(log, nipy)
