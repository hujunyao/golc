#!/usr/bin/python2.6
# -*- coding: utf_8 -*-
import getopt
import sys
import re

usage="""
  logtool usage:
		-s search string 
		-l list csm, htrx
		-i input logcat file
	logtool -s facebook -l csm=000E809F htrx=000E00E4 -i logcat.log
"""

clq_re = '[A-Z]+\ +\[[A-Z0-9]+'
clq_split_re = '\ +\['

class csm:
	def __init__(self):
		self.id=''
		self.htrx=[]

	def setid(self, id):
		self.id = id

	def sethtrx(self, htrxid):
		self.htrx.append(htrxid)

def log_list(sstr, lstr, fp):
	csmarray=[]
	csmv = ''
	htrxv = ''
	print sstr, lstr

	la = lstr.split(' ')
	for item in la:
		t,v = item.split('=')
		if t == 'csm':
			if v == '':
				csmv='ALL'
			else:
				csmv = v
		elif t == 'htrx':
			if v == '':
				htrxv='ALL'
			else:
				htrxv = v

	for line in fp:
		clq = line.find('Constructed HTTP CLQ from ')
		if clq != -1:
			clqinfo = re.findall(clq_re, line)
			obj = csm()
			for info in clqinfo:
				tag, key = re.split(clq_split_re, info)
				if tag == 'HTRX':
					htrxkey = key 
				elif tag == 'CSM':
					csmkey = key
			obj.setid(csmkey)
			obj.sethtrx(htrxkey)
			print obj.id, obj.htrx
			break
			
if __name__ == '__main__':
	lstr = ''
	sstr = ''
	logfile = ''

	try:
		optlist, args = getopt.gnu_getopt(sys.argv[1:], 's:i:l:h')
	except getopt.GetoptError, err:
		print usage
		sys.exit(1)

	for o, a in optlist:
		if o == '-h':
			print usage
		if o == '-s':
			sstr = a
		elif o == '-l':
			lstr = a
		elif o == '-i':
			logfile = a

	fp = open(logfile)
	try:
		if lstr != '':
			log_list(sstr, lstr, fp)
	finally:
		fp.close()
