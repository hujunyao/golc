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

def get_crcs_sec(d):
  YSEC = 365 * 24 * 60 * 60
  MSEC = 30 * 24 * 60 * 60
  DSEC = 24 * 60 * 60
  HSEC = 60 * 60
  MSEC = 60
  sec = 0.0

  sec = (d.year-2000)*YSEC + d.month * MSEC + d.day * DSEC + d.hour * HSEC + d.minute * MSEC + d.second  + d.microsecond * 0.000001
  return sec

class crcsDB(object):
  def __init__(self, user):
    self.conn = sqlite3.connect('logdb/' + user+'.sqlite')
    c = self.conn.cursor()
    cmd = 'CREATE TABLE crcslog (date real, lport INTEGER, rport INTEGER, app text, info text)'
    c.execute(cmd)
    self.conn.commit()
    self.user = user

  def load_crcslog(self, crcslog):
    fd = open(crcslog)
    c = self.conn.cursor()
    for line in fd:
      #print line
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
#        tmvutc = datetime(tmv.year, tmv.month, tmv.day, tmv.hour, tmv.minute, tmv.second, tmv.microsecond, crcsgmt)
#        tmv.astimezone(crcsgmt)
#        tmv.replace(crcsgmt)
        tms = tmv.strftime('%s.%f')
#        tms =  str(get_crcs_sec(tmv))
        cmd = 'INSERT INTO crcslog VALUES (' + tms +', '+ports[0]+','+ports[1]+',"'+info[11]+'","' + line + '")'
        print cmd
        try:
          c.execute(cmd)
        except sqlite3.OperationalError:
          print cmd

    self.conn.commit()

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
  try:
    optlist, args = getopt.gnu_getopt(sys.argv[1:], 'u:h')
  except getopt.GetoptError, err:
    print usage
    sys.exit(1)
  for o, a in optlist:
    if o == '-h':
      print usage
      sys.exit(1)
    elif o == '-u':
      user = a 

  crcsdb = crcsDB(user)
  
  logs = glob.glob('logdb/*.crcs')
  for log in logs:
    print log
    crcsdb.load_crcslog(log)
