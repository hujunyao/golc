function DEBUG(msg) {
	dump("MSG: " + msg + "\n");
}


mikanLoginObject.prototype.doPlay = function() {
	DEBUG("is_success " + this.is_success + " token = " + this.token);
}

mikanLoginObject.prototype.doLogin = function() {
	DEBUG("mikanLogin.doLogin");
	try {
		var obj = document.getElementById('username');
		var username = obj.value;
		obj = document.getElementById('password');
		var password = obj.value;
		if(username == "" || password == "")
			return;

		DEBUG("username = " + username + "password = " + password);
		//var req = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Components.interfaces.nsIXMLHttpRequest);
		var XMLHttpRequest = Components.Constructor("@mozilla.org/xmlextras/xmlhttprequest;1", "nsIJSXMLHttpRequest");
		var req = new XMLHttpRequest();
		req.mozBackgroundRequest = true;
		var url = "http://v.uniview-tech.com:8080/uvs/userInfo/login?username="+username+"&password="+password;
		req.open('GET', url);
		req.addEventListener('load', function() {
			try {
			var parser = new DOMParser();
			var doc = parser.parseFromString(req.responseText, "application/xml");
			var elems = doc.getElementsByTagName('is_success');
			//DEBUG("length = " + elems.length + " is_success = " + elems[0].textContent);
			//DEBUG("resp = " + req.responseText);
			if(elems[0].textContent == 'true') {
				mikanLoginObject.prototype.is_success = true;
				elems = doc.getElementsByTagName('user_token');
				mikanLoginObject.prototype.token = elems[0].textContent;
				//var elem = document.getElementById('loginbox');
				//elem.setAttribute("hidden", "true");
				//elem = document.getElementById('mkvbox');
				//elem.setAttribute("hidden", "false");
				var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"].getService(Components.interfaces.nsIWindowMediator);
				var mainWindow = wm.getMostRecentWindow("navigator:browser");
				var doc = mainWindow.document;
				var elem = doc.getElementById('mklogout');
				elem.setAttribute('hidden', 'false');
				elem.setAttribute('uid', username);
				elem.setAttribute('token', mikanLoginObject.prototype.token);
				elem = doc.getElementById('mkseparator');
				elem.setAttribute('hidden', 'false');
				window.close();
			} else {
				mikanLoginObject.prototype.is_success = false;
			}
			} catch(ex) {
				DEBUG("load XML resp, ex = " + ex);
			}
		}, false);
		req.send(null);
	} catch(ex) {
		DEBUG("ex = " + ex);
	}
}

mikanLoginObject.prototype.init = function() {
	try {
	} catch(ex) {
		DEBUG("ex = " + ex);
	}
}

mikanLoginObject.prototype.term = function() {
	DEBUG("mikanLogin.term");
}

var mkLogin = new mikanLoginObject();

window.addEventListener("load", mkLogin.init, false);
window.addEventListener("unload", mkLogin.term, false);
