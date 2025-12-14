This repo is no longer maintained. I haven't used it in many years and don't have any sort of test setup. There have been a couple of recent pull requests with what appears to be a lot of good work. If something isn't working well for you, perhaps check out those related forks.


cpiped captures an audio stream and outputs it to a pipe with a bit of buffering
to handle clock mismatch. It also includes silence/sound detection and the ability
to run a command on detection.

I'm using this to capture line-in audio from a sound card and send it to 
forked-daapd (https://github.com/ejurgensen/forked-daapd). On sound detection, it
runs a simple script to start playing the associated pipe in forked-daapd.
