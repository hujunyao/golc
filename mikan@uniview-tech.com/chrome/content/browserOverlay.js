function DEBUG(msg) {
	dump("MSG: " + msg + "\n");
}

mikanObject.prototype.doCheck = function() {
	DEBUG("gMK.doCheck");
	try {
	} catch(ex) {
		DEBUG("ex = " + ex);
	}
}

mikanObject.prototype.doPlay = function() {
	DEBUG("gMK.doPlay");
	try {
		window.open("chrome://mikan/content/play.xul", "MiKan", "modal,chrome,width=320,height=220,centerscreen");
	} catch(ex) {
		DEBUG("ex = " + ex);
	}
}

mikanObject.prototype.doMiKan = function() {
	DEBUG("gMK.doMiKan");
	try {
		var elem = document.getElementById('mklogout');
		if(elem.getAttribute('hidden') == 'true') {
			window.open("chrome://mikan/content/mikan.xul", "MiKan", "modal,chrome,width=320,height=240,centerscreen");
		} else {
			var elem = document.getElementById('mklogout');
			var uid = elem.getAttribute('uid');
			var token = elem.getAttribute('token');
			var url="http://v.uniview-tech.com:8080/uvs/userInfo/media_share?username=" + uid + "&token=" + token + "&link=" + gBrowser.currentURI.spec;
		  var XMLHttpRequest = Components.Constructor("@mozilla.org/xmlextras/xmlhttprequest;1", "nsIJSXMLHttpRequest");
		  var req = new XMLHttpRequest();
		  req.mozBackgroundRequest = true;
		  req.open('GET', url);
			//DEBUG("doMiKan url =" + url);
  		req.addEventListener('load', function() {
  			try {
  			//DEBUG("resp = " + req.responseText);
  			var elem = document.getElementById('mklogout');
  			elem.setAttribute('hidden', 'true');
  			elem = document.getElementById('mkseparator');
  			elem.setAttribute('hidden', 'true');
  			elem.setAttribute('uid', '');
  			elem.setAttribute('token', '');

  			var alertService = Components.classes['@mozilla.org/alerts-service;1'].getService(Components.interfaces.nsIAlertsService);
  			var msg = gBrowser.currentURI.spec + " Saved";
  			alertService.showAlertNotification(null, "MiKan ", msg, false, '', null);
  			} catch(ex) {
  				DEBUG("doMiKan ex = " + ex);
  			}
  		}, false);
			req.send(null);
		}
	} catch(ex) {
		DEBUG("ex = " + ex);
	}
}

mikanObject.prototype.doLogout = function() {
	DEBUG("gMK.doLogout");
	try {
		//http://v.uniview-tech.com:8080/uvs/userInfo/logout?username=test;
		var url = "http://v.uniview-tech.com:8080/uvs/userInfo/logout?username=";
		var elem = document.getElementById('mklogout');
		var uid = elem.getAttribute('uid');
		url = url + uid;
		var XMLHttpRequest = Components.Constructor("@mozilla.org/xmlextras/xmlhttprequest;1", "nsIJSXMLHttpRequest");
		var req = new XMLHttpRequest();
		req.mozBackgroundRequest = true;
		req.open('GET', url);
		req.addEventListener('load', function() {
			try {
			DEBUG("resp = " + req.responseText);
			var elem = document.getElementById('mklogout');
			elem.setAttribute('hidden', 'true');
			elem = document.getElementById('mkseparator');
			elem.setAttribute('hidden', 'true');
			elem.setAttribute('uid', '');
			} catch(ex) {
				DEBUG("logout ex = " + ex);
			}
		}, false);
		req.send(null);
	} catch(ex) {
		DEBUG("ex = " + ex);
	}
}

mikanObject.prototype.doShare = function() {
	DEBUG("gMK.doShare");
	try {
		window.open("chrome://mikan/content/share.xul", "Share", "modal,chrome,width=320,height=220,centerscreen");
	} catch(ex) {
		DEBUG("ex = " + ex);
	}
}

mikanObject.prototype.init = function() {
	try {
	  var bar = document.getElementById("nav-bar");
	  var mkbtn = document.getElementById("mikan-item");
    bar.insertBefore(mkbtn, document.getElementById("search-container"));
	  DEBUG("gMK.init" + " mkbtn = " + mkbtn);
	} catch(ex) {
		DEBUG("ex = " + ex);
	}
}

mikanObject.prototype.term = function() {
	DEBUG("gMK.term");
	var elem = document.getElementById('mklogout');
	var uid = elem.getAttribute('uid');
	if(uid) {
		DEBUG("uid = " + uid);
		try {
		gMK.doLogout();
		} catch(ex) {
			DEBUG("ex = " + ex);
		}
	}
}

var gMK = new mikanObject();

window.addEventListener("load", gMK.init, false);
window.addEventListener("unload", gMK.term, false);
