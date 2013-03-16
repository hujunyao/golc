function DEBUG(msg) {
	dump("MSG: " + msg + "\n");
}

mikanShareObject.prototype.doShare = function() {
	DEBUG("mikanShare.doShare");
	try {
		var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"].getService(Components.interfaces.nsIWindowMediator);
		var mainWindow = wm.getMostRecentWindow("navigator:browser");
		var uri = mainWindow.gBrowser.currentURI;
		DEBUG("uri = " + uri.spec);
		var shareurl = "http://service.weibo.com/share/share.php?url=";
		var weibourl = shareurl + uri.spec + "&title="+mainWindow.gBrowser.contentTitle+"&ralateUid=2792309554";
		mainWindow.gBrowser.selectedTab = mainWindow.gBrowser.addTab(weibourl);

		window.close();
	} catch(ex) {
		DEBUG("ex = " + ex);
	}
}

mikanShareObject.prototype.init = function() {
	try {
	} catch(ex) {
		DEBUG("ex = " + ex);
	}
}

mikanShareObject.prototype.term = function() {
	DEBUG("mikanLogin.term");
}

var mkShare = new mikanShareObject();

window.addEventListener("load", mkShare.init, false);
window.addEventListener("unload", mkShare.term, false);
