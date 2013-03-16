import glob
import os
import sys
import shutil

extarray = ['.po']

def visitor(args, dir, filearray):
  for file in filearray:
    fpath = os.path.join(dir, file)
    if not os.path.isdir(fpath):
      if os.path.splitext(fpath)[1] in extarray:
        shutil.copyfile(fpath, "po/"+os.path.split(fpath)[1])

def searchdir():
  os.mkdir('po')
  os.path.walk("./LC_MESSAGES/", visitor, None)

def mcp():
  basepath = "po/"
  files = glob.glob('*/*/*.po')
  for file in files:
    shutil.copyfile(file, basepath+file)

if __name__ == '__main__':
  searchdir()
