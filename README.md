
        __ __| |           |  /_) |     ___|             |           |
           |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
           |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
          _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/


# MPC-LiveXplore
Akai MPC Live/X/Force/One technical explorations and hacks


## IMG WITH SSH ACCESS for MPC LIVE AND FORCE (2020-11-25)

If you want to get a remote Linux command line with SSH instead of opening your MPC, 
patched images are available :

MPC 2.9.0  https://drive.google.com/drive/folders/1A57y88qUesdRu_S2F8FVn3AhZaA_dDgG?usp=sharing

Force 3.0.6 https://drive.google.com/drive/folders/1AqEcxZnJkUNG-8yA7DVGSTJy_sd6ijqr?usp=sharing

Nothing else than ssh activation was modified.
Remove the "ssh" trailer and copy the img to the root of an usb key.
A non-empty usb key will work contrary to what is stated in the documentation.

[SSH activation Wiki page](https://github.com/TheKikGen/MPC-LiveXplore/wiki/Enabling-SSH-on-the-MPC-Live-X-one-Force)


## MPC V2.9.0 IS HERE AND THE DRUMSYNTH PLUGIN IS IN ! (2020-11-25)

The latest 2.9.0 MPC firmware features an 8-track DrumSynth, reminiscent of Roland's TRs, and Korg ER-1 that many will have forgotten! 
Honestly, this plugin is a success, and it is certain that many MPCs will be sold at Christmas!
Akai: Publish your APIs so third-party developers could create more plugins . Why not a MPC plugins market place ?

[![Akai Drumsynth intro](https://img.youtube.com/vi/lPDg5_H2eLA/0.jpg)](https://www.youtube.com/watch?v=lPDg5_H2eLA)


## THE MPC LIVE MK II CONFIRMED (2020-03-18)

ONE MONTH LATER : CONFIRMED. Look at [fcc web site](https://apps.fcc.gov/oetcf/eas/reports/ViewExhibitReport.cfm?mode=Exhibits&RequestTimeout=500&calledFromFrame=Y&application_id=RfTo93wUp5E%2FXSCXNY9a4Q%3D%3D&fcc_id=Y4O-ACVB) : 

## THE MPC LIVE MK II SOON (2020-02-01) ?

Beyond my [previous post in gearslutsz](https://www.gearslutz.com/board/showthread.php?p=14503941) regarding a shell script mentioning the MPC Live MKII, other traces were found in the 2.72 MPC binary after a deeper analysis and confirmed by the existence of 2x4 dtb linux drivers in the /boot directory.

I have also seen code areas mentioning new plugins that I think will be present in new releases or reserved for the new MPC?  

      => NEW :   MPC:DrumSynth    MPC:Xpand     MPC:OrganB3

we can therefore deduce that the 4 MPCs will share the same software platform and mainly the same core hardware.
This is very good news for MPC Live owners as that means updates and new features probably in a next future for all.   

The Force softwaree is very close: it is probably a branch of the MPC one. By the way, the executable runs on a MPC Live...and is also called "MPC". 

The main MCU board (originally designed by Radxa) is strictly the same on the 4 products: only the internal USB peripherals are different (controller, sound card). The MPC One supposed to be "a new" product is in fact a Live/X rebranded !  

The internal MIDI interface is named and identified as following, depending on the hardware :  

      USB String name = "MPC X Controller",  USB VID:PID = "09e8:003a"
      USB String name = "MPC Live Controller", USB VID:PID = "09e8:003b"
      USB String name = "APC Live USB Device" or "Akai Pro Force", USB VID:PID = "09e8:0040"
      USB String name = "MPC One MIDI", USB VID:PID = "09e8:0046"
      USB String name = "MPC Live II", USB VID:PID = "09e8:0047"

Akai (InMusic) will certainly capitalize on the evolutions of its embedded software (it's very expensive to develop), and make the software of its products converge to a 3.x ... Personally, I speculate on a Live MPC MKII based on the same hardware but using the good recipes of the 4 previous MPC/Force (drums, CV gates, square format ...) + a significant evolution of the software: addition of plugin, Clip mode improved like what is done on the force, improved midi, etc. ...  

I also noticed that Akai was starting to lock its firmware more seriously...you can probably expect features to be enabled depending on the model you acquired...ex: X: everything. Live MK 2: almost everything, MK1/One nothing more...  Right now, it is clearly stated in the latest MPC (2.72) user manual that the MPC ONE does not have the standalone Ableton controller, even though the firmware is exactly the same as on the Live.

<img border="0" src="https://scontent-cdt1-1.xx.fbcdn.net/v/t1.0-9/85106040_889619251467495_2131203209392291840_n.jpg?_nc_cat=106&_nc_sid=1480c5&_nc_ohc=i9EmeYz5MZMAX8Rq8ff&_nc_ht=scontent-cdt1-1.xx&oh=27e9aad0ca843261a8a4f43d04195f19&oe=5E9C7341" />

We'll see that in 6 months...

All these InMusic (FCC ID Y40) products seem to share the same core hardware :
      
      ACV5 MPC X              ACV6 MPC Kitten Mattress      ACV7 MPC Touch
      ACV8 MPC Live           ACV9 MPC Studio Black         ACVA MPC Studio Live (aka ONE)
      ADA2 Borris The Turtle  JP07 SC5000 PRIME             JP08 SC5000M PRIME
      JP12 SC4000 PRIME       JC11 PRIME 4                  JC16 PRIME 2
      Force (Aka ADA2)        SC4000                        SC5000 PRIME Screen
      SC5000M PRIME Screen    SC4000 PRIME Screen           PRIME 4 Screen
      PRIME 2 Screen          ACVB MPC LIVE MKII


## MPC Arp patterns and progressions

You will find all arp patterns and progressions here.
You must copy pattern files to '/usr/share/Akai/SME0/Arp Patterns',
and progressions to '/usr/share/Akai/SME0/Progressions'.
Don't forget to remount the partition read/write with a "mount remount / -o rw,remount" command.

-----------------------------------------------------------------------------------------------------------------------
[How to add 2 new midi ports C D : a quick guide with MIDI-OX](https://github.com/TheKikGen/MPC-LiveXplore/wiki/Add-2-new-midi-ports-C-D---a-quick-guide-with-MIDI-OX)

[How to activate the linux console](https://github.com/TheKikGen/MPC-LiveXplore/wiki/How-to-activate-the-linux-console)

[SSH activation and flashing the rootfs with fastboot](https://github.com/TheKikGen/MPC-LiveXplore/wiki/SSH-activation-and-flashing-the-rootfs-with-fastboot)

[How to extract the rootfs image from an official Akai update img file](https://github.com/TheKikGen/MPC-LiveXplore/wiki/How-to-extract-the-2.6-rootfs-img-from-the-update.img)

[MPC Touch black screen syndrom (Windows 10)](https://github.com/TheKikGen/MPC-LiveXplore/wiki/MPC-Touch-black-screen-syndrom-(Win-10))

[MPC Live X internal EMMC disk informations](https://github.com/TheKikGen/MPC-LiveXplore/wiki/MPC-Live-X-internal-EMMC-disk-informations)


