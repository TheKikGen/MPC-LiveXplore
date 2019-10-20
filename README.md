# MPC-LiveXplore
Akai MPC Live/X/Force technical explorations and hacks

After my hack to add 4 MIDI OUT on the MPC Live, I went a little further.
I plugged a TTL USB serial interface into the main CPU board after opening the case (with a few cold sweats!!!).

<img border="0" src="https://github.com/TheKikGen/USBMidiKliK4x4/wiki/mpclive-hack/akai-mpc-live-TTLserial.png?raw=true"  />

Once the connection is made, you can follow the entire Boot process of the MPC Live, until you get a login prompt. 

The "root" user without a password allows you to access the system, and for example to launch some ALSA commands (the Linux audio system).

```
# amidi -l
Dir Device    Name
IO  hw:2,0,0  MPC Live Controller MIDI 1
IO  hw:2,0,1  MPC Live Controller MIDI 2
IO  hw:2,0,2  MPC Live Controller MIDI 3
IO  hw:2,0,3  MPC Live Controller MIDI 4
#


# cat /proc/asound/cards
 0 [CODEC          ]: USB-Audio - USB Audio CODEC
                      Burr-Brown from TI USB Audio CODEC at usb-ff500000.usb-1.1, full speed
 1 [Audio          ]: USB-Audio - MPC Live Audio
                      Akai Professional MPC Live Audio at usb-ff500000.usb-1.5, high speed
 2 [Controller     ]: USB-Audio - MPC Live Controller
                      Akai Pro MPC Live Controller at usb-ff500000.usb-1.6, full speed
#


# lsusb -t
/:  Bus 02.Port 1: Dev 1, Class=root_hub, Driver=ehci-platform/1p, 480M
    |__ Port 1: Dev 2, If 0, Class=, Driver=hub/7p, 480M
        |__ Port 1: Dev 3, If 0, Class=, Driver=cdc_acm, 12M
        |__ Port 1: Dev 3, If 1, Class=, Driver=cdc_acm, 12M
        |__ Port 2: Dev 4, If 0, Class=, Driver=snd-usb-audio, 12M
        |__ Port 2: Dev 4, If 1, Class=, Driver=snd-usb-audio, 12M
        |__ Port 2: Dev 4, If 2, Class=, Driver=snd-usb-audio, 12M
        |__ Port 2: Dev 4, If 3, Class=, Driver=usbhid, 12M
        |__ Port 4: Dev 8, If 0, Class=, Driver=uas, 480M
        |__ Port 5: Dev 6, If 2, Class=, Driver=snd-usb-audio, 480M
        |__ Port 5: Dev 6, If 0, Class=, Driver=snd-usb-audio, 480M
        |__ Port 5: Dev 6, If 3, Class=, Driver=, 480M
        |__ Port 5: Dev 6, If 1, Class=, Driver=snd-usb-audio, 480M
        |__ Port 6: Dev 7, If 0, Class=, Driver=snd-usb-audio, 12M
        |__ Port 6: Dev 7, If 1, Class=, Driver=snd-usb-audio, 12M
/:  Bus 01.Port 1: Dev 1, Class=root_hub, Driver=dwc2/1p, 480M
#

```

We can see above that I have connected a Behringer "cheap" usb audio interface that is well detected by the OS but not visible in the MPC software.  


I was also able to decode all the MIDI messages of the controller in "internal" mode by snorting the messages with amidi.

<img border="0" src="https://github.com/TheKikGen/USBMidiKliK4x4/wiki/mpclive-hack/akai-mpc-live-padsysex.png?raw=true"  />

This opens up some interesting perspectives!
