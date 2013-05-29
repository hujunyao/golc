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
import copy

usage = """
          -h display help message
        """

def do_merge(infofd, sorted_tminfo, s0, s1):
  merged_tminfo = []
  idx = 0
  cnt = len(sorted_tminfo) - 1
#  sorted_tminfo[0][0] = 0.0
#  sorted_tminfo[cnt][1] = 0.0

  if len(sorted_tminfo) <= 0:
    merged_tminfo.append([s0, s1])
    return merged_tminfo

  for tm in sorted_tminfo:
    if idx >= cnt:
      break
    ntm = sorted_tminfo[idx+1]
    r = ntm[0] - tm[1]
    if r < 60:
      tm[1] = 0.0
      ntm[0] = 0.0
      #if r < 0:
      #  print 'pcapinfo merge may error'
    idx = idx + 1
    
  #print sorted_tminfo
  stm = 0.0
  etm = 0.0
  is_append = False
  if s0 < sorted_tminfo[0][0]:
    #print 'append', s0, sorted_tminfo[0][0]
    merged_tminfo.append([s0, sorted_tminfo[0][0]])

  for tm in sorted_tminfo:
    #print tm
    if tm[1] > 0.0:
      stm = tm[1]
      is_append = True
    elif is_append and tm[0] > 0.0:
      etm = tm[0]
      #print 'append', stm, etm
      merged_tminfo.append([stm, etm])
      is_append = False

  if sorted_tminfo[cnt][1] < s1:
    #print 'append', sorted_tminfo[cnt][1], s1
    merged_tminfo.append([sorted_tminfo[cnt][1], s1])

#  print 'merged'
#  print merged_tminfo
  return merged_tminfo

def chk_date_range2(tm, tmarray):
  low = 0
  is_s2 = False
  ishit = False
  high = len(tmarray) - 1
  mid = -1
  while low <= high:
    try:
      mid = (low + high)//2
      midtm = tmarray[mid]
      if midtm[7] == 'S2':
        if midtm[1] < tm:
          low = mid + 1
        elif midtm[0] > tm:
          high = mid - 1
        else:
          ishit = True
          break
      else:
        if midtm[1] <= tm:
          low = mid + 1
        elif midtm[0] >= tm:
          high = mid - 1
        else:
          ishit = True
          break
    except IndexError:
      print mid

  if ishit and  tmarray[mid][7] == 'S2':
    is_s2 = True
  elif ishit and tmarray[mid][7] == '--':
    is_s2 = False

  return ishit,mid,is_s2

def chk_date_range(tm, tmarray):
  low = 0
  ishit = False
  high = len(tmarray) - 1
  mid = -1
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
    self.soinfo = []
    self.pcapinfo = []
    self.pcaplostinfo = []
    self.pcaplostsec = 0.0

  def setgap(self, sec):
    self.gap = sec

#  def writepcapinfo(self, sflag, eflag, tl):
#    plistm = 0.0
#    plietm = 0.0
#    for pli in self.pcaplostinfo:
#      if stm > pli[0] and stm < pli[1]:
#        sflag = True
#      if etm > pli[0] and etm < pli[1]:
#        eflag = True
#
#    if sflag and eflag:
#      sd = datetime.utcfromtimestamp(tl[0])
#      ed = datetime.utcfromtimestamp(tl[1])
#      tl[2] = '%.3f' %(tl[1] - tl[0])
#      tl[0] = sd.strftime('%Y/%m/%d %H:%M:%S.%f')
#      tl[1] = ed.strftime('%Y/%m/%d %H:%M:%S.%f')
#      idx = idx + 1
#      wfd.writerow(tl)
#
#  def chk_pcaplost(self, stm, etm):
#    sflag = False
#    eflag = False
#
#    for pli in self.pcaplostinfo:
#      if stm > pli[0] and stm < pli[1]:
#        sflag = True
#      if etm > pli[0] and etm < pli[1]:
#        eflag = True
#
#      if sflag or eflag:
#        break
#    return sflag, eflag

  def build_final_soinfo(self, s0, s1):
    newsoinfo = []
    tmps = 0.0
    tmpe = 0.0

    for pli in self.pcaplostinfo:
      stm = pli[0]
      etm = pli[1]
      if stm <= s0:
        tmps = s0
      elif stm < s1:
        tmps = stm
      else:
        tmps = 0.0

      if etm >= s1:
        tmpe = s1
      elif etm > s0:
        tmpe = etm
      else:
        tmpe = 0.0

      if tmps > 0.0 and tmpe > 0.0:
        self.pcaplostsec = self.pcaplostsec + (tmpe - tmps)
        if tmpe <= tmps:
          print 'PCAP lost statistic ERROR', stm, etm, s0, s1

      for so in self.soinfo:
        if so[0] > stm and so[1] < etm:
          so[7] = '--'
          #print so

        if stm > so[0] and stm < so[1]:
          so[2] = stm
        if etm > so[0] and etm < so[1]:
          so[3] = etm
          break

    for so in self.soinfo:
      #print 'old.soinfo', so
      if so[2] == 0.0 and so[3] == 0.0:
        newsoinfo.append(so)
      elif so[2] > 0.0 and so[3] > 0.0:
        #print '2 and 3 old.soinfo', so
        tmpso = copy.copy(so)
        tmpso[1] = so[2]
        tmpso[2] = 0.0
        tmpso[3] = 0.0
        newsoinfo.append(tmpso)
        tmpso = copy.copy(so)
        tmpso[0] = so[2]
        tmpso[1] = so[3]
        tmpso[2] = 0.0
        tmpso[3] = 0.0
        tmpso[7] = '--'
        #print tmpso
        newsoinfo.append(tmpso)
        tmpso = copy.copy(so)
        tmpso[0] = so[3]
        tmpso[2] = 0.0
        tmpso[3] = 0.0
        newsoinfo.append(tmpso)
      elif so[2] == 0.0:
        #print '2 old.soinfo', so
        tmpso = copy.copy(so)
        tmpso[1] = so[3]
        tmpso[2] = 0.0
        tmpso[3] = 0.0
        tmpso[7] = '--'
        #print tmpso
        newsoinfo.append(tmpso)
        #tmpso = so
        tmpso = copy.copy(so)
        tmpso[0] = so[3]
        tmpso[2] = 0.0
        tmpso[3] = 0.0
        newsoinfo.append(tmpso)
      elif so[3] == 0.0:
        #print '3 old.soinfo', so
        #tmpso = so
        tmpso = copy.copy(so)
        tmpso[1] = so[2]
        tmpso[2] = 0.0
        tmpso[3] = 0.0
        newsoinfo.append(tmpso)
        #tmpso = so
        tmpso = copy.copy(so)
        tmpso[0] = so[2]
        tmpso[2] = 0.0
        tmpso[3] = 0.0
        tmpso[7] = '--'
        #print tmpso
        newsoinfo.append(tmpso)

    return newsoinfo

  def build_soinfo2(self, dS0, dS1):
    news2info = []
    idx = 0
    cnt = len(s2info) - 1
    for s2 in s2info:
      news2info.append(s2)
      if idx < cnt:
        news2info.append([s2[1], s2info[idx+1][0], 0, 0, 0, 0, 0, 'Sx', 0])
      idx = idx + 1

    isS0Flag = False
    isS1Flag = False
    cnt = len(news2info) - 1
    for s2 in news2info:
      if dS0 > s2[0] and dS0 < s2[1]:
        s2[2] = dS0
        isS0Flag = True
      if dS1 > s2[0] and dS1 < s2[1]:
        s2[3] = dS1
        isS1Flag = True

    #print isS0Flag, isS1Flag, news2info
    start_append = False

    if isS0Flag == False:
      if dS0 < news2info[0][0] and dS1 > news2info[0][0]:
        self.soinfo.append([dS0, news2info[0][0], 0, 0, 0, 0, 0, 'Sx', 0])
        start_append = True
      elif dS0 > news2info[cnt][1]:
        self.soinfo.append([dS0, dS1, 0, 0, 0, 0, 0, 'Sx', 0])

    for s2 in news2info:
      if s2[2] > 0.0 and s2[3] > 0.0:
        self.soinfo.append([s2[2], s2[3], 0, 0, 0, 0, 0, 'S2', 0])
        break
      elif s2[2] > 0.0:
        self.soinfo.append([s2[2], s2[1], 0, 0, 0, 0, 0, s2[7], 0])
        start_append = True
      elif s2[3] > 0.0:
        self.soinfo.append([s2[0], s2[3], 0, 0, 0, 0, 0, s2[7], 0])
        break
      else:
        if start_append:
          self.soinfo.append(s2)

    if isS1Flag == False:
      if dS1 < news2info[0][0]:
        self.soinfo.append([dS0, dS1, 0, 0, 0, 0, 0, 'Sx', 0])
      elif dS0 < news2info[cnt][1]:
        self.soinfo.append([news2info[cnt][1], dS1, 0, 0, 0, 0, 0, 'Sx', 0])
    #print self.soinfo

  def build_soinfo(self, dS0, dS1):
    idx = 0
    cnt = len(s2info) - 1
    is_s2 = False
    news2info = []
    ishit = False

    for s2 in s2info:
      #print s2
      if s2[1] < dS0:
        continue
      elif s2[0] > dS1:
        break
      else:
        news2info.append(s2)

    #print news2info
    cnt = len(news2info) - 1
    idx = 0
    if cnt < 0:
      self.soinfo.append([dS0, dS1, 0, 0, 0, 0, 0, 'Sx', 0])
      return

    if dS0 < news2info[0][0]:
      self.soinfo.append([dS0, news2info[0][0], 0, 0, 0, 0, 0, 'Sx', 0])
    else:
      self.soinfo.append([dS0, news2info[0][1], 0, 0, 0, 0, 0, 'S2', 0])
      idx = 1
      is_s2 = True

    while idx < cnt:
      if is_s2:
        self.soinfo.append([news2info[idx-1][1], news2info[idx][0], 0, 0, 0, 0, 0, 'Sx', 0])
        self.soinfo.append([news2info[idx][0], news2info[idx][1], 0, 0, 0, 0, 0, 'S2', 0])
      else:
        self.soinfo.append([news2info[idx][0], news2info[idx][1], 0, 0, 0, 0, 0, 'S2', 0])
        self.soinfo.append([news2info[idx][1], news2info[idx+1][0], 0, 0, 0, 0, 0, 'Sx', 0])

      idx = idx +1

    if dS1 > news2info[cnt][1]:
      if is_s2:#dS1 > news2info[cnt][1]:
        self.soinfo.append([news2info[cnt-1][1], news2info[cnt][0], 0, 0, 0, 0, 0, 'Sx', 0])
        self.soinfo.append([news2info[cnt][0], news2info[cnt][1], 0, 0, 0, 0, 0, 'S2', 0])
        self.soinfo.append([news2info[cnt][1], dS1, 0, 0, 0, 0, 0, 'Sx', 0])
      else:
        self.soinfo.append([news2info[cnt][0], news2info[cnt][1], 0, 0, 0, 0, 0, 'S2', 0])
        self.soinfo.append([news2info[cnt][1], dS1, 0, 0, 0, 0, 0, 'Sx', 0])
    else:
      self.soinfo.append([news2info[cnt][0], dS1, 0, 0, 0, 0, 0, 'S2', 0])
    #print len(s2info), len(self.soinfo)
    #print s2info
    #print '***'
    #print self.soinfo
    #sys.exit(1)

  def loadpcap(self, pcapfile, field, exp):
    self.tm = float(0.0)
    try:
      pcapinfo = pyshark.read(pcapfile, field, exp)
    except pyshark.PySharkError:
      return
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
    self.pcapinfo.append([stm, self.tm, pcapfile])
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

    sorted_tminfo =sorted(self.pcapinfo, key=lambda tm: tm[0])

    for pi in sorted_tminfo:
      d0 = datetime.utcfromtimestamp(pi[0])
      d0 = d0.strftime('%Y/%m/%d %H:%M:%S.%f')
      d1 = datetime.utcfromtimestamp(pi[1])
      d1 = d1.strftime('%Y/%m/%d %H:%M:%S.%f')
      pidetail = d0 + ' -- ' + d1 + ' * ' +str(pi) + '\n'
      infofd.write(pidetail)

    self.pcaplostinfo = do_merge(infofd, sorted_tminfo, s0, s1)
#    for pli in self.pcaplostinfo:
#      print pli
#      infofd.write(str(pli))
#    infofd.write()
#    print 'newsoinfo'
    self.soinfo = self.build_final_soinfo(s0, s1)
    print 'PCAP lost statistic: %.2f%%' %(100*(self.pcaplostsec/(s1-s0)))
    #for so in self.soinfo:
    #  print so
    #sys.exit(1)
    c = self.conn.cursor()
    for o in self.radio_up:
      tm = o[0]['frame.time_epoch']
      is_wifi = False
      is_s2 = False
      is_hit = False
      try:
        proto = o[0]['ip.proto']
      except KeyError:
        proto = 0

      if proto != 6:
        continue

      if s0 != 0 and s1 != 0 and (tm < s0 or tm > s1):
        continue
      is_wifi,idx = chk_date_range(tm, netinfo)
      #for ni in netinfo:
      #  if tm >= ni[0] and tm <= ni[1]:
      #    is_wifi = True
      #    break
      if is_wifi:
        continue

      is_hit,idx,is_s2 = chk_date_range2(tm, self.soinfo)
      if is_hit == False:
#        print 'TCPHIT.ERROR', o[0]
        d = datetime.utcfromtimestamp(tm)
        print d.strftime('%Y/%m/%d %H:%M:%S.%f'), 'TCPHIT.ERROR', o[0]
      #  is_so, soidx = chk_date_range2(tm, self.soinfo)
      #  #print is_so, soidx, tm
      #  if is_so == False:
      #    print 'ERROR', o[0]
      #for s2obj in s2info:
      #  if tm >= s2obj[0] and tm <= s2obj[1]:
      #    is_s2 = True
      #    break

      d = datetime.utcfromtimestamp(tm)
      #o[0]['frame.time_epoch'] = o[0]['frame.time_epoch'].strftime('%Y/%m/%d %H:%M:%S.%f')
      #try:
      #  proto = o[0]['ip.proto']
      #except KeyError:
      #  proto = 0

      if o[1] != self.logfile:
        infofd.write(o[1] + '\n')
        self.logfile = o[1]
      if proto == 6:
        if is_hit:
          idxinfo = ' ' + self.soinfo[idx][7] +'T'+ str(idx) + '\n'
          infofd.write(str(o[0]) + idxinfo)
        #else:
        #  infofd.write(str(o[0]) + '\n')
        dstport = o[0]['tcp.dstport']
        srcport = o[0]['tcp.srcport']
        o[0]['frame.time_epoch'] = d.strftime('%Y/%m/%d %H:%M:%S.%f')
        cnt = cnt + 1
        if is_s2:
          s2cnt = s2cnt + 1

        self.soinfo[idx][3] = self.soinfo[idx][3] + 1
        #else:
        #  if is_so:
        #    #print soidx, len(self.soinfo), self.soinfo[soidx]
        #    self.soinfo[soidx][3] = self.soinfo[soidx][3] + 1
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
          self.soinfo[idx][4] = self.soinfo[idx][4] + 1
          #else:
          #  if is_so:
          #    self.soinfo[soidx][4] = self.soinfo[soidx][4] + 1
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
      is_hit = False
      try:
        proto = o[0]['ip.proto']
      except KeyError:
        proto = 0

      if proto != 17:
        continue

      if s0 != 0 and s1 != 0 and (tm < s0 or tm > s1):
        continue

      is_wifi,idx = chk_date_range(tm, netinfo)
      #for ni in netinfo:
      #  if tm >= ni[0] and tm <= ni[1]:
      #    is_wifi = True
      #    break
      if is_wifi:
        continue

      is_hit,idx,is_s2 = chk_date_range2(tm, self.soinfo)
      if is_hit == False:
#        print 'UDPHIT.ERROR', o[0]
        d = datetime.utcfromtimestamp(tm)
        print d.strftime('%Y/%m/%d %H:%M:%S.%f'), 'UDPPHIT.ERROR', o[0]
      #  is_so, soidx = chk_date_range2(tm, self.soinfo)
      #  if is_so == False:
      #    print 'ERROR', o[0]
      #  #else:
      #  #  print 'OK', o[0]
      #for s2obj in s2info:
      #  if tm >= s2obj[0] and tm <= s2obj[1]:
      #    is_s2 = True
      #    break

      #try:
      #  proto = o[0]['ip.proto']
      #except KeyError:
      #  proto = 0
      #o[0] = datetime.utcfromtimestamp(o[0]).strftime('%Y/%m/%d %H:%M:%S.%f')
      #o[3] = datetime.utcfromtimestamp(o[3]).strftime('%Y/%m/%d %H:%M:%S.%f')
      if o[1] != self.logfile:
        infofd.write(o[1] + '\n')
        self.logfile = o[1]

      if proto == 17:
        if is_hit:
          idxinfo = ' ' + self.soinfo[idx][7] +'U'+ str(idx) + '\n'
          infofd.write(str(o[0]) + idxinfo)

        cnt = cnt + 1
        if is_s2:
          s2cnt = s2cnt + 1
        self.soinfo[idx][5] = self.soinfo[idx][5] + 1
        #else:
        #  if is_so:
        #    self.soinfo[soidx][5] = self.soinfo[soidx][5] + 1

        if o[0]['udp.dstport'] == 53:
          dnscnt = dnscnt + 1
          if is_s2:
            dnss2cnt = dnss2cnt + 1
          self.soinfo[idx][6] = self.soinfo[idx][6] + 1
          #else:
          #  if is_so:
          #    self.soinfo[soidx][6] = self.soinfo[soidx][6] + 1
    detail = 'UDP radio up: ' +str(s2cnt)+'/'+ str(cnt) + ' DNS radio up: '+str(dnss2cnt)+'/' + str(dnscnt)
    print detail
    infofd.write(detail + '\n')
    infofd.close()

  def out_s2info_csv(self):
    csvfd = open('out/'+self.user+'-s2info.csv', 'wb')
    wfd = csv.writer(csvfd)
    wfd.writerow(['SDATE', 'EDATE', 'TIME', 'TCP radio up', 'APP radio up', 'UDP radio up', 'DNS radio up', 'Flag', 'IDX'])
    idx = 0
    for so in self.soinfo:
      tl = so
      tl[8] = idx
      #sflag, eflag = chk_pcaplost(tl[0], tl[1])
      #if sflag == False and eflag == False:
      sd = datetime.utcfromtimestamp(tl[0])
      ed = datetime.utcfromtimestamp(tl[1])
      tl[2] = '%.3f' %(tl[1] - tl[0])
      tl[0] = sd.strftime('%Y/%m/%d %H:%M:%S.%f')
      tl[1] = ed.strftime('%Y/%m/%d %H:%M:%S.%f')
      idx = idx + 1
      wfd.writerow(tl)
      #else:
      #  writepcapinfo(wfd, sflag, eflag, tl)
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
  obj.build_soinfo2(dS0, dS1)
  obj.setgap(sec)
  #logs = glob.glob('logs/*.pcap')
  logs = glob.glob('logs/*.pcap')
  #print logs, pf
  for log in logs:
    #print log
    obj.loadpcap(log, pcapfield, exp)

  obj.dump(dS0, dS1)
  obj.out_s2info_csv()
