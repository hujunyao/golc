#!/usr/bin/python2.7
# -*- coding: utf_8 -*-
import getopt
import sys
import os

if __name__ == '__main__':
  ssh_cmd = ['ssh -q voyeur@ap4.z.red', 'ssh -q voyeur@ap5.z.red']
  rcmd = '/home/voyeur/bin/fetchcrcs.py'
  user = ''
  crcsuser = ''
  sec = '5'

  os.system('mkdir -p logdb')
  os.system('rm -rf logdb/*.sqlite')
  os.system('rm -rf logdb/*.crcs')
  os.system('rm -rf logdb/*.pcap')

#convert IMEI userid to CRCS userid
  try:
    optlist, args = getopt.gnu_getopt(sys.argv[1:], 'p:u:ahftd')
  except getopt.GetoptError, err:
    print err
    sys.exit(1)
  for o, a in optlist:
    if o == '-u':
      user = a
      crcsuser = str(hex(int(user))).strip('0x')
    elif o == '-p':
      sec = a

  s = ''
  for arg in sys.argv[1:]:
     s = s + ' ' + arg
  #print s

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

  #if s != '' and s.find('-h') < 0 :
  #  realcmd = 'scp -q root@squid3.red:/tmp/logs.zip /tmp/logs.zip'
  #  print 'sgw:', realcmd
  #  os.system(realcmd)

##uncompress logfile to logdb
  print 'sgw:', 'uncompress all crcslog to logdb'
  os.system('unzip -q -d logdb /tmp/crcs-ap4.zip')
  os.system('unzip -q -d logdb /tmp/crcs-ap5.zip')
  #os.system('unzip -q -d logdb /tmp/logs.zip')

##convert crcslog to sqlite3 db
  print 'sgw:', 'convert all crcslog to db'
  os.system('./crcs2db.py -u ' + user)
  os.system('scp -q logdb/'+user+'.sqlite root@squid3.red:/root/logdb/')

##parse all pcap file 
  print 'sgw:', 'process pcap file'
  os.system("ssh -q root@squid3.red '/opt/seven/bin/pcapinfo.py -u " + user + "' -p " + sec)
