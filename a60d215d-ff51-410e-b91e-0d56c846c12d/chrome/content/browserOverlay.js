function DEBUG(msg) {
	dump("MSG: " + msg + "\n");
}

function get_basewindow(win) {
  var rv;
  try {
      var requestor = win.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
      var nav = requestor.getInterface(Components.interfaces.nsIWebNavigation);
      var dsti = nav.QueryInterface(Components.interfaces.nsIDocShellTreeItem);
      var owner = dsti.treeOwner;
      requestor = owner.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
      rv = requestor.getInterface(Components.interfaces.nsIXULWindow);
      rv = rv.docShell;
      rv = rv.QueryInterface(Components.interfaces.nsIDocShell);
      rv = rv.QueryInterface(Components.interfaces.nsIBaseWindow);
  } catch (ex) {
      rv = null;
  }
  return rv;
}

CheckNowObject.prototype.doCheck = function() {
	DEBUG("gCN.doCheck");
	try {
	var win = get_basewindow(window);
	var instance = Components.classes["@wuruxu.com/cnutils;1"].createInstance();
	var obj = instance.QueryInterface(Components.interfaces.nsICNUtils);
	obj.doCheck(win);
	DEBUG("obj = " + obj);
	} catch(ex) {
		DEBUG("ex = " + ex);
	}
}

CheckNowObject.prototype.init = function() {
	DEBUG("gCN.init");
}

CheckNowObject.prototype.term = function() {
	DEBUG("gCN.term");
}

var gCN = new CheckNowObject();

window.addEventListener("load", gCN.init, false);
window.addEventListener("unload", gCN.term, false);
