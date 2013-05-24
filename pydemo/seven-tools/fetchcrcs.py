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

usage = """
       -a specify app package name(com.skype.raider)
       -d specify day range, minday:maxday(default 0.0:1.0)
       -t set search target, demo013
       -u specify userid (crcs format)
       -h display help
      """
def do_search_logfile(log, user):
  fd = open(log)
  for line in fd:
    info = line.split(',')
    tms = info[0]
    uid = info[1]
    logtype = info[2]
    tms = tms.strip(' ')
    tmv = datetime.strptime(tms, '%Y-%m-%d  %H:%M:%S.%f')

def zip_appcrcslog(zf, log, user, app, appcrcs):
  #print log, app
  fd = open(log)
  wifiline = ''
  sline = ''
  is_wifi = False
  prev_is_wifi = False
  for line in fd:
    info = line.split(',')
    r = line.find(user)
    if r > 0:
      appcrcs.write(line)
      #if info[2] == 'netlog':
      #  nettype = info[16]
      #  if nettype == 'wifi':
      #    is_wifi = True
      #    wifiline = line
      #  else:
      #    is_wifi = False

      #  if prev_is_wifi != is_wifi:
      #    prev_is_wifi = is_wifi
      #    if is_wifi:
      #      sline = line
      #    else:
      #      netinfo.write('S,'+sline)
      #      netinfo.write('E,'+wifiline)

      #if line.find(app)  > 0 :
      #  #print line

def getlogfiles(t):
  s = int(t[4:]) + 26
  logpath = '/usr/local/seven/arm0'+str(s)+'*/logs/crcs*'
#  print logpath
  logs = glob.glob(logpath)
  return logs

if __name__ == '__main__':
  nd = '1.0'
  app = ''
  minsec = 0
  maxsec = 0
  logfiles = []
  user = ''

  try:
    optlist, args = getopt.gnu_getopt(sys.argv[1:], 'u:a:d:t:hfp')
  except getopt.GetoptError, err:
    print usage
    sys.exit(1)
  for o, a in optlist:
    if o == '-h':
      print usage
      sys.exit(1)
    elif o == '-t':
      logfiles = getlogfiles(a)
    elif o == '-d':
      nd = a
    elif o == '-u':
      user = a 
    elif o == '-a':
      app = a

  if len(logfiles) <= 0:
    print usage
    sys.exit(1)

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

  print minsec, maxsec,app

  currsec = time.time()
  zf = zipfile.ZipFile('/tmp/crcs.zip', 'w', zipfile.zlib.DEFLATED)
  hostname = os.uname()[1]
  basename = hostname+'-'+user +'.crcs'

  if app != '' or user != '':
    appcrcs = open('/tmp/'+basename, 'w')

  #netinfo = open('/tmp/'+basename[:-5]+'.netinfo', 'w')
  for log in logfiles:
    dt = currsec - os.stat(log).st_mtime
    if dt > minsec and dt < maxsec:
      if app == '' and user == '':
        #print log
        arcname = os.path.basename(log)
        zf.write(log, arcname)
      else:
        zip_appcrcslog(zf, log, user, app, appcrcs)
        appcrcs.flush()

  if app != '' or user != '':
    zf.write('/tmp/'+basename, basename)
    appcrcs.close()

  #zf.write('/tmp/'+basename[:-5]+'.netinfo', basename[:-5]+'.netinfo')
  #netinfo.close()

  zf.close()
