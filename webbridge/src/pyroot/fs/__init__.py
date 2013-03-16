class webapp:
  def __init__(self, path):
    self.path = path
    print "generate dirlist ", path
    self.title = "File System Browser"
    pass

  def run(self):
    return "<html><title>"+self.title+"</title><body>root = "+self.path+"\nThis is demo of webbridge</body></html>"
