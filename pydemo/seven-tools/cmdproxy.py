#!/usr/bin/python2.7
# -*- coding: utf_8 -*-
import getopt
import sys
import os
from datetime import datetime
import zipfile

if __name__ == '__main__':
  ssh_cmd = ['ssh -q voyeur@ap4.z.red', 'ssh -q voyeur@ap5.z.red']
  rcmd = '/home/voyeur/bin/fetchcrcs.py'
  user = ''
  crcsuser = ''
  sec = '5'
  app = ''

  os.environ['TZ'] = 'GMT'
  s0 = os.environ['S0']
  s1 = os.environ['S1']
  s2 = os.environ['S2']
  s3 = os.environ['S3']
  os.system('mkdir -p logdb')
  os.system('rm -rf logdb/*.sqlite')
  os.system('rm -rf logdb/*.crcs')
  os.system('rm -rf logdb/*.pcap')
#  os.system('rm -rf logdb/*.zip')

#  tmvS0 = datetime.strptime(s0, '%Y-%m-%d  %H:%M:%S.%f')
#  tmvS1 = datetime.strptime(s1, '%Y-%m-%d  %H:%M:%S.%f')
#  tmsS0 = tmvS0.strftime('%s.%f')
#  tmsS1 = tmvS1.strftime('%s.%f')
#  print s0, s1, tmsS0, tmsS1
#  sys.exit(1)

#convert IMEI userid to CRCS userid
  try:
    optlist, args = getopt.gnu_getopt(sys.argv[1:], 'p:u:a:hftd')
  except getopt.GetoptError, err:
    print err
    sys.exit(1)
  for o, a in optlist:
    if o == '-u':
      user = a
      try:
        crcsuser = str(hex(int(user))).strip('0x')
      except ValueError:
        crcsuser = user.lower() #str(hex(int(user))).strip('0x')
    elif o == '-p':
      sec = a
    elif o == '-a':
      app = a

  s = ''
  for arg in sys.argv[1:]:
     s = s + ' ' + arg
  print '\033[94m./cmdproxy.py' + s + '\033[0m'

  for cmd in ssh_cmd:
    realcmd = cmd + " '" + rcmd + s +"'"
    realcmd = realcmd.replace(user, crcsuser)
    print realcmd
    os.system(realcmd)

  if s != '' and s.find('-h') < 0:
    postcmd = ['scp -q voyeur@ap4.z.red:/tmp/crcs.zip /tmp/crcs-ap4.zip', 'scp -q voyeur@ap5.z.red:/tmp/crcs.zip /tmp/crcs-ap5.zip']
    for cmd in postcmd:
      print 'sgw:', cmd
      os.system(cmd)

  print 'sgw: fetch files from squid3.red'
  realcmd = "ssh -q root@squid3.red '" + '/opt/seven/bin/fetchlog.py ' + s +"'"
  #print 'sgw:', realcmd
  os.system(realcmd)

##uncompress logfile to logdb
  print 'sgw:', 'uncompress all crcslog to logdb'
  os.system('unzip -q -d logdb /tmp/crcs-ap4.zip')
  os.system('unzip -q -d logdb /tmp/crcs-ap5.zip')
  #os.system('unzip -q -d logdb /tmp/logs.zip')

##convert crcslog to sqlite3 db
  print 'sgw:', 'process all crcslog'
  cmd = './crcs2db.py -u ' + user + ' -a ' + app
  print cmd
  os.system(cmd)
  os.system('scp -q logdb/'+user+'.sqlite root@squid3.red:/root/logdb/')
  os.system('scp -q logdb/'+user+'.netinfo.py root@squid3.red:/opt/seven/bin/netinfo.py')
  os.system('scp -q logdb/'+user+'.s2info.py root@squid3.red:/opt/seven/bin/s2info.py')
  #sys.exit(1)

##parse all pcap file 
  cmd = "ssh -q root@squid3.red '/opt/seven/bin/pcapinfo.py -u " + user + "' -p " + sec
  try:
    tmvS0 = datetime.strptime(s0, '%Y-%m-%d  %H:%M:%S.%f')
    tmvS1 = datetime.strptime(s1, '%Y-%m-%d  %H:%M:%S.%f')
    tmsS0 = tmvS0.strftime('%s.%f')
    tmsS1 = tmvS1.strftime('%s.%f')
    cmd = cmd + ' -r ' + tmsS0 + ':'+tmsS1
  except ValueError:
    print s0, s1, cmd

  #print 'sgw:', 'process pcap file'
  print cmd
  print s0, s1#, cmd
  os.system(cmd)

##copy output file from squid3.red
  s0 = s0.replace(' ', '_')
  s1 = s1.replace(' ', '_')
  cmd = 'scp -q root@squid3.red:/root/out/'+user+'-detail.txt logdb/'+user+'-detail-'+s0+'--'+s1+'.txt'
  print 'sgw:', cmd
  os.system(cmd)
  cmd = 'scp -q root@squid3.red:/root/out/'+user+'-s2info.csv logdb/'+user+'-s2info-'+s0+'--'+s1+'.csv'
  print 'sgw:', cmd
  os.system(cmd)

  cmd = "ssh -q root@squid3.red '/opt/seven/bin/pcapinfo.py -u " + user + "' -p " + sec
  try:
    tmvS0 = datetime.strptime(s2, '%Y-%m-%d  %H:%M:%S.%f')
    tmvS1 = datetime.strptime(s3, '%Y-%m-%d  %H:%M:%S.%f')
    tmsS0 = tmvS0.strftime('%s.%f')
    tmsS1 = tmvS1.strftime('%s.%f')
    cmd = cmd + ' -r ' + tmsS0 + ':'+tmsS1
  except ValueError:
    print s2, s3, cmd

  #print 'sgw:', 'process pcap file'
  print cmd
  print s2, s3#, cmd
  os.system(cmd)

##copy output file from squid3.red
  s2 = s2.replace(' ', '_')
  s3 = s3.replace(' ', '_')
  cmd = 'scp -q root@squid3.red:/root/out/'+user+'-detail.txt logdb/'+user+'-detail-'+s2+'--'+s3+'.txt'
  print 'sgw:', cmd
  os.system(cmd)
  cmd = 'scp -q root@squid3.red:/root/out/'+user+'-s2info.csv logdb/'+user+'-s2info-'+s2+'--'+s3+'.csv'
  print 'sgw:', cmd
  os.system(cmd)

  zf = zipfile.ZipFile('logdb/'+user+'.zip', 'w', zipfile.zlib.DEFLATED)
  fn = 'logdb/'+user+'-detail-'+s2+'--'+s3+'.txt'
  zf.write(fn, os.path.basename(fn))
  fn = 'logdb/'+user+'-s2info-'+s2+'--'+s3+'.csv'
  zf.write(fn, os.path.basename(fn))

  fn = 'logdb/'+user+'-detail-'+s0+'--'+s1+'.txt'
  zf.write(fn, os.path.basename(fn))
  fn = 'logdb/'+user+'-s2info-'+s0+'--'+s1+'.csv'
  zf.write(fn, os.path.basename(fn))

  fn = 'logdb/'+user+'.s2info'
  zf.write(fn, os.path.basename(fn))

  zf.close()
