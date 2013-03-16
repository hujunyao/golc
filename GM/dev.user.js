// ==UserScript==
// @name           lcread
// @namespace      wrxzzj
// @description    lcread filter
// @include        http://www.lcread.com/bookPage/*
// ==/UserScript==

changePage();

function changePage() {
	var ccon    = document.getElementById('ccon');
	var str = ccon.innerHTML;
	var strarr = str.split("<br>");
	var newstr = strarr[0] +"<br>"+ strarr[1] + "<br>";
	var len = strarr.length;
	
	for (var idx=len-2; idx > 1; idx-=3) {
		newstr += strarr[idx-1] + "<br>";
		newstr += strarr[idx] + "<br>";
		newstr += strarr[idx-2] + "<br>";
	}
	newstr += strarr[len-1];
	ccon.innerHTML = newstr;
}