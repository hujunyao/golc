// ==UserScript==
// @name           jjwxc
// @namespace      filter
// @description    jjwxc.net filter
// @include        http://www.jjwxc.net/*
// ==/UserScript==
changePage();

function changePage() {
	var ccon    = document.getElementsByClassName('noveltext');
	var str = ccon[0].innerHTML;
	var strarr = str.split("<br>");
	var newstr = strarr[0] +"<br>"+ strarr[1] + "<br>";
	var len = strarr.length;
	
	for (var idx=len-6; idx >1; idx-=3) {
		newstr += strarr[idx-1] + "<br>";
		newstr += strarr[idx] + "<br>";
		newstr += strarr[idx-2] + "<br>";
	}
	newstr += strarr[len-5] + "<br>" + strarr[len-4] + "<br>" + strarr[len-3] + "<br>" + strarr[len-2] + "<br>" + strarr[len-1];
	ccon[0].innerHTML = newstr;
}