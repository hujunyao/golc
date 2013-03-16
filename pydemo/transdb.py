import glob
import os
import sys
import shutil
from pysqlite2 import dbapi2 as sqlite3
import getopt
import polib
import zipfile

usage = '''
\t\t\t transdb.py  V0.10
\t-o select translated po&mo file target directory
\t-h show help 
\t-a select transdb action mode 
\t\tinitdb: parse translated po file into sqlite3 database file.
\t\ttranslate: translate your po file from database, generate translated po&mo files in output directory.
\t\tfilter: filter your po file from database, generate untranslated po files in output directory.
\t\tupdate: update database from your translated po file for selected locale
\t\tbackup: backup database , it will be useful before you do initdb action

\ttransdb.py usage example:
\t\t./transdb.py -a initdb
\t\t./transdb.py -a translate -o output your_po_file.po
\t\t./transdb.py -a filter -o output your_po_file.po
\t\t./transdb.py -a update -l da your_translated_po_file.po
\t\t./transdb.py -a backup
'''
output = '/home/user/dev/fs/dvm/bsp/trunk/applicance/va/updater/binary/usr/share/locale/'

extarray = ['.po']

def parsepo(fpath, c):
  try:
    po = polib.pofile(fpath)
  except:
    print 'load po file %s failure!\n' %(fpath)
    return

  for entry in po:
    if entry.msgstr != '': 
      t = (entry.msgid, entry.msgstr)
      c.execute("insert into transdb values (?, ?)", t)

def visitor(args, dir, filearray):
  locale, action, conn = args

  for file in filearray:
    fpath = os.path.join(dir, file)
    if not os.path.isdir(fpath):
      if os.path.splitext(fpath)[1] in extarray:
        c = conn.cursor()
        parsepo(fpath, c)
        conn.commit()
        c.close()

def do_initdb(locale, conn):
  c = conn.cursor()
  c.execute('''create table transdb (orig text, trans text)''')
  conn.commit()
  c.close()
  os.path.walk(locale+"/LC_MESSAGES/", visitor, (locale, action, conn))

def get_from_db(locale, msgid, c):
  str = ''
  t = (msgid, )
  c.execute('select * from transdb where orig=?', t)
  val = c.fetchone()
  if val == None:
    print 'translate "%s" for %s is unavailable' %(msgid, locale)
  else:
    str = val[1]

  return str

def do_filter(pofile, locale, conn):
  do_filter = False
  newpo = polib.POFile()
  newpo.metadata['Project-Id-Version'] = '1.0'
  newpo.metadata['Report-Msgid-Bugs-To'] = 'you@example.com'
  newpo.metadata['POT-Creation-Date'] = '2007-10-18 14:00+0100'
  newpo.metadata['PO-Revision-Date'] = '2007-10-18 14:00+0100'
  newpo.metadata['Last-Translator'] = 'you <you@example.com>'
  newpo.metadata['Language-Team'] = 'RuXu WU<wrxzzj@gmail.com>'
  newpo.metadata['MIME-Version'] = '1.0'
  newpo.metadata['Content-Type'] = 'text/plain; charset=utf-8'
  newpo.metadata['Content-Transfer-Encoding'] = '8bit'

  try:
    po = polib.pofile(pofile)
  except:
    print 'load po file %s failure!' %(pofile)

  c = conn.cursor()
  for entry in po:
    msgstr = get_from_db(locale, entry.msgid, c)
    if msgstr == '':
      do_filter = True
      newentry = polib.POEntry('', '')
      newentry.msgid = entry.msgid
      newentry.occurrences = entry.occurrences
      newpo.append(newentry)

  c.close()
  if do_filter: 
    if output[-1] == '/':
      path = output+locale+'/LC_MESSAGES/'
    else:
      path = output+'/'+locale+'/LC_MESSAGES/'
  
    if os.access(path, 0) == 0:
      os.makedirs(path)
    path += os.path.split(pofile)[1]
    newpo.encoding = 'utf8'
    newpo.save(path)

def do_update(pofile, locale):
  print 'update pofile %s for locale %s' %(pofile, locale)
  dbfile = locale+'/trans.db'
  try:
    po = polib.pofile(pofile)
  except:
    print 'load po file %s failure!' %(pofile)
    return

  conn = sqlite3.connect(dbfile)
  c = conn.cursor()
  for entry in po:
    if entry.msgstr != '': 
      t = (entry.msgid, entry.msgstr)
      c.execute("insert into transdb values (?, ?)", t)

  conn.commit()
  c.close()
  conn.close()

def do_translate(pofile, locale, conn):
  try:
    po = polib.pofile(pofile)
  except:
    print 'load po file %s failure!' %(pofile)

  c = conn.cursor()
  for entry in po:
    entry.msgstr = get_from_db(locale, entry.msgid, c)

  c.close()
  if output[-1] == '/':
    path = output+locale+'/LC_MESSAGES/'
  else:
    path = output+'/'+locale+'/LC_MESSAGES/'
  if os.access(path, 0) == 0:
    os.makedirs(path)
  path += os.path.split(pofile)[1]
  po.encoding = 'utf8'
  po.metadata['Content-Type'] = 'text/plain; charset=utf-8'
  #print 'save translated po file to %s' %(path)
  po.save(path)
  po.save_as_mofile(path[0:-2]+'mo')

if __name__ == '__main__':
  action = ''
  output = 'OUTPUT'

  try:
    optlist, args = getopt.gnu_getopt(sys.argv[1:], 'ha:o:l:')
  except getopt.GetoptError, err:
    print usage
    sys.exit(1)

  for o, a in optlist:
    if o == '-h':
      print usage
      sys.exit(1)
    elif o == '-a':
      action = a
    elif o == '-o':
      output = a
    elif o == '-l':
      locale = a

  for arg in args:
    if arg.find('.po') != -1:
      pofile = arg

  if action == '':
    print usage
  elif action == 'update':
    if locale == '' or pofile == '':
      print usage
    else:
      do_update(pofile, locale)
    sys.exit(0)
  elif action == 'backup':
    z = zipfile.ZipFile('db-backup.zip', 'w')

  files = os.listdir('./')
  for path in files:
    if path == 'output':
      continue
    if os.path.isdir(path):
      dbfile = path+'/trans.db'

      #remove dbfile when action is initdb 
      if action == 'initdb':
        if os.access(dbfile, 0):
          os.remove(dbfile)
      elif action == 'backup':
        z.write(dbfile)
        continue

      conn = sqlite3.connect(dbfile)
      if action == 'initdb':
        do_initdb(path, conn)
      elif action == 'translate':
        do_translate(pofile, path, conn)
      elif action == 'filter':
        do_filter(pofile, path, conn)
      conn.close()

  if action == 'backup':
    z.close()
