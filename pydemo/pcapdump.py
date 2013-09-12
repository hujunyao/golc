#!/usr/bin/python2.7
# -*- coding: utf_8 -*-
import getopt
import sys
import os
import pyshark
import glob
import time
from datetime import datetime

class pcapDB():
  def __init__(self):
    self.tm = 0
    self.got = False
    self.pcapinfo = []

  def setTM(self, tm):
    self.tm = tm

  def dump(self):
    sorted_pcapinfo = sorted(self.pcapinfo, key=lambda tm: tm[0])
    for pi in sorted_pcapinfo:
      print pi

  def loadpcap(self, pcapfile, field, exp):
    if self.got:
      return

    try:
      pcapinfo = pyshark.read(pcapfile, field, exp)
    except pyshark.PySharkError:
      return

    sta = 0
    stm = 0
    etm = 0
    for pkt in pcapinfo:
      tm = pkt['frame.time_epoch']
      if sta == 0:
        stm = tm
        sta = 1
      else:
        etm = tm

    stm = datetime.utcfromtimestamp(stm)
    stm = stm.strftime('%Y-%m-%d  %H:%M:%S.%f')
    etm = datetime.utcfromtimestamp(etm)
    etm = etm.strftime('%Y-%m-%d  %H:%M:%S.%f')

#    print stm, etm, pcapfile
    self.pcapinfo.append([stm, etm, pcapfile])

if __name__ == '__main__':
  pcapfield = []
  pcapfield.append('frame.time_epoch')
  pcapfield.append('frame.number')
  exp = 'sll.etype != 0x886c'

  os.environ['TZ'] = 'GMT'
  #print tms
  obj = pcapDB()
  obj.setTM(0.0)
  pcaplogs = glob.glob('*.pcap')
  for log in pcaplogs:
    #print log
    obj.loadpcap(log, pcapfield, exp)

  obj.dump()
