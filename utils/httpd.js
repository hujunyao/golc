var http = require('http');
var pc= require('punycode');
var bytes = require('bytes');
var d = require('debug');
var fs = require('fs');
var url = require('url');
var path = require("path");
var minimal = require("minimal-node");

var fscss = "<style type='text/css'> a, a:active {text-decoration: none; color: blue;} a:visited {color: #48468F;} a:hover, a:focus {text-decoration: underline; color: red;} body {background-color: #F5F5F5;} h2 {margin-bottom: 12px;} table {margin-left: 12px;} th,td { font: monospace; text-align: left;} th { font: 120% font-weight: bold; padding-right: 14px; padding-bottom: 3px;} td {font: 100% ;padding-right: 14px;} td.s, th.s {text-align: right; padding-right:36px;} td.f {padding-right:36px} td.d {font: 100% font-weight bold;} div.list { font: 120% monospace; background-color: white; border-top: 1px solid #646464; border-bottom: 1px solid #646464; padding-top: 10px; padding-bottom: 14px;} div.foot { font: 120% monospace; color: #787878; padding-top: 4px;} </style>";

var mimeType = {'.js' : 'text/javascript','.css' : 'text/css', '.txt' : 'text/plain','.jpg' : 'image/jpeg','.gif' : 'image/gif','.png' : 'image/png', '.zip' : 'application/zip'};

var httpd = new Object();

httpd.path = process.cwd();

httpd.fetchDir = function(path) {
	var mydir = fs.readdirSync(httpd.path + path);
	var num = mydir.length;
	var dirBuf = "<table cellpadding='0' cellspacing='0'><thead><tr><th class='n'>Name</th><th class='s'>Size</th><th class='m'>Last Modified</th></tr></thead>";
	for(var i = 0; i < num; i++) {
		var file = httpd.path + path +  mydir[i];
		var stat = fs.statSync(file);
		d.DD("file = " + file);
		if(mydir[i][0] == '.') continue;

		if(stat.isDirectory()) {
			dirBuf += "<tr><td class='d'><a href='" + path + mydir[i]+ "/'>"+mydir[i]+"/</a></td><td class='s'>" + bytes(stat.size) + "</td><td class='m'>" + stat.mtime.toDateString() + "</td></tr>";
		} else {
			dirBuf += "<tr><td class='f'><a href='" + path + mydir[i]+ "'>"+mydir[i]+"</a></td><td class='s'>" + bytes(stat.size) + "</td><td class='m'>" + stat.mtime.toDateString() + "</td></tr>";
		}
		//d.DD("file[" + i + "] = " + mydir[i]);
	}

	return dirBuf;
}

http.createServer(function (req, res) {
	var r = url.parse(req.url);
	d.DD("path = " + r.pathname);
	var contentType = mimeType[path.extname(r.pathname)];
	var isRaw = true;

	if(r.pathname == "/favicon.ico") return;
	var stat = fs.statSync(httpd.path + r.pathname);

	if(stat.isDirectory()) {
		var buf = httpd.fetchDir(r.pathname);
		res.writeHead(200, {'Content-Type': 'text/html'});
		isRaw = false;
	} else {
		if(contentType) {
			var buf = fs.readFileSync(httpd.path + r.pathname);
			var contentType = mimeType[path.extname(r.pathname)];
			res.writeHead(200, {'Content-Type': contentType.length ? contentType : 'text/html'});
		} else {
			res.writeHead(200, {'Content-Type': 'text/html'});
		}
	}

	if(isRaw == false) {
		res.write("<?xml version='1.0' encoding='utf-8'?>\n<!DOCTYPE html PUBLIC '-//W3C//DTD XHTML 1.1//EN' 'http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd'>\n<html xmlns='http://www.w3.org/1999/xhtml' xml:lang='en'><head><title>Index of ");
		if(stat.isDirectory()) {
			res.write("</title>" + fscss + "</head><body><h2>Index of " + r.pathname + "</h2><div class='list'><table cellpadding='0' cellspacing='0'>");
		} else {
			res.write("</title>" + fscss + "</head><body><h2>Content of " + r.pathname + " </h2><div class='list'><table cellpadding='0' cellspacing='0'>");
		}
  	res.write(buf);
		res.end("</table></div><div class='foot'>Powered by Linux (Nodejs " +  process.version + ")</div></body></html>");
	} else {
		fs.readFile(httpd.path + r.pathname, function(err, data) {
			if(err) {
			} else {
				res.write(data, 'utf8');
				res.end();
			}
		});
	}
}).listen(3000, '192.168.1.110');

d.DD("this is a debug bytes = " + bytes(98732));
d.DD('Server running at http://192.168.1.110:1337/');
