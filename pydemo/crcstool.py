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
md5_re = '[A-Z:]+\ +\[[A-Z0-9]+'
clq_split_re = '\ +\['
key_re = '[A-Za-z:]+\ +\[[A-Z0-9]+'
clqmatch = 'Constructed HTTP CLQ from '

class htrx:
	def __init__(self, line, fp):
		self.csm = ''
		self.reqMD5 = ''
		self.respMD5 = ''
		self.reqkey = ''
		self.url0 = ''
		self.url1 = ''
		self.id = ''
		self.reqh = ''
		self.currline = ''
		self.ignore = False
		self.parse_clq_line(line)
		self.parse_req_header(fp)

	def parse_clq_line(self, line):
		clqinfo = re.findall(clq_re, line)
		for item in clqinfo:
			tag, key = re.split(clq_split_re, item)
			if tag == 'HTRX':
				self.id = key
			elif tag == 'CSM':
				self.csm = key

	def set_ignore(self):
		self.ignore = True

	def parse_req_header(self, fp):
		eofmatch = '[EOF]'
		for line in fp:
			if line.find(clqmatch) != -1:
				self.currline = line
				break
			elif line.find(eofmatch) != -1:
				break
			elif line.find('.cpp:') != -1:
				break
			else:
				self.reqh += line

	def set_cacheable_key(self, key, fp):
		self.reqkey = key
		line = fp.next()
		if line.find('Normalized request headers and body') != -1:
			md5info = re.findall(md5_re, line)
			tag, md5 = re.split(clq_split_re, md5info[0])
			self.reqMD5 = md5
			#print line, self.reqkey, self.id, self.reqMD5
			#sys.exit(1)

	def setid(self, id):
		self.id = id

	def setcsm(self,csmid):
		self.csm = csmid

	def setrespMD5(self, md5):
		self.respMD5 = md5

	def seturl0(self, url):
		self.url0 = url

	def seturl1(self, url):
		self.url1 = url

def compare_htrx_respMD5_reqkey(obj ,tmpobj):
	if obj.respMD5 == tmpobj.respMD5:
		if obj.reqkey != tmpobj.reqkey:
			return 0
	return 1

def compare_htrx_respMD5_reqMD5(obj, tmpobj):
	if obj.respMD5 == tmpobj.respMD5:
		if obj.reqMD5 != tmpobj.reqMD5:
			return 0
	return 1

def get_real_url(fakeurl):
	tmp = fakeurl[69:]
	idx = tmp.find('/')
	url = 'http://' + tmp[0:idx].replace('_', '.') + tmp[idx:]
	return url

def print_url(htrxdict):
	myhtrxdict = sorted(htrxdict.items(), key=lambda d:htrxdict[d[0]].url1)
	for item in myhtrxdict:
		obj = item[1]
		if obj.url1.find(sstr) != -1:
			print 'HTRX: %s, URL: %s' %(obj.id, obj.url1)

	print 'total %d htrx obj ' %len(myhtrxdict)

def log_list(sstr, lstr, fp):
	htrxdict = {}
	htrxarray = []
	csmv = ''
	htrxv = ''

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

	keymatch = 'adding to the cacheable set, key'
	respmatch = 'Constructed HTTP CSN from'
	normatch = 'Final normalized URL'

	match = clqmatch
	for line in fp:
		clq = line.find(match)
		if clq != -1:
			obj = htrx(line, fp)
			htrxdict[obj.id] = obj

			if obj.currline != '':
				obj = htrx(obj.currline, fp)
				htrxdict[obj.id] = obj
			#print obj.id, obj.csm
		elif line.find(keymatch) !=-1:
			keyinfo = re.findall(key_re, line)
			tag, htrxid  = re.split(clq_split_re, keyinfo[0])
			tag, key = re.split(clq_split_re, keyinfo[1])
			#print line, htrxid, key
			htrxdict[htrxid].set_cacheable_key(key, fp)
		elif line.find(respmatch) != -1:
			keyinfo = re.findall(key_re, line)
			tag, htrxid  = re.split(clq_split_re, keyinfo[0])
			tag, md5= re.split(clq_split_re, keyinfo[1])
			#print line, htrxid, md5
			htrxdict[htrxid].setrespMD5(md5)
		elif line.find(normatch) != -1:
			keyinfo = re.findall('[A-Z]+\ +\[[^\]]+', line)
			tag, htrxid = re.split(clq_split_re, keyinfo[0])
			tag, url = re.split(clq_split_re, keyinfo[1])
			htrxdict[htrxid].seturl1(get_real_url(url))
			#print line, htrxid, url

##print simalar url
#	print_url(htrxdict)
#	sys.exit(1)

##compare respMD5 and reqkey or reqMD5
	for id in htrxdict:
		obj = htrxdict[id]
		if obj.respMD5 == '':
			obj.set_ignore()
		elif obj.url1.find(sstr) == -1:
			obj.set_ignore()

	i = 0
	for id in htrxdict:
		obj = htrxdict[id]
		if obj.ignore:
			i = i+1
			continue

		prefix=True
		j = i+1
		for tmpid in htrxdict:
			if j == 0:
				tmpobj = htrxdict[tmpid]
				if tmpobj.ignore:
					continue
#				if compare_htrx_respMD5_reqMD5(obj, tmpobj) == 0:
				if compare_htrx_respMD5_reqkey(obj, tmpobj) == 0:
					tmpobj.set_ignore()
					if prefix:
						print '\n---HTRX:%s, HTRX.respMD5:%s HTRX.reqkey:%s' %(obj.id, obj.respMD5, obj.reqkey)
						print '---URL: %s' %(obj.url1)
						prefix=False
					print '***HTRX:%s  HTRX.respMD5:%s HTRX.reqkey:%s' %(tmpobj.id, tmpobj.respMD5, tmpobj.reqkey)
					print '***URL: %s' %(tmpobj.url1)
			else:
				j = j-1
		i = i+1

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
