var strconv = Components.classes["@mozilla.org/intl/utf8converterservice;1"].getService(Components.interfaces.nsIUTF8ConverterService);
var mystr = strconv.convertStringToUTF8("百度", "UTF-8", true);

