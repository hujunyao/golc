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

usage = """
        fetchlog.py  usage:
        -d specify day range, minday:maxday(default = 0.0:1.0)
        -u select user to fetch log file
        -f specify log file prefix (capture, sys, logcat, default = *)
       """

def zip_logs(zf, logs, logprefix):
  try:
    zlog = zipfile.ZipFile(logs)
    logfns = zlog.namelist()
    for log in logfns:
      match=logprefix + '*'
      if fnmatch.fnmatch(log, match):
        zlog.extract(log, 'logs/')
        zf.write('logs/'+log, log)
  except zipfile.BadZipfile:
    print f, 'is a BadZipfile'

if __name__ == '__main__':
  nd = '1.0'
  minsec = 0
  maxsec = 0
  logfiles = []
  user = '*'
  zfpre = ''

  os.system('rm -rf logs/')
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
    elif o == '-d':
      nd = a
    elif o == '-f':
      zfpre = a

  try:
    daysec = 24*60*60
    r = nd.find(':')
    if r != -1:
      minsec = daysec * float(nd[:r])
      maxsec = daysec * float(nd[r+1:])
      if minsec > maxsec:
        print usage
        sys.exit(1)
    else:
      maxsec = daysec * float(nd)
  except ValueError:
    print usage
    sys.exit(1)


  currsec = time.time()
  zf = zipfile.ZipFile('/tmp/logs.zip', 'w', zipfile.zlib.DEFLATED)

  logfiles = glob.glob('/archive/*/'+user+'/logcat*')
  for log in logfiles:
    dt = currsec - os.stat(log).st_mtime
    if dt > minsec and dt < maxsec:
      print log
      if zfpre == '':
        arcname = os.path.basename(log)
        zf.write(log, arcname)
      else:
        zip_logs(zf, log, zfpre)

  zf.close()
