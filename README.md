
        __ __| |           |  /_) |     ___|             |           |
           |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
           |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
          _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/


# MPC-LiveXplore
Akai MPC Live/X/Force/One technical explorations and hacks

Don't forget the wiki here : https://github.com/TheKikGen/MPC-LiveXplore/wiki

| [Download link for Force 3.3.0, MPC 2.14 SSH and bootstrap images](https://drive.google.com/drive/folders/1Bq1lbtcFUft9sbbCdm-z_temecFJeeSV?usp=sharing) 

In the ssh folder, you will find last ssh update images for Akai MPCs and Force.  
Bootstrap sdcard images (IamForce2, rtpmidi, .. ) are stored in the "bootstrap" folder. 
  
To flash a sdcard image, I recommend to use Balena [Etcher](https://www.balena.io/etcher/).  This is secure. You need a usb/sdcard at less of 4G of capacity.  

If you download any of these images, please consider making a small donation to encourage our team to continue their research into your favorite toys.  This will pay for the coffees swallowed during the long nights of reverse-engineering!

[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=thekikgen@gmail.com&lc=FR&item_name=Donation+to+TheKikGen+projects&no_note=0&cn=&currency_code=EUR&bn=PP-DonationsBF:btn_donateCC_LG.gif:NonHosted)

Thanks : Gian Andrea, Christopher, Egil, Conte de Sociale, Jamie Fenton, Harald Fiekler, Michael Hess, Dawid Wenderski !

Other Github projects : <br>
[Hacking libraries and code for MPC and Force devices](https://github.com/TheKikGen/MPCLiveXplore-libs)<br>
[Bootstrap : MPC/Force boot time customization](https://github.com/TheKikGen/MPC-LiveXplore-bootstrap)<br>
[USBMidiKlik4x4 : advanced multi-port USB MIDI interface](https://github.com/TheKikGen/USBMidiKliK4x4)<br>
[Kikpad : new firmware for the Smartpad, a Launchpad clone from Midiplus](https://github.com/TheKikGen/kikpad)<br>

____


## ACVR : A new MPC for Christmas ? (2023-11-15)
(ACVR is the MPC KEY 37)
A member of the "MPC X LIVE ONE KEYS FORCE hacking group" reported that a new "ACVR" Id was under examination (prototype stage) at the FCC. 
Most probably a new device, maybe the "MPC MINI" , or a boosted MPC LIVE with 4GB memory..., like the One+.

[https://fccid.io/Y4O-ACVR](https://device.report/INMUSIC/)

## VNC4MPC : Remote control any MPC or Force from a computer with VNC (2023-04-15)
[![VNC4MPC video](https://img.youtube.com/vi/Dc77jStsER8/0.jpg)](https://www.youtube.com/watch?v=Dc77jStsER8) 
 
A new module will be available soon within the Bootstrap : VNC4MPC.  It allows you to control remotely your Force or your MPC 
from any computer with a VNC client installed (TightVNC, UltraVNC, VNCViewer...).  

## I AM FORCE - V2 - BETA RELEASE (2023-03-30)
[![iamforce video](https://img.youtube.com/vi/Ivp53UNk8ck/0.jpg)](https://www.youtube.com/watch?v=Ivp53UNk8ck) 
[![iamforce video](https://img.youtube.com/vi/gVtn33kzCa4/0.jpg)](https://www.youtube.com/watch?v=gVtn33kzCa4) 

You liked IamForce, allowing your MPC to become a Force, so here is the V2 BETA,  an entirely rewritten version of IamForce V1.
IamForce2 supports 4 differents midi devices with a plugin system, and a new standalone mode is you haven't any external controller. 
[Download the last bootstrap sddcard img](https://drive.google.com/drive/folders/1Bq1lbtcFUft9sbbCdm-z_temecFJeeSV?usp=sharing) and burn it to a USB stick or a sdcard.  

IamForce2 does not update your MPC, but launches an isolated environment for the Force software from the external SD card. If you remove the card, you will find your MPC again at the next boot. However, you must have installed a modified MPC image for ssh activation.  The latest version of the MPC SSH tkgl firmware is provided on the root of the card, so you can update directly from the MPC user interface.
If you own a MPC Live mk1 or mk2, IamForce2 can be used unplugged, on battery.

A [documentation](https://github.com/TheKikGen/MPCLiveXplore-libs/wiki) is available (still in progress).

## Collection of our Stats usage by AKAI/InMusic (2023-01-27)

Despite the fact that the checkbox "collect usage statistics" is unchecked in the preferences, my MPC keeps sending requests to Google Analytics.  A network sniff of the WLAN0 interface (the WIFI) shows this without any doubt.

                        Host: www.google-analytics.com
                        Accept: */*
                        User-Agent: MPC/2.11 (AZ01; ACV8U; <unknown>; 5.15.78)
                        Content-Type: application/x-www-form-urlencoded
                        Content-length: 159
                        192.168.002.025.49444-142.250.075.238.00080: POST /collect HTTP/1.1
                        Host: www.google-analytics.com
                        Accept: */*
                        User-Agent: MPC/2.11 (AZ01; ACV8U; <unknown>; 5.15.78)
                        Content-Type: application/x-www-form-urlencoded
                        Content-length: 203

and so on (more than 10 HTTP POST). As far as I know Google Analytics is not a license activation server ....

## MPC ONE MK2 FOR SOON  ?? (2023-01-26)

I just realized that a new hardware identifier has been added in the latest MPC 2.11.7 image: ACVA2. ACVA is the MPC One.  
Is there an MPC One MK2 in preparation in addition to the MPC XL (ACV5S) or is it just an identical hardware version that has evolved?

                        Akai image [MPC-2.11.7-Update.img] details :
                        timestamp : 639b5f86
                        description : MPC upgrade image
                        compatible : inmusic,acv5 inmusic,acv8 inmusic,acva
                        => inmusic,acva2
                        inmusic,acvb inmusic,acvm inmusic,acv5s
                        inmusic,devices : 9e8403a 9e8403b 9e84046
                        => 9e84056
                        9e84047 9e8404b 9e84052

update : So it's confirmed. Compliance documentation available at FCC.GOV web site...
____

## MPC XL - CODE NAME ACV5S - A NEW MPC X FOR SOON (2022-12-20)

While exploring the last Force 3.2 image update from Akai, someone from the [Mpc live X and Force hacking modding custom group](https://www.facebook.com/groups/550328948678055) found a shell script containing an "ACV5S" hardware id in front of "MPC XL" label.  There are also similar traces in the latest Akai images header.  So it seems that a new MPC x is about to be released soon, maybe with larger screen, more pads, more knobs, more memory, more etc... to be a XL one !
Discussion [here](https://github.com/TheKikGen/MPC-LiveXplore/discussions/47)

____

## DIRECT DOWNLOAD LINKS FOR AKAI/AIR PLUGINS CONTENTS (2022-11-11)

If you want to get instant download from any computer rather than doing that from your Force or MPC preferences menu, here are direct download links to all current plugins.  You need to unzip and copy the content in the ad-hoc subdirectroy of the "Synths" contents directory, on your external HD or USB key as follow :

Synths/                        

[AIR Music Technology - MPC - Fabric](https://cdn.inmusicbrands.com/akai/20421/AIR%20Music%20Technology%20-%20MPC%20-%20Fabric.zip)<br>
[AIR Music Technology - MPC - Fabric Electric Piano](https://cdn.inmusicbrands.com/akai/20421/AIR%20Music%20Technology%20-%20MPC%20-%20Fabric%20Electric%20Piano.zip)<br> 
[AIR Music Technology - MPC - Fabric Piano](https://cdn.inmusicbrands.com/akai/20421/AIR%20Music%20Technology%20-%20MPC%20-%20Fabric%20Piano.zip)<br>
[AIR Music Technology - MPC - Fabric XL](https://cdn.inmusicbrands.com/akai/20421/AIR%20Music%20Technology%20-%20MPC%20-%20Fabric%20XL.zip)<br>
[AIR Music Technology - MPC - Hype](https://cdn.inmusicbrands.com/akai/20421/AIR%20Music%20Technology%20-%20MPC%20-%20Hype.zip)<br>
[AIR Music Technology - MPC - Mellotron](https://cdn.inmusicbrands.com/akai/20421/AIR%20Music%20Technology%20-%20MPC%20-%20Mellotron.zip)<br> 
[AIR Music Technology - MPC - Mini D](https://cdn.inmusicbrands.com/akai/20421/AIR%20Music%20Technology%20-%20MPC%20-%20Mini%20D.2.zip)<br>
[AIR Music Technology - MPC - OPx-4](https://cdn.inmusicbrands.com/akai/20421/AIR%20Music%20Technology%20-%20MPC%20-%20OPx-4.zip)<br>
[AIR Music Technology - MPC - Organ](https://cdn.inmusicbrands.com/akai/20421/AIR%20Music%20Technology%20-%20MPC%20-%20Organ.zip)<br>
[AIR Music Technology - MPC - Solina](https://cdn.inmusicbrands.com/akai/20421/AIR%20Music%20Technology%20-%20MPC%20-%20Solina.zip)<br>
[AIR Music Technology - MPC - Stage EP](https://cdn.inmusicbrands.com/akai/20421/AIR%20Music%20Technology%20-%20MPC%20-%20Stage%20EP.zip)<br>
[AIR Music Technology - MPC - Stage Piano](https://cdn.inmusicbrands.com/akai/20421/AIR%20Music%20Technology%20-%20MPC%20-%20Stage%20Piano.zip)<br>
[AIR Music Technology - MPC - Studio Strings](https://cdn.inmusicbrands.com/akai/20421/AIR%20Music%20Technology%20-%20MPC%20-%20Studio%20Strings.zip)<br>
[WayOutWare - MPC - Odyssey](https://cdn.inmusicbrands.com/akai/20421/WayOutWare%20-%20MPC%20-%20Odyssey.zip)<br>

[AIR Music Technology - MPC - AIR Flavor Pro](https://cdn.inmusicbrands.com/akai/20421/AIR%20Music%20Technology%20-%20MPC%20-%20AIR%20Flavor%20Pro.2.zip)<br>
[AIR Music Technology - MPC - AIR Flavor Chorus](https://cdn.inmusicbrands.com/akai/20421/AIR%20Music%20Technology%20-%20MPC%20-%20AIR%20Chorus.zip)<br>
[AIR Music Technology - MPC - AIR Delay Pro](https://cdn.inmusicbrands.com/akai/20421/AIR%20Music%20Technology%20-%20MPC%20-%20AIR%20Delay%20Pro.zip)<br>
[AIR Music Technology - MPC - AIR Vintage Filter](https://cdn.inmusicbrands.com/akai/20421/AIR%20Music%20Technology%20-%20MPC%20-%20AIR%20Vintage%20Filter.zip)<br>



To download a plugin, simply click on one of the link above. 
The Fabric collection are not individual plugins. The main Fabric plugin is required for the other 3 to work (XL, Piano, Electric Piano).

NB : You'll need activation from Akai. This is just the content download.
____

## NEW SSH IMAGES RELEASED FOR MPC AND FORCE (2022-02-07)

                C:\Users\tkgl>ssh root@192.168.x.y

                  _____ _          _  ___ _                   _         _
                 |_   _| |_  ___  | |/ (_| |____ _ ___ _ _   | |   __ _| |__ ___
                   | | | ' \/ -_) | ' <| | / / _` / -_| ' \  | |__/ _` | '_ (_-<
                   |_| |_||_\___| |_|\_|_|_\_\__, \___|_||_| |____\__,_|_.__/__/
                                             |___/

                Welcome to the MPC OS ssh mod !
                Linux force 5.4.130-inmusic-2021-07-08-rt59 #1 SMP PREEMPT_RT Fri Jan 21 17:28:36 UTC 2022 armv7l GNU/Linux
                Your IP address is 192.168.2.126
                WARNING : you have root permissions. You could harm your system.
                root@force:~#

This release add a nice login banner, a usefull command prompt, and allows now to launch a bootstrap script from a FAT32 disk.  
Links at the top of the page. 

## I AM FORCE - V2 - PRELIMINARY RELEASE (2022-02-06)
[![iamforce video](https://img.youtube.com/vi/x_guX13cvb8/0.jpg)](https://www.youtube.com/watch?v=x_guX13cvb8) 
[![iamforce video](https://img.youtube.com/vi/zxvWxjmXFvg/0.jpg)](https://www.youtube.com/watch?v=zxvWxjmXFvg) 

This is a preliminary version of a further development of the IAMFORCE POC presented one year ago, allowing a MPC owner to test and run the Force software.
This special image works with a sdcard or a usb stick. No need to update your MPC.  Plug in it into your MPC, switch on the beast, and you will get  the Force. 
Important : you need an SSH modded image on your MPC (see links at the top of this page).

MPC settings are totally isolated from Force settings in a "chroot" sandbox, so you can't corrupt your MPC configuration.  
The Force image is embedded on the sdcard img. It is an original root file system, version 3.1.3 including streaming, and usb audio compliance.  

The shutdown doesn't work.  It will make you to return to the MPC mode, in which you can truly shutdown your MPC. So, a little pain...
One last point for MPC Live users : it can be used on battery. The power supply status is faked.

## MPC KEY 61 - CODE NAME ACVM - WILL BE AN HYBRID KBD/PADS MPC (2021-11-30)
<img width="500" border="0" src="https://www.synthanatomy.com/wp-content/uploads/2021/07/Akai-Pro-MPC-Key-61-cover.001.jpeg"  />

It seems that Akai finally decided to release not a Force (ACVK), but an MPC with a keyboard (ACVM). 
The Force synth project would have been abandoned in favor of an MPC synth.  
Images path in the last MPC img 2.10.1 :  /usr/share/Akai/ACVMTestApp/Resources

## I AM FORCE (2021-02-11)
[![tkglctrl video](https://img.youtube.com/vi/fVG7otydEA0/0.jpg)](https://www.youtube.com/watch?v=fVG7otydEA0) 

This [Proof of Concept](https://github.com/TheKikGen/MPC-LiveXplore/wiki/Iamforce-:-a-MPC-Live-like-a-Force-proof-of-concept) shows how an AKAI MPC Live and a hacked Midiplus Smartpad can be mutated to simulate a Force controller. Even if this project was technically challenging, believe me, the original Akai Force product offers much more comfort for almost the same price...one less tinkering !!


## FORCE 6 - CODE NAME ACVK - WILL BE AN HYBRID KEYBOARD/PADS DEVICE (2021-02-03)
<img width="500" border="0" src="https://medias.audiofanzine.com/images/thumbs3/akai-professional-force-3241206.png"  />

While exploring a recent MPC 2.9 update, someone from the [Mpc live X and Force hacking modding custom group](https://www.facebook.com/groups/550328948678055) found a png showing the **FORCE 6** new keyboard from Akai. This confirms the upcoming release of a 5 octaves hybrid keyboard, with 5 * 8 pads, 8 QLINKS, and a touch screen.   The embedded software will most certainly be the same as the one already installed on the MPC/Force range, as well as the RK3288-based hardware core.    
Png located internally at : /usr/share/Akai/ACVBTestApp/Resources/acvk.top.830x314.png

## GLOBAL MIDI MAPPING FOR STANDALONE MPC - HIDDEN FEATURE (2021-01-21)

I recently investigated more deeply around the "Global midi learn" to propose a less specific midi control than I did with the anyctrl.so LD_PRELOAD library. I discovered that the global midi mapping available in the MPC software is also available in standalone mode.   
Read the how-to [here](https://github.com/TheKikGen/MPC-LiveXplore/wiki/MPC-global-midi-mapping-in-standalone-mode-how-to).
 
## USE ANY MIDI CONTROLLER AS MPC OR FORCE CONTROL SURFACE (2021-01-04)

This is a hack of the MPC standalone software, using LD_PRELOAD and a special library launched with the MPC binary allowing the use of any usb midi controller as a secondary control surface. It is thus possible to extend available buttons, pads or qlinks like those of the MPC X or MPC One (track mute, pad mixer, solo, mute, etc...).  This is particularly interesting for the Force which does not have an external controller mode in the midi settings unlike the MPCs.  
[![tkglctrl video](https://img.youtube.com/vi/PQ-h3_DM6EI/0.jpg)](https://www.youtube.com/watch?v=PQ-h3_DM6EI)

Check the preload_libs github repo [here](https://github.com/TheKikGen/MPC-LiveXplore/tree/master/src/preload_libs).  

## IMG WITH SSH ACCESS for MPC LIVE AND FORCE (2020-11-25)

If you want to get a remote Linux command line with SSH instead of opening your MPC, 
patched images are available (edit : check at the top of this page) :

Very few things have been modified in that mod :
- empty password for root. Recent version of Force Firmware added a password to root acount.
- permanent activation of ssh at boot (multi-user.target.wants/sshd.service)
- ssh root empty password authentication in sshd_config
- bootstrap to launch a custom scripts on a sdcard before starting MPC

Remove any trailer after ".img" and copy the img to the root of an usb key.
A non-empty usb key will work contrary to what is stated in the documentation.

Check here to launch your own script : [tkgl_bootstrap](https://github.com/TheKikGen/MPC-LiveXplore-bootstrap) 

And here for more details about [SSH activation](https://github.com/TheKikGen/MPC-LiveXplore/wiki/Enabling-SSH-on-the-MPC-Live-X-one-Force)


## MPC V2.9.0 IS HERE AND THE DRUMSYNTH PLUGIN IS IN ! (2020-11-25)

The latest 2.9.0 MPC firmware features an 8-track DrumSynth, reminiscent of Roland's TRs, and Korg ER-1 that many will have forgotten! 
Honestly, this plugin is a success, and it is certain that many MPCs will be sold at Christmas!
Akai: Publish your APIs so third-party developers could create more plugins . Why not a MPC plugins market place ?

[![Akai Drumsynth intro](https://img.youtube.com/vi/lPDg5_H2eLA/0.jpg)](https://www.youtube.com/watch?v=lPDg5_H2eLA)


## THE MPC LIVE MK II CONFIRMED (2020-03-18)

ONE MONTH LATER : CONFIRMED. Look at [fcc web site](https://apps.fcc.gov/oetcf/eas/reports/ViewExhibitReport.cfm?mode=Exhibits&RequestTimeout=500&calledFromFrame=Y&application_id=RfTo93wUp5E%2FXSCXNY9a4Q%3D%3D&fcc_id=Y4O-ACVB) : 

## THE MPC LIVE MK II SOON (2020-02-01) ?

Beyond my [previous post in gearslutsz](https://www.gearslutz.com/board/showthread.php?p=14503941) regarding a shell script mentioning the MPC Live MKII, [other traces were found](https://github.com/TheKikGen/MPC-LiveXplore/wiki/MPC-Live-II-coming-soon...) in the 2.72 MPC binary after a deeper analysis and confirmed by the existence of 2x4 dtb linux drivers in the /boot directory.

I have also seen code areas mentioning new plugins that I think will be present in new releases or reserved for the new MPC?  

      => NEW :   MPC:DrumSynth    MPC:Xpand     MPC:OrganB3

we can therefore deduce that the 4 MPCs will share the same software platform and mainly the same core hardware.
This is very good news for MPC Live owners as that means updates and new features probably in a next future for all.   

## MPC Arp patterns and progressions

You will find all arp patterns and progressions here.
You must copy pattern files to '/usr/share/Akai/SME0/Arp Patterns',
and progressions to '/usr/share/Akai/SME0/Progressions'.
Don't forget to remount the partition read/write with a "mount remount / -o rw,remount" command.

-----------------------------------------------------------------------------------------------------------------------


I want also to mention here this excellent source of information from niklasnisbeth : https://niklasnisbeth.gitlab.io/mpc-internals/
