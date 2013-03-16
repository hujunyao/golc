#!/usr/bin/python2.6
# -*- coding: utf_8 -*-

import glob
import os
import sys
import shutil
import polib
import getopt
import string

usage = '\tpo2dtd.py V0.10 \nUsage: python po2dtd.py -t template.dtd -s /home/user/podir \n'
localeset = {'bg':'bg', 'cs':'cs', 'da':'da', 'de':'de', 'el':'el', 'es_ES':'es-ES', 'fi':'fi', 'fr':'fr', 'he':'he', 'hu':'hu', 'it':'it', 'ja':'ja', 'ko':'ko', 'nb':'nb-NO', 'nl':'nl', 'pl':'pl', 'pt_BR':'pt-BR', 'pt':'pt-PT', 'ro':'ro', 'ru':'ru', 'sk':'sk', 'sl':'sl', 'sr':'sr', 'sv':'sv', 'tr':'tr', 'zh_CN':'zh-CN', 'zh_TW':'zh-TW'}

def formatpo(pofile):
  f = open(pofile)
  buf = f.read()
  f.close()
  buf = buf.replace('msgid  ', 'msgid ')
  buf = buf.replace('msgid   ', 'msgid ')
  buf = buf.replace('msgstr  ', 'msgstr ')
  buf = buf.replace('msgstr   ', 'msgstr ')
  f = open(pofile, 'w')
  f.write(buf)
  f.close()

def po2dtd(pofile, locale, template):
  if os.access('locale/'+locale, 0) == 0:
    os.makedirs('locale/'+locale)

  f = open(template)
  buf = f.read()
  f.close()

  try:
    po = polib.pofile(pofile)
  except:
    print 'load po file %s failed\n' %(pofile)
    return
  for entry in po:
    if entry.msgstr != '':
      buf = buf.replace('"'+entry.msgid+'"', '"'+entry.msgstr+'"')

  f = open('locale/'+locale+'/diskinfo.dtd', 'w')
  buf = buf.encode('utf-8')
  f.write(buf)
  f.close()

if __name__ == '__main__':
  template = ''
  podir = '/home/user/temp/po'

  try:
    optlist, args = getopt.gnu_getopt(sys.argv[1:], 't:s:')
  except getopt.GetoptError, err:
    print usage
  for o, a in optlist:
    if o == '-t':
      template = a
    if o == '-s':
      podir = a

  if template == '':
    print usage
    sys.exit(1)
  else:
    for locale in localeset:
      pofile = podir+'/trans-20100528_'+locale+'.po'
      if os.access(pofile, 0):
        localeEXT = localeset[locale]
        formatpo(pofile)
        po2dtd(pofile, localeEXT, template)
      else:
        print pofile + ' is not exist'
