#!/usr/bin/python2.5
# -*- coding: utf_8 -*-
def trans_main():
  import sys
  import xlrd
  import polib
  import getopt
  import os

  usage = '\ttrans.py V0.16 \nUsage: python trans.py FILE.po FILE.xls\n\t-o target: set output filename\n\t-q:\t   enable quickly SCP\n\t-h:\t   print usage'
  pofn = ''
  xlsfn = ''
  output = ''
  QSCP = False

  try:
    optlist, args = getopt.gnu_getopt(sys.argv[1:], 'o:hq')
  except getopt.GetoptError, err:
    print usage
    sys.exit(1)

  for o, a in optlist:
    if o == '-o':
      output = a
    elif o == '-h':
      print usage
    elif o == '-q':
      QSCP = True

  for arg in args:
    if arg.find('.po') !=-1:
      pofn = arg
    elif arg.find('.xls')!=-1:
      xlsfn = arg

  if xlsfn=='' or pofn=='':
    print usage
    sys.exit(1)

  if output == '':
    eidx = pofn.find('.po')
    sidx = pofn.rfind('/')
    output = pofn[sidx+1:eidx]
    print 'Auto set target to "'+output+'"\n'

  if QSCP == True:
    try:
      from ssh4py import SSH2
      import socket

      dvm_user=os.getenv('DVM_USER')
      dvm_pass=os.getenv('DVM_PASS')
      dvm_basepath=os.getenv('DVM_BASEPATH')
      dvm_host = os.getenv('DVM_HOST')
      if dvm_user == None:
        dvm_user = 'root'
      if dvm_pass == None:
        dvm_pass = '123456'

      if dvm_host==None and dvm_basepath==None:
        print 'Please export DVM_HOST & DVM_BASEPATH first.'
      elif dvm_host==None:
        print 'Please export DVM_HOST first.'
      elif dvm_basepath==None:
        print 'Please export DVM_BASEPATH first.'
      else:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((dvm_host, 22))
        sock.setblocking(True)
        ssh = SSH2.Session()
        ssh.startup(sock)
        ssh.setPassword(dvm_user, dvm_pass)

    except ImportError, err:
      print 'ssh4py module unavailale, quickly SCP was disabled.'
      QSCP = False
      pass
    except SSH2.Error, err:
      print '(username=%s, password=%s) Authentication with host="%s" failed.' %(dvm_user, dvm_pass, dvm_host)
      sock.close()
      sys.exit(1)

  wb=xlrd.open_workbook(xlsfn)
  po=polib.pofile(pofn)

  locale = ("", "", "zh_CN", "zh_TW", "ar", "de", "es", "fr", "it", "ja", "ko", "nl", "pt", "ru", "pt_BR", "sv_SE", "nb", "fi_FI", "da", "pl", "he", "hu", "sl_SI", "el", "cs_CZ", "sk", "tr", "hr")

  sh = wb.sheet_by_index(0)
  nrows = sh.nrows
  ncols = sh.ncols

  prefix = '.'#os.getenv("HOME")

  for idx in range(2, len(locale)):
    for rownum in range(4, sh.nrows):
      xls_msgid=sh.cell(rownum, 1).value.replace(' ', '').replace('\n', '').replace('\\n', '`').replace('\\"', '"')
      xls_msgstr = sh.cell(rownum, idx).value.replace('\n', '').replace('\\n', '\n').replace('\\"', '"')
  
      for entry in po:
        mystr = entry.msgid
        mystr = mystr.replace(' ', '').replace('\n', '`')
        if mystr==xls_msgid:
          entry.msgstr=xls_msgstr.encode('utf-8')
        else:
          pass 
  
    path = prefix+"/"+locale[idx]+"/LC_MESSAGES/"
    if os.access(path, 0) == 0:
      os.makedirs(path)
  
    path = path+output
    po.encoding="utf8"
    po.save(path+'.po')
    po.save_as_mofile(path+'.mo')

    if QSCP == True:
      exts = ['.po', '.mo']
      for ext in exts:
        mypath = path+ext
        info=os.stat(mypath)
        scppath = dvm_basepath+mypath[1:]
        try:
          channel = ssh.SCPPut(scppath, 0x1FF&info.st_mode, info.st_size)
        except SSH2.Error, err:
          print 'scp %s[%s] file to %s@%s:%s failed.' %(ext[1:], locale[idx], dvm_user, dvm_host, scppath)
          continue
        file = open(mypath)
        content = file.read(info.st_size)
        file.close()
        channel.write(content)
        channel.close()

  if QSCP == True:
    print '\nSCP all translated files to %s@%s:%s successfully.' %(dvm_user, dvm_host, dvm_basepath)
    ssh.close()
    sock.close()

if __name__ == '__main__':
  trans_main()
