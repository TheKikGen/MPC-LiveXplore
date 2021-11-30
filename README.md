
        __ __| |           |  /_) |     ___|             |           |
           |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
           |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
          _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/


# MPC-LiveXplore
Akai MPC Live/X/Force/One technical explorations and hacks

SSH images : 
| [MPC 2.10.1](https://drive.google.com/file/d/1pdYVSlkgc2BsGnpN1XlW4DVW8i5Ubkdl/view?usp=sharing) 
| [MPC 2.9.1](https://drive.google.com/file/d/1CV7ve0irk7pT2x3QsRl2mMMK_h4mAl7c/view?usp=sharing) 
| [Force 3.0.6](https://drive.google.com/file/d/13xuB-GGLZNNpmqIQehXZRNTmSv2YjwPi/view?usp=sharing)
| [Force 3.1.1](https://drive.google.com/file/d/1rG9qWXRzg0q-Ljx1ja5wt-AJe5oEf7gi/view?usp=sharing)


If you download any of these images, please consider making a small donation to encourage our team to continue their research into your favorite toys.  This will pay for the coffees swallowed during the long nights of reverse-engineering!

[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=thekikgen@gmail.com&lc=FR&item_name=Donation+to+TheKikGen+projects&no_note=0&cn=&currency_code=EUR&bn=PP-DonationsBF:btn_donateCC_LG.gif:NonHosted)

____

## MPC KEY 61 - CODE NAME ACVM - WILL BE AN HYBRID KBD/PADS MPC (2021-11-30)
<img width="500" border="0" src="https://scontent-cdt1-1.xx.fbcdn.net/v/t39.30808-6/262789660_1345518102544272_3765518872430473631_n.jpg?_nc_cat=109&ccb=1-5&_nc_sid=dbeb18&_nc_ohc=vqnKiiuh7OsAX-hQLty&_nc_ht=scontent-cdt1-1.xx&oh=8378ab2095dd3c3346bbc02c1a0e55ff&oe=61AB4935"  />

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

Check here to launch your own script : [tkgl_bootstrap](https://github.com/TheKikGen/MPC-LiveXplore/tree/master/tkgl_bootstrap) 

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

<ul class="m-0 p-0 list-style-none" data-filterable-for="wiki-pages-filter" data-filterable-type="substring" data-pjax="">
        <li class="Box-row">
          <strong><a class="d-block" href="https://github.com/TheKikGen/MPC-LiveXplore/wiki">Wiki Home</a></strong>
        </li>
        <li class="Box-row">
          <strong><a class="d-block" href="https://github.com/TheKikGen/MPC-LiveXplore/wiki/About-overlays-in-the-MPC-filesystem">About overlays in the MPC filesystem</a></strong>
        </li>
        <li class="Box-row">
          <strong><a class="d-block" href="https://github.com/TheKikGen/MPC-LiveXplore/wiki/Add-2-new-midi-ports-C-D---a-quick-guide-with-MIDI-OX">Add 2 new midi ports C D   a quick guide with MIDI OX</a></strong>
        </li>
        <li class="Box-row">
          <strong><a class="d-block" href="https://github.com/TheKikGen/MPC-LiveXplore/wiki/Enabling-SSH-on-the-MPC-Live-X-one-Force">Enabling SSH on the MPC Live X one Force</a></strong>
        </li>
        <li class="Box-row">
          <strong><a class="d-block" href="https://github.com/TheKikGen/MPC-LiveXplore/wiki/How-to-activate-the-linux-serial-console">How to activate the linux serial console</a></strong>
        </li>
        <li class="Box-row">
          <strong><a class="d-block" href="https://github.com/TheKikGen/MPC-LiveXplore/wiki/How-to-extract-the-2.6-rootfs-img-from-the-update.img">How to extract the 2.6 rootfs img from the update.img</a></strong>
        </li>
   <li class="Box-row">
          <strong><a class="d-block" href="https://github.com/TheKikGen/MPC-LiveXplore/wiki/Iamforce-:-a-MPC-Live-like-a-Force-proof-of-concept
">Iamforce : a MPC Live like a Force proof of concept</a></strong>
        </li>
        <li class="Box-row">
          <strong><a class="d-block" href="https://github.com/TheKikGen/MPC-LiveXplore/wiki/MPC-global-midi-mapping-in-standalone-mode-how-to">MPC global midi mapping in standalone mode how to</a></strong>
        </li>
        <li class="Box-row">
          <strong><a class="d-block" href="https://github.com/TheKikGen/MPC-LiveXplore/wiki/MPC-internal-controller---Midi-aspects">MPC internal controller   Midi aspects</a></strong>
        </li>
        <li class="Box-row">
          <strong><a class="d-block" href="https://github.com/TheKikGen/MPC-LiveXplore/wiki/MPC-Live-II-coming-soon...">MPC Live II coming soon...</a></strong>
        </li>
        <li class="Box-row">
          <strong><a class="d-block" href="https://github.com/TheKikGen/MPC-LiveXplore/wiki/MPC-Live-X-internal-EMMC-disk-informations">MPC Live X internal EMMC disk informations</a></strong>
        </li>
        <li class="Box-row">
          <strong><a class="d-block" href="https://github.com/TheKikGen/MPC-LiveXplore/wiki/MPC-Touch-:-How-to-solve-sound-pops-and-cracks">MPC Touch : How to solve sound pops and cracks</a></strong>
        </li>
        <li class="Box-row">
          <strong><a class="d-block" href="https://github.com/TheKikGen/MPC-LiveXplore/wiki/MPC-Touch-black-screen-syndrom-(Win-10)">MPC Touch black screen syndrom (Win 10)</a></strong>
        </li>
        <li class="Box-row">
          <strong><a class="d-block" href="https://github.com/TheKikGen/MPC-LiveXplore/wiki/SSD-partitioning">SSD partitioning</a></strong>
        </li>
    </ul>


I want also to mention here this excellent source of information from niklasnisbeth : https://niklasnisbeth.gitlab.io/mpc-internals/
