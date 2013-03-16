function DEBUG(msg) {
  dump("MSG: " + msg + "\n");
}

var diskinfo = new Object();
diskinfo.conn = Components.classes["@mozilla.org/tcpconn;1"].getService(Components.interfaces.nsITCPConn);

diskinfo.onDataRead = function(msgstring) {
  try {
    DEBUG("onDataRead = " + msgstring);
  } catch(ex) {
    DEBUG(ex);
  }
}

diskinfo.init =  function() {
  try {
    diskinfo.conn.openTCPConn("127.0.0.1", 10086);
    diskinfo.conn.setOnReadCallback(diskinfo.onDataRead);
    diskinfo.conn.sendMessage("This is a MSG");
  } catch(ex) {
    DEBUG(ex);
  }

  try {
    var obj = Components.classes["@mozilla.org/diskinfo;1"].getService(Components.interfaces.nsIDiskinfo);
    var pcnt = obj.getMountPointFreeInfo("/media/WinXP_D");
    DEBUG("mountPointFreeInfo.precent = " + pcnt);
  } catch(ex) {
    DEBUG(ex);
  }
  const prefB = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);

  var fontsize = prefB.getIntPref("mail.view_font_size");
  var style = "font-size: normal";
  if(fontsize==1)
    style = "font-size:large";
  else if(fontsize==2)
    style = "font-size:small";

  var elems = document.getElementsByTagName("treechildren");
  var treechild = elems.item(0);
  treechild.setAttribute("style", style);
  var treechild = elems.item(1);
  treechild.setAttribute("style", style);

  var acctMgr = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);
  var allAccounts = acctMgr.accounts;
  var cnt = allAccounts.Count();
  DEBUG("account count = " + cnt);

  var msgId = "004401ca5d33$9b1dd7e0$d15987a0$@xue@devicevm.com.cn";
  for(var idx=0; idx < cnt; idx++) {
    var acct = allAccounts.QueryElementAt(idx, Components.interfaces.nsIMsgAccount);
    var server = acct.incomingServer;
    var myPath = server.localPath;
    DEBUG("acct= " + acct + " acct.incomingServer = " + server + " path = " + myPath.path + " passwd = " + server.password);
    var msgFolder = server.rootMsgFolder;
    var msgTotal = msgFolder.getTotalMessages(true);
    //DEBUG("total number = " + msgTotal + " URI = " + msgFolder.URI + " sizeOnDisk = " + msgFolder.sizeOnDisk);

    if(msgTotal > 0) {
      try {
        var myFolder = msgFolder.getChildNamed("Inbox");
        SelectFolder(myFolder.URI);
        var msgDB = myFolder.msgDatabase;
        var myHdr = msgDB.getMsgHdrForMessageID(msgId);
        DEBUG("msgHdr.subject = " + myHdr.mime2DecodedSubject);
        //document.getElementById("tabmail").openTab("message", {folderPaneVisible: false, msgHdr: myHdr, viewWrapperToClone: gFolderDisplay.view, background:true});
      } catch(ex) {
        DEBUG(ex);
      }
    }
  }

  //this.di_timeout = setTimeout(diskinfo.update_info, 2000, false);
  //document.getElementById("diskinfo-sp1").setAttribute("value", "/dev/hda1");
  /*var canvas = document.getElementById("diskinfo-sp1");
  dump("getContext start\n");
  canvas.save();
  if(canvas.getContext) {
    dump("getContext ok\n");
    var ctx = canvas.getContext("2d");
    ctx.fillStyle = "rgb(128,0,128)";
    ctx.fillRect(0, 0, 100, 24);
  }
  canvas.restore();*/
  //setInterval('diskinfo.update_info()',3000);
}

diskinfo.load_cssfile = function(filename) {
  try {
    var css=document.createElementNS('http://www.w3.org/1999/xhtml', 'link');
    css.setAttribute('rel', 'stylesheet');
    css.setAttribute('href', "chrome://diskinfo/skin/" + filename);
    css.setAttribute('type', 'text/css');
    document.documentElement.appendChild(css);
    //document.documentElement.insertBefore(document.documentElement.firstChild, css);
  } catch(ex) {
    DEBUG(ex);
  }
}

diskinfo.compose_newmail = function(to, subject) {
  try {
    var msgComposeType = Components.interfaces.nsIMsgCompType;
    var msgComposFormat = Components.interfaces.nsIMsgCompFormat;
    var msgComposeService = Components.classes["@mozilla.org/messengercompose;1"].getService();
    msgComposeService = msgComposeService.QueryInterface(Components.interfaces.nsIMsgComposeService);

    var params = Components.classes["@mozilla.org/messengercompose/composeparams;1"].createInstance(Components.interfaces.nsIMsgComposeParams);
    if (params) {
      params.type = msgComposeType.New;
      params.format = msgComposFormat.Default;
      var composeFields = Components.classes["@mozilla.org/messengercompose/composefields;1"].createInstance(Components.interfaces.nsIMsgCompFields);
      if (composeFields) {
        composeFields.to = to;
        composeFields.subject = subject;
        params.composeFields = composeFields;
        msgComposeService.OpenComposeWindowWithParams(null, params);
      }
    }
  } catch(ex) {
    DEBUG(ex);
  }
}

diskinfo.show_notification = function(xulfile) {
  window.openDialog(xulfile, "StorageWarning", "modal,centerscreen,chrome,resizeable=no");
}

diskinfo.popup_notification = function(title, text) {
  try {
  var alertService = Components.classes['@mozilla.org/alerts-service;1'].getService(Components.interfaces.nsIAlertsService);
  alertService.showAlertNotification(null, title, text, false, '', null);
  } catch(ex) {
    DEBUG(ex);
  }
}
diskinfo.update_info = function() {
  //diskinfo.show_notification("chrome://diskinfo/content/storagewarning.xul");
  //GetFolderMessages();
  //folders = GetSelectedMsgFolders();
  //DEBUG("folder.len = " + folders.length + "obj = " + folders);
  //clearTimeout(this.di_timeout);
  //this.di_timeout = setTimeout(diskinfo.update_info, 1000, false);*/
}

window.addEventListener("load", diskinfo.init, false);
