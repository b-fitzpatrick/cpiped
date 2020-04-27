cpiped (https://github.com/b-fitzpatrick/cpiped) captures an audio stream and outputs it to a pipe with a bit of buffering
to handle clock mismatch. It also includes silence/sound detection and the ability
to run a command on detection.

I'm using this to capture line-in audio from a sound card and send it to 
forked-daapd (https://github.com/ejurgensen/forked-daapd). On sound detection, it
runs a simple script to start playing the associated pipe in forked-daapd.

NDpiped builds on cpiped but modifies code such that the buffer is only written if there is sound.  Otherwise I found buffer was constantly writing and Forked-Daapd was playing silence rather then my playlist.
