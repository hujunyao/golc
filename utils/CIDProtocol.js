/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
 *   Doron Rosenberg <doronr@us.ibm.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Test protocol related
const kSCHEME = "cid";
const kPROTOCOL_NAME = "Search Protocol";
const kPROTOCOL_CONTRACTID = "@mozilla.org/network/protocol;1?name=" + kSCHEME;
const kPROTOCOL_CID = Components.ID("789409b9-2e3b-4682-a5d1-81ca80a76456");

// Mozilla defined
const kSIMPLEURI_CONTRACTID  = "@mozilla.org/network/simple-uri;1";
const kIOSERVICE_CONTRACTID  = "@mozilla.org/network/io-service;1";
const nsISupports            = Components.interfaces.nsISupports;
const nsIIOService           = Components.interfaces.nsIIOService;
const nsIProtocolHandler     = Components.interfaces.nsIProtocolHandler;
const nsIURI                 = Components.interfaces.nsIURI;

/*referencing uri*/
var g_baseURI;

function Protocol()
{
}

Protocol.prototype =
{
  QueryInterface: function(iid)
  {
    if (!iid.equals(nsIProtocolHandler) &&
        !iid.equals(nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  },

  scheme: kSCHEME,
  defaultPort: -1,
  protocolFlags: nsIProtocolHandler.URI_NORELATIVE |
                 nsIProtocolHandler.URI_NOAUTH,
  
  allowPort: function(port, scheme)
  {
    return false;
  },

  newURI: function(spec, charset, baseURI)
  {
    var uri = Components.classes[kSIMPLEURI_CONTRACTID].createInstance(nsIURI);
    uri.spec = spec;
    g_baseURI = baseURI.spec;
    dump("baseURI:" + g_baseURI + ".\n");
    iPos=g_baseURI.indexOf("/");
    conti = iPos
    while (conti >= 0)
    {
    	conti=g_baseURI.indexOf("/", iPos + 1);
    	dump("conti is " + conti + ", len is " + g_baseURI.length + " .\n");
    	if (conti >= 0)
    	{
    		iPos = conti;
    	}
    }
    dump("iPos=" + iPos + ".\n");
    if (iPos >= 0)
    {
    	g_baseURI=g_baseURI.substring(0, iPos + 1);
    }
    dump("baseURI1:" + g_baseURI + ".\n");
    return uri;
  },

  newChannel: function(aURI)
  { 
    var ios        = Components.classes[kIOSERVICE_CONTRACTID].getService(nsIIOService); 
    var partURI = aURI.spec; 
    partURI = partURI.substring(partURI.indexOf(":") + 1, partURI.length);    
    partURI = encodeURI(partURI);

    nsiURI = ios.newURI(g_baseURI+"attachments/"+partURI, null, null); 
    
    dump("cid:url: " + g_baseURI + "attachments/"+partURI + ".\n");

    var myChannel;
		if (g_baseURI.indexOf("http://") >= 0)
		{
			myChannel = ios.newChannelFromURI(nsiURI).QueryInterface(Components.interfaces.nsIHttpChannel); 
		}
		else if (g_baseURI.indexOf("file://") >= 0)
		{
	  	myChannel = ios.newChannelFromURI(nsiURI).QueryInterface(Components.interfaces.nsIFileChannel);
		}
		else if(g_baseURI.indexOf("https://") >= 0)
		{
			myChannel = ios.newChannelFromURI(nsiURI).QueryInterface(COmponents.interfaces.nsIHttpsChannel);
		}

    myChannel.originalURI = aURI; 

    return myChannel; 
  }, 
}

var ProtocolFactory = new Object();

ProtocolFactory.createInstance = function (outer, iid)
{
  if (outer != null)
    throw Components.results.NS_ERROR_NO_AGGREGATION;

  if (!iid.equals(nsIProtocolHandler) &&
      !iid.equals(nsISupports))
    throw Components.results.NS_ERROR_NO_INTERFACE;

  return new Protocol();
}


/**
 * JS XPCOM component registration goop:
 *
 * We set ourselves up to observe the xpcom-startup category.  This provides
 * us with a starting point.
 */

var TestModule = new Object();

TestModule.registerSelf = function (compMgr, fileSpec, location, type)
{
  compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
  compMgr.registerFactoryLocation(kPROTOCOL_CID,
                                  kPROTOCOL_NAME,
                                  kPROTOCOL_CONTRACTID,
                                  fileSpec, 
                                  location, 
                                  type);
}

TestModule.getClassObject = function (compMgr, cid, iid)
{
  if (!cid.equals(kPROTOCOL_CID))
    throw Components.results.NS_ERROR_NO_INTERFACE;

  if (!iid.equals(Components.interfaces.nsIFactory))
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    
  return ProtocolFactory;
}

TestModule.canUnload = function (compMgr)
{
  return true;
}

function NSGetModule(compMgr, fileSpec)
{
  return TestModule;
}

