    __ __| |           |  /_) |     ___|             |           |
      |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
      |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
     _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/
 
-----------------------------------------------------------------------------
# MPC LD_PRELOAD libraries


###	TKGL_ANYCTRL

This "low-level" library allows you to set up any controller as a control surface to drive the MPC standalone application. 

NB: you need ssh access to your MPC.

When pressing buttons  (START,STOP, MAIN, SHIFT, qlinks, pads, etc...), the hardware MPC/Force surface controller sends a corresponding note on / note off or a cc  midi msg.
By a simple midi message mapping in your own controller, it is possible now to simulate "buttons" press, and get more shortut like those of the MPC X 
(track mute, pad mixer, solo, mute, etc...) or to add more pads or qlinks. 

#### How is it done ?

All midi ports are changed back to non-exclusive,full read/write allowing the use of aconnect ALSA utility.   
The "PRIVATE" input port that is read by the MPC application to capture messages from the MPC controller is replaced by a rawmidi virtual port (usually #135). 
The input of that virtual port is reconnected to the physical output of the controller with a system aconnect command (see script below), before the sending of
SYSEX controller identification sequences.  This virtual port can be then used to connect any other controller in addition to the standard hardware.  
Note that running status are inhibited.

So , now, you can add "buttons" allowing direct access to the different screens of the MPC application, as the MPC X or ONE do.  
You can even consider making a dedicated DIY usb controller to add missing additional buttons or qlinks (like SOLO or MUTE buttons on the MPC Live for example).

#### Quick setup

Copy the tkgl_anyctrl.so library on a usb stick of a smartcard.
The "ANYCTRL_NAME" can contains a string / substring matching the name of the midi controller you want to use to simulate a MPC controller.   
To avoid crashes due to infinite loops, that controller will be disabled from the MPC application point of view, so you will not see it anymore in midi devices. 
If that variable is not defined, the application will start as usual, but will still use virtual ports in place of hardware ports.
You can retrieve the name of your midi controller with a "aconnect -l | grep client" command.
If you do this in an ssh session manually, don't forget to stop the MPC application with a "systemctl stop inmusic-mpc" command.

Example (after a fresh boot) : 

	# systemctl stop inmusic-mpc"

	# aconnect -l | grep client
	client 0: 'System' [type=kernel]
	client 20: 'MPC Live Controller' [type=kernel,card=1]
	client 24: 'KIKPAD' [type=kernel,card=2]
	client 130: 'Virtual MIDI Output 1 Input' [type=user,pid=265]
	client 131: 'Virtual MIDI Output 2 Input' [type=user,pid=265]
	client 132: 'Virtual MIDI Input 1 Output' [type=user,pid=265]
	client 133: 'Virtual MIDI Input 2 Output' [type=user,pid=265]
	
	# export ANYCTRL_NAME="KIKPAD";LD_PRELOAD=/media/tkgl/mpcroot/root/preload_libs/tkgl_anyctrl.so /usr/bin/MPC
	MPC 2.9.0
	------------------------------------
	TKGL_ANYCTRL V1.0 by the KikGen Labs
	------------------------------------
	(tkgl_anyctrl) MPC card id hw:1 found
	(tkgl_anyctrl) MPC Private port is hw:1,0,1
	(tkgl_anyctrl) MPC Public port is hw:1,0,0
	(tkgl_anyctrl) First virtual seq client available address is 135
	(tkgl_anyctrl) KIKPAD connect port is 24:0
	(tkgl_anyctrl) Port creation was already disabled for : KIKPAD
	**** Audio 44100Hz; 2-in; 6-out; 128sample buffer
	**** Warning: inefficient input path: hardware=2 filter=4
	**** Warning: inefficient output path: hardware=6 filter=8
	ButtonStates reply from firmware: {0,0,0,0}
	(tkgl_anyctrl) Port creation disabled for : Client-136 Virtual RawMIDI
	(tkgl_anyctrl) Port creation disabled for : Client-137 Virtual RawMIDI
	(tkgl_anyctrl) Port creation disabled for : Client-135 Virtual RawMIDI


#### Make

You need to setup a 2.30 glibc to incompatibility issues and undefined symbols when using LD_PRELOAD.  The best option is to build  chroot system dedicated for that.   Usually you will cross compile with "arm-linux-gnueabihf-gcc tkgl_anyctrl.c -o tkgl_anyctrl.so -shared -fPIC -ldl" 

You can also use the "tkgl_anyctrl" module of the tkgl_bootstrap (under construction).
