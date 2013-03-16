// ==UserScript==
// @name           dev
// @namespace      wrxzzj
// @description    dev
// @include        http://www.lcread.com/bookPage/*
// ==/UserScript==
//此处的修改是针对百度首页

changePage();

function changePage() {
	var ccon    = document.getElementById('ccon');
  var re = new RegExp("<br>");

  strarr = conn.innerHTML.match(re);
  alert(strarr[2]);
	//ccon.innerHTML = '<a href="http://www.lcread.com>连城读书网</a>';
}
