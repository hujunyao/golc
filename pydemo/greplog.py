#!/usr/bin/python2.7
# -*- coding: utf_8 -*-
import zipfile
import os
import time
import sys
import fnmatch
import re
import getopt

class logfiles:
  def __init__(self, minsec, maxsec):
    self.maxsec = maxsec
    self.minsec = minsec
    self.currsec = time.time()
    self.filearray = []

  def append(self, filename):
    self.filearray.append(filename)

def visitdir(arg, dirname, names):
  for filename in names:
    f = os.path.join(dirname, filename)
    dt = arg.currsec - os.stat(f).st_mtime
    if dt > arg.minsec and dt  < arg.maxsec and fnmatch.fnmatch(f, '*.zip'):
      arg.append(f)

def do_regex_file(zf, log, pattern, search, debugfd):
  logfd = zf.open(log)
  cnt = 0
  output = []
  content = logfd.readlines()
  for line in content:
    cnt = cnt+1
    if pattern:
      mo = pattern.match(line)
      if mo:
        output.append(str(cnt)+' '+line)
    elif search != "":
      r = line.find(search)
      if r >= 0:
        output.append(str(cnt)+' '+line)

  if len(output) > 0:
    debugfd.write(zf.filename+' -->\n')
    debugfd.write('-----'+log+'-----\n')
    print '-----', log, '-----'
    for o in output:
      debugfd.write(o+'\n')
      print o

def do_regex_grep(filearray, pattern, search):
  debugfd = open('/tmp/debug.log', 'w')
  for f in filearray:
    try:
      zf = zipfile.ZipFile(f)
      logfns = zf.namelist()
      print f, '-->'
      for log in logfns:
        if fnmatch.fnmatch(log, '*.log'):
          do_regex_file(zf, log, pattern, search, debugfd)
      zf.close()
    except zipfile.BadZipfile:
      print f, 'is a zipfile.BadZipfile'
  debugfd.close()

def do_grep(dirpath, minsec, maxsec, pattern, search):
  patt = None
  if pattern != "":
    patt = re.compile(pattern)
  arg = logfiles(minsec, maxsec)
  os.path.walk(dirpath, visitdir, arg)
  do_regex_grep(arg.filearray, patt, search)
 
if __name__ == '__main__':
  usage = """
    usage: ./greplog.py -d 1.0 -s patt
    -d day minday:maxday range (-d 0.0:1.0)
    -g regex search
    -s simple text search
  """
  pattern = ""
  search = ""
  nd = "1.0"
  maxsec = 0
  minsec = 0
  try:
    optlist, args = getopt.gnu_getopt(sys.argv[1:], 's:d:g:h')
  except getopt.GetoptError, err:
    print usage
    sys.exit(1)

  for o, a in optlist:
    if o == '-d':
      nd = a
    elif o == '-h':
      print usage
      sys.exit(1)
    elif o == '-g':
      pattern = a
    elif o == '-s':
      search = a

  if pattern == "" and search == "":
    print "erorr: check '-g' or -s argument"
    print usage
    sys.exit(1)

  try:
    daysec = 24*60*60
    r = nd.find(':')
    if r != -1:
      minsec = daysec * float(nd[:r])
      maxsec = daysec * float(nd[r+1:])
      if minsec > maxsec:
        print "erorr: check '-d' argument"
        print usage
        sys.exit(1)
    else:
      maxsec = daysec * float(nd)

    print daysec, minsec, maxsec, nd, pattern
  except ValueError:
    print "erorr: check '-d' argument"
    print usage
    sys.exit(1)

  do_grep('/archive', minsec, maxsec, pattern, search)
