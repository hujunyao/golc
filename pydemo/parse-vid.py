#!/usr/bin/python2.6
# -*- coding: utf_8 -*
import os
import sys
import httplib
import json
import time
import random
import math
import xml.dom.minidom

def parse_ku6_vid():
  conn = httplib.HTTPConnection("v.ku6.com")
  #conn.request("GET", "/fetch.htm?t=getVideo4Player&vid=kYkvXM4OyXRMHNNSGua9tQ..")
  #conn.request("GET", "/fetch.htm?t=getVideo4Player&vid=B3mZSV975EBGFMdM")
  conn.request("GET", "/fetch.htm?t=getVideo4Player&vid=XiVfqNCX6WdbZiEQ")
  resp = conn.getresponse()
  print resp.status, resp.reason
  data = resp.read()
  print data
  vs = json.loads(data)
  print vs['data']['ad'] + "\n"
  print vs['data']['bigpicpath'] + "\n"
  files = vs['data']['f'].split(',')
  for f in files:
    print f
  conn.close()
  
def genSID():
  nowTime = int(time.time()*1000)
  r1 = random.randint(1000,1998)
  r2 = random.randint(1000,9999)
  return "%d%d%d" %(nowTime, r1, r2)

def genKey(key1, key2):
  k = int(key1, 16)
  k = k ^ 0xA55AA5A
  return key2+str(k)

def getFileIDString(seed):
  mixed=[]
  source=list("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ/\\:._-1234567890")
  myseed=float(seed)

  for i in range(len(source)):
    myseed = (myseed * 211 + 30031) % 65536
    idx = math.floor((myseed/65536)*len(source))
    idx = int(idx)
    mixed.append(source[idx])
    source.remove(source[idx])

  return mixed

def genFileID(fileid, seed):
  #print "fileid = " + fileid + " seed = " + seed
  mixed = getFileIDString(seed)
  ids = fileid.split('*')
  fidstr = ''
  for ch in ids:
    if ch != '':
      fidstr = fidstr + mixed[int(ch, 10)]

  return fidstr

def get_mp4_path(mp4data, idx, fileid, sid, key):
  prefix_sid = "http://f.youku.com/player/getFlvPath/sid/"
  prefix_fid = "/st/mp4/fileid/"
#03000813084CE54FC6EA5E02996C2428A5DDD0-D7C9-1FC9-BE2A-F271B8D73835?start=240&K=842e597e2bee562e28276a14&hd=1&myp=0&ts=392&ypremium=1"
  if idx < 10:
    path = prefix_sid + sid + "_0" + str(idx) + prefix_fid + fileid + "?K=" + key
  else:
    path = prefix_sid + sid + "_" + str(idx) + prefix_fid + fileid + "?K=" + key

  print path

def get_flv_path(flvdata, idx, fileid, sid, key):
  prefix_sid = "http://f.youku.com/player/getFlvPath/sid/"
  prefix_fid = "/st/flv/fileid/"

  if idx < 10:
    path = prefix_sid + sid + "_0" + str(idx) + prefix_fid + fileid + "?K=" + key
  else:
    path = prefix_sid + sid + "_" + str(idx) + prefix_fid + fileid + "?K=" + key

  print path

def get_hd2_path(hd2data, idx, fileid, sid, key):
  prefix_sid = "http://f.youku.com/player/getFlvPath/sid/"
  prefix_fid = "/st/flv/fileid/"

  if idx < 10:
    path = prefix_sid + sid + "_0" + str(idx) + prefix_fid + fileid + "?K=" + key
  else:
    path = prefix_sid + sid + "_" + str(idx) + prefix_fid + fileid + "?K=" + key

  print path

def do_parse_mp4_files(mp4_vdata, fileid, sid, key):
  #print mp4_vdata
  idx = 0
  for mp4 in mp4_vdata:
    key = mp4['k']
    if key == -1:
      print 'paid needed for mp4 video stream'
      continue

    val = hex(idx)
    idxstr=str(val).upper()
    if idx < 16:
      myfileid = fileid[0:9]+idxstr[2:]+fileid[10:]
    else:
      myfileid = fileid[0:8]+idxstr[2:]+fileid[10:]
    get_mp4_path(mp4, idx, myfileid, sid, key)
    idx = idx+1

def do_parse_flv_files(flv_vdata, fileid, sid, key):
  #print flv_vdata
  idx = 0
  for flv in flv_vdata:
    key = flv['k']
    if key == -1:
      print 'paid needed for flv video stream'
      continue

    val = hex(idx)
    idxstr=str(val).upper()
    if idx < 16:
      myfileid = fileid[0:9]+idxstr[2:]+fileid[10:]
    else:
      myfileid = fileid[0:8]+idxstr[2:]+fileid[10:]
    get_flv_path(flv, idx, myfileid, sid, key)
    idx = idx + 1

def do_parse_hd2_files(hd2_vdata, fileid, sid, key):
  #print flv_vdata
  idx = 0
  for hd2 in hd2_vdata:
    key = hd2['k']
    if key == -1:
      print 'paid needed for flv-HD2 video stream'
      continue

    val = hex(idx)
    idxstr=str(val).upper()
    if idx < 16:
      myfileid = fileid[0:9]+idxstr[2:]+fileid[10:]
    else:
      myfileid = fileid[0:8]+idxstr[2:]+fileid[10:]
    get_hd2_path(hd2, idx, myfileid, sid, key)
    idx = idx + 1

def parse_youku_vid(vid):
  conn = httplib.HTTPConnection('v.youku.com')
  conn.request('GET', '/player/getPlayList/VideoIDS/' + vid)
  resp = conn.getresponse()
  data = resp.read()
  #print data
  vs = json.loads(data)
  vdata = vs['data'][0]
  #print vdata["logo"] + "  " + vdata["key2"] + "\n"
  st = vdata["streamtypes"]
  #print "seed %d \n" %(vdata['seed']) 
  #print vdata['streamfileids']

  try:
    k1 = vdata['key1']
    k2 = vdata['key2']
    key = genKey(k1, k2)
  except :
    key = "0x9988776655"

  sid = genSID()
  seed = vdata['seed']

  for ft in st:
    if ft == 'mp4':
      mp4fid = vdata['streamfileids']['mp4']
      fileid = genFileID(mp4fid, seed)
      do_parse_mp4_files(vdata['segs']['mp4'], fileid, sid, key)
    if ft == 'flvhd':
      flvfid = vdata['streamfileids']['flv']
      fileid=genFileID(flvfid, seed)
      do_parse_flv_files(vdata['segs']['flv'], fileid, sid, key)
    if ft == 'hd2':
      hd2fid = vdata['streamfileids']['hd2']
      fileid=genFileID(hd2fid, seed)
      do_parse_hd2_files(vdata['segs']['hd2'], fileid, sid, key)
  #vs = json.loads(vs['data'])
  #print vs
  #print vs['data']['key1'] + '  ' + vs['data']['key2'] + '\n'
  conn.close()

def parse_sohu_vid(vid):
  url = "/vrs_flash.action?vid=" + vid + "&ver=2&bw=1669&g=8" #ver=2 normal ver=1 high ver=21 super
  conn = httplib.HTTPConnection('hot.vrs.sohu.com')
  conn.request('GET', url)
  resp = conn.getresponse()
  data = resp.read()
  print data
  vs = json.loads(data)
  prot = vs['prot']
  allot = vs['allot']
  su = vs['data']['su']
  clips = vs['data']['clipsURL']
  print 'prot=',prot, "allot=", allot, "su=", su
  idx = 0
  for clip in clips:
    s = clip.find('/tv/')
    clipfile=clip[s:]
    #print clip[s:], su[idx]
    newurl='/?prot='+str(prot)+'&file='+clipfile + '&new='+su[idx]# + '&t='+t
    #print newurl
    conn = httplib.HTTPConnection(allot)
    conn.request('GET', newurl)
    resp = conn.getresponse()
    newdata = resp.read()
    #print newdata
    mydata = newdata.split('|')
    #print mydata[0], mydata[3]
    myfile = su[idx]
    myurl=mydata[0]+myfile[1:]+"?start=0&key="+mydata[3]
    print myurl
    idx = idx+1

def _foreach_elem(data):
  print 'data: ', repr(data)

def gen_qiyi_vidurl(vidurl):
  conn = httplib.HTTPConnection('www.iqiyi.com')
  s = vidurl.find('.com')
  conn.request('GET', vidurl[s+4:])
  resp = conn.getresponse()
  data = resp.read()
  
def parse_qiyi_vid(vid, pid, ptype):
  #url = "/t.hml?tn=0.5890470100566745"
  #conn = httplib.HTTPConnection('data.video.qiyi.com')
  #conn.request('GET', url)
  #resp = conn.getresponse()
  #data = resp.read()
  #print data
  #vs=json.loads(data)
  #t = int(vs['t'])
  #print t
  #key = 2391461978 ^ t

  urlarray = []
  vidarray = []
  url = "/v/"+vid+"/"+pid+"/"+ptype + "/"
  conn = httplib.HTTPConnection('cache.video.qiyi.com')
  conn.request('GET', url)
  resp = conn.getresponse()
  data = resp.read()
  #print data
  dom = xml.dom.minidom.parseString(data)
  datas = dom.getElementsByTagName('data')
  for vid in datas:
    #print vid
    ver = vid.getAttribute('version')
    nl = vid.childNodes
    #print nl
    for n in nl:
      #ver = n.getAttribute('version')
      #print 'version = ', ver
      #print n
      if n.nodeType == vid.TEXT_NODE:
        vidarray.append(n.data) #ver=3: super ver=2 : high, ver=1: normal, ver=96: fast
        print 'version =', ver , ', vid =', n.data

  files = dom.getElementsByTagName('fileUrl')
  for myfile in files:
    nodes = myfile.getElementsByTagName('file')
    for node in nodes:
      nl = node.childNodes
      for n in nl:
        if n.nodeType == node.TEXT_NODE:
          urlarray.append(n.data)

  for url in urlarray:
    mystr = url.replace("http://", "http://127.0.0.1:8181/get?")
    print mystr
    #strarr = url.split('qiyi.com')
    #newurl = '/'+str(key) + strarr[1]
    ##print newurl
    #conn = httplib.HTTPConnection('data.video.qiyi.com')
    #conn.request('GET', newurl)
    #resp = conn.getresponse()
    #data = resp.read()
    ##print data
    #vs = json.loads(data)
    #print vs['l']
    

def parse_letv(magicstr):
  urlpre = '/vod/v1/'
  s = magicstr.find('Y24v')
  e = magicstr.find('P2I9')
  key = ""
  if e > 0:
    key = magicstr[s+4: e]
  else: 
    e = magicstr.find('j9ip')
    if e > 0:
      key = magicstr[s+4:e] + "g=="
    else:
      e = magicstr.find('/Yj0')
      key = magicstr[s+4:e] + "="
  print key
  newurl = urlpre + key + "?format=1&b=800&expect=3&host=www_letv_com"
  conn = httplib.HTTPConnection('g3.letv.cn')
  conn.request('GET', newurl)
  resp = conn.getresponse()
  data = resp.read()
  print data, '\n'
  vs = json.loads(data)
  nodelist = vs['nodelist']
  #print nodelist, '\n'
  for node in nodelist:
    print node['location']

if __name__ == '__main__':
  parse_qiyi_vid(sys.argv[1], sys.argv[2], sys.argv[3])
  #gen_qiyi_vidurl(sys.argv[1])
  #parse_sohu_vid(sys.argv[1])
  #parse_ku6_vid()
  #parse_letv(sys.argv[1])
  #if len(sys.argv) == 2:
  #  url = sys.argv[1]
  #  if url[-4:] == 'html':
  #    vid = sys.argv[1].split('id_')
  #    vid = vid[1].split('.')
  #    parse_youku_vid(vid[0])
  #  if url[-3:] == 'swf':
  #    vid = url.split('sid')
  #    vid = vid[1].split('/')
  #    vid = vid[1]
  #    parse_youku_vid(vid)
  #else:
  #  print "command usage: ./parse-vid.py http://v.youku.com/v_show/id_XMzQ2NDY1MzUy.html"
