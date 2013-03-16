    try {
      var stobj = Components.classes["@splashtop.com/stutils;1"].getService(Components.interfaces.nsISTUtils);
      var exepath = SvUtils.getSpecialDirectory("XREExeF").path;
      stobj.createShortCut(exepath, "-url stconnect:welcome", "Splashtop\ Connect.lnk", SvUtils.getExtensionFile("resource/SVlogo.ico").path);
      var product = stobj.getSystemInfo("select * from Win32_BaseBoard", "Product");
      D("Product = " + product);
      var version = stobj.getSystemInfo("select * from Win32_OperatingSystem", "Version");
      D("OSVersion = " + version);
      var MACAddress = stobj.getSystemInfo("select * from Win32_NetworkAdapter", "MACAddress");
      D("MACAddress = " + MACAddress);
      var SID = stobj.sid;
      D("SID = " + SID);
			var username = stobj.username;
      D("Username = " + username);
			var domain = stobj.domain;
			D("Domain = " + domain);
    } catch(e) {
      D("stobj = " + e);
    }

