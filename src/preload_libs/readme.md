    __ __| |           |  /_) |     ___|             |           |
      |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
      |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
     _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/
 
-----------------------------------------------------------------------------
# MPC LD_PRELOAD libraries


###	TKGL_ANYCTRL MPC LD_PRELOAD library.

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
Create a shell script named "tkgl_anyctrl_cxscript.sh" to initiate midi connections, and place it in the same directory than the library. 
The script must contain at a minimum the following command to enable the standard MPC controller :
  
	  # MPC controller 
    aconnect 20:1 135:0

You can add then any connection you may need after that line, for example :

	  # MPC controller 
    aconnect 20:1 135:0
    # Nanokey studio 
    aconnect 24:0 135:0

I strongly recommend to disable all MPC and virtual input/output "135" midi ports in the MPC midi settings to avoid midi feedback when in a midi track.

Stop MPC application with "systemctl stop inmusic-mpc", and use the following preload syntax :

    LD_PRELOAD=/(path to the tkgl library)/tkgl_anyctrl.so /usr/bin/MPC

#### Compile with :

	  arm-linux-gnueabihf-gcc tkgl_anyctrl.c -o tkgl_anyctrl.so -shared -fPIC 

You can also use the "tkgl_anyctrl" module of the tkgl_bootstrap.
