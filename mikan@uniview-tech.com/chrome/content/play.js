function DEBUG(msg) {
	dump("MSG: " + msg + "\n");
}

mikanPlayObject.prototype.doCommand = function() {
	DEBUG("mikanPlay.doCommand");
	try {
  	var btn = document.getElementById('play-button');

		if(mkPlay.isPlaying == false) {
 		 	btn.setAttribute('label', "Stop");
 		 	btn.setAttribute('image', "chrome://mikan/content/stop.png");
		} else {
			btn.setAttribute('label', "Play");
			btn.setAttribute('image', "chrome://mikan/content/play.png");
		}
		mkPlay.isPlaying = !mkPlay.isPlaying;
	} catch(ex) {
		DEBUG("doCommand.ex = " + ex);
	}
}

mikanPlayObject.prototype.init = function() {
	try {
		mkPlay.isPlaying = false;
		DEBUG("mikanPlay.init");
		var tv = document.getElementById('play-tv1');
		tv.setAttribute('hidden', 'false');
		tv.setAttribute('label', "Daddy's TV");

		tv = document.getElementById('play-tv2');
		tv.setAttribute('hidden', 'false');
		tv.setAttribute('label', "Lili Wang's TV");

		tv = document.getElementById('play-tv3');
		tv.setAttribute('hidden', 'false');
		tv.setAttribute('label', "Tony's TV");

		document.getElementById('play-mikan-menulist').selectedIndex = 1;
		//document.getElementById('play-mikan-tvlist').selectedIndex = 2;
	} catch(ex) {
		DEBUG("ex = " + ex);
	}
}

mikanPlayObject.prototype.term = function() {
	DEBUG("mikanPlay.term");
}

var mkPlay = new mikanPlayObject();

window.addEventListener("load", mkPlay.init, false);
window.addEventListener("unload", mkPlay.term, false);
