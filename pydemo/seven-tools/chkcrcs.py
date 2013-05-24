#!/usr/bin/python2.7
# -*- coding: utf_8 -*-
import getopt
import sys
import os
import getopt
import glob
from datetime import datetime
import zipfile
import time
import fnmatch
import sqlite3

usage = """
        chkcrcs.py  usage:
        -u select user to chkcrcs
       """

def do_merge(sorted_tminfo):
  merged_tminfo = []
  idx = 0
  cnt = len(sorted_tminfo) - 1
  sorted_tminfo[0][0] = 0.0
  sorted_tminfo[cnt][1] = 0.0

  for tm in sorted_tminfo:
    if idx >= cnt:
      break
    ntm = sorted_tminfo[idx+1]
    r = ntm[0] - tm[1]
    if r < 60:
      tm[1] = 0.0
      ntm[0] = 0.0
      if r < 0:
        print 'may error'

    idx = idx + 1
    

def do_dump(sorted_tminfo):
  info = [0, 0]
  for tm in sorted_tminfo:
    if tm[1] > 0.0:
      d = datetime.utcfromtimestamp(tm[1])
      stm = d.strftime('%Y/%m/%d %H:%M:%S.%f')
      info[0] = stm
    elif tm[0] > 0.0:
      d = datetime.utcfromtimestamp(tm[0])
      etm = d.strftime('%Y/%m/%d %H:%M:%S.%f')
      info[1] = etm
      print 'PCAP-LOST: ', info

if __name__ == '__main__':
  user = ''

  try:
    optlist, args = getopt.gnu_getopt(sys.argv[1:], 'f:d:u:htap')
  except getopt.GetoptError, err:
    print usage, err
    sys.exit(1)

  for o, a in optlist:
    if o == '-h':
      print usage
      sys.exit(1)
    elif o == '-u':
      user = a


  if user == '':
    print usage
    sys.exit(1)

  crcsconn = sqlite3.connect('/root/logdb/' + user + '.sqlite')
  pcapconn = sqlite3.connect('/root/logdb/pcap-' + user + '.sqlite')
  c = crcsconn.cursor()
  rows = c.execute('select * from crcslog')
  cnt = 0
  rcnt = 0
  for row in rows:
    rcnt = rcnt + 1
    tm = row[0]
    c = pcapconn.cursor()
    tmprows = c.execute('select * from pcaplog where sdate < '+ str(tm) + ' and edate >' + str(tm))
    rn = 0
    for tmpr in tmprows:
      rn = rn + 1

    if rn == 0:
      #print row
      cnt = cnt + 1

  print 'total crcs log:', rcnt, ' non-pcap crcs log:', cnt

  tminfo = []
  c = pcapconn.cursor()
  pcaprows = c.execute('select * from pcaplog')
  for row in pcaprows:
    tminfo.append([row[0], row[1]])
  sorted_tminfo =sorted(tminfo, key=lambda tm: tm[0])
  do_merge(sorted_tminfo)
  print sorted_tminfo
  do_dump(sorted_tminfo)
  crcsconn.close()
  pcapconn.close()
