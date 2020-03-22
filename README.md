# MPC-LiveXplore
Akai MPC Live/X/Force/One technical explorations and hacks


## THE MPC LIVE MK II CONFIRMED (2020-03-18)

ONE MONTH LATER : CONFIRMED. Look at [fcc web site](https://apps.fcc.gov/oetcf/eas/reports/ViewExhibitReport.cfm?mode=Exhibits&RequestTimeout=500&calledFromFrame=Y&application_id=RfTo93wUp5E%2FXSCXNY9a4Q%3D%3D&fcc_id=Y4O-ACVB) : 

## THE MPC LIVE MK II SOON (2020-02-01) ?

Beyond my [previous post in gearslutsz](https://www.gearslutz.com/board/showthread.php?p=14503941) regarding a shell script mentioning the MPC Live MKII) :


other traces of a new MPC in the last 2.72 firmware MPC binary were found after deeper analysis :

<img border="0" src="https://scontent-cdt1-1.xx.fbcdn.net/v/t1.0-9/84102618_882090452220375_7210150333543088128_n.jpg?_nc_cat=106&_nc_ohc=z3gl3AKTi3gAX92AuhU&_nc_ht=scontent-cdt1-1.xx&oh=0d6fb9e5a964ff9a557f2711ed135717&oe=5EBEB9D5"  />

and confirmed by the existence of 2x4 dtb linux drivers in the /boot directory :

<img border="0" src="https://scontent-cdg2-1.xx.fbcdn.net/v/t1.0-9/84054703_882226772206743_6139209442400403456_n.jpg?_nc_cat=110&_nc_ohc=xLlWOZHcM7sAX_4w-Ks&_nc_ht=scontent-cdg2-1.xx&oh=a28ac0cd68f776431ca1d5806d293614&oe=5E8EB670" />

I have also seen code areas mentioning new plugins that I think will be present in new releases or reserved for the new MPC?  

      => NEW :   MPC:DrumSynth    MPC:Xpand     MPC:OrganB3

we can therefore deduce that the 4 MPCs will share the same software platform and mainly the same core hardware.
This is very good news for MPC Live owners as that means updates and new features probably in a next future for all.   

The Force softwaree is very close: it is probably a branch of the MPC one. By the way, the executable runs on a MPC Live...and is also called "MPC". 

https://www.facebook.com/groups/550328948678055/permalink/1024338721277073/  . 

The main MCU board (originally designed by Radxa) is strictly the same on the 4 products: only the internal USB peripherals are different (controller, sound card). The MPC One supposed to be "a new" product is in fact a Live/X rebranded !  

Akai (InMusic) will certainly capitalize on the evolutions of its embedded software (it's very expensive to develop), and make the software of its products converge to a 3.x ... Personally, I speculate on a Live MPC MKII based on the same hardware but using the good recipes of the 4 previous MPC/Force (drums, CV gates, square format ...) + a significant evolution of the software: addition of plugin, Clip mode improved like what is done on the force, improved midi, etc. ...  

I also noticed that Akai was starting to lock its firmware more seriously...you can probably expect features to be enabled depending on the model you acquired...ex: X: everything. Live MK 2: almost everything, MK1/One nothing more...  Right now, it is clearly stated in the latest MPC (2.72) user manual that the MPC ONE does not have the standalone Ableton controller, even though the firmware is exactly the same as on the Live.

<img border="0" src="https://scontent-cdt1-1.xx.fbcdn.net/v/t1.0-9/85106040_889619251467495_2131203209392291840_n.jpg?_nc_cat=106&_nc_sid=1480c5&_nc_ohc=i9EmeYz5MZMAX8Rq8ff&_nc_ht=scontent-cdt1-1.xx&oh=27e9aad0ca843261a8a4f43d04195f19&oe=5E9C7341" />

We'll see that in 6 months...


-----------------------------------------------------------------------------------------------------------------------
[How to add 2 new midi ports C D : a quick guide with MIDI-OX](https://github.com/TheKikGen/MPC-LiveXplore/wiki/Add-2-new-midi-ports-C-D---a-quick-guide-with-MIDI-OX)

[How to activate the linux console](https://github.com/TheKikGen/MPC-LiveXplore/wiki/How-to-activate-the-linux-console)

[SSH activation and flashing the rootfs with fastboot](https://github.com/TheKikGen/MPC-LiveXplore/wiki/SSH-activation-and-flashing-the-rootfs-with-fastboot)

[How to extract the rootfs image from an official Akai update img file](https://github.com/TheKikGen/MPC-LiveXplore/wiki/How-to-extract-the-2.6-rootfs-img-from-the-update.img)

[MPC Touch black screen syndrom (Windows 10)](https://github.com/TheKikGen/MPC-LiveXplore/wiki/MPC-Touch-black-screen-syndrom-(Win-10))

[MPC Live X internal EMMC disk informations](https://github.com/TheKikGen/MPC-LiveXplore/wiki/MPC-Live-X-internal-EMMC-disk-informations)


