# MPC-LiveXplore
Akai MPC Live/X/Force technical explorations and hacks

## How to add 2 new midi ports C D : a quick guide with MIDI-OX.

<img border="0" width="350" src="https://github.com/TheKikGen/USBMidiKliK4x4/wiki/mpclive-hack/akai-mpc-live-usbmidiklik.jpg?raw=true"  />

1. First, you must install MIDI-OX (or any other MIDI tool) on your computer (You will find this software here : http://www.midiox.com/ for Windows).

2. Connect the [UsbMidiKlik3x3 board](https://github.com/TheKikGen/USBMidiKliK4x4) to an USB port, and launch MIDI-OX. You should see the USB Midi Klik 4x4 in the MIDI Devices.

<img border="0" src="https://github.com/TheKikGen/USBMidiKliK4x4/wiki/sysexguide/sysexguide_midiox1.png?raw=true"  />

3. To configure the board with SYSEX, you must choose the MIDI Output  "USB MIDIKlik 4x4"  being the first port as port 2-4 don't understand sysex (this is an optimization avoiding to  listen midi sysex on all of the 4 USB ports).

_Note : Don't forget that even if you have 4 USB port, you have only 3 hardware midi jacks !_

4. Open the Sysex View (Menu View->SysEx...)  : now we can start sending SYSEX.

<img border="0" src="https://github.com/TheKikGen/USBMidiKliK4x4/wiki/sysexguide/sysexguide_midiox2.png?raw=true"  />

<img border="0" src="https://github.com/TheKikGen/USBMidiKliK4x4/wiki/sysexguide/sysexguide_midiox3.png?raw=true"  />

5. Change the USB Product ID & Vendor ID to let think the MPC Live it is an Akai interface. Syssex message have the following format : 

`F0 77 77 78 <func = 0x0C> <n1n2n3n4 = Vendor Id nibbles> <n1n2n3n4 = Product Id nibbles> F7`

USB Vendor ID / Product ID of the Live internal midi interface are  (hex values) => Vendor Id:  09E8, Product Id: 003B

So the corresponding sysex message will be :

`F0 77 77 78 0C 00 09 0E 08 00 00 03 0B F7`

6. Change the Product String as it is checked by the standalone software of the MPC Live, the sysex message will be the following  :

`F0 77 77 78 <func id = 0x0b> <USB Midi Product name > F7`

=> Product string is  "MPC Live Controller". The string must be converted to hexadecimal values :
`4D 50 43 20 4C 69 76 65 20 43 6F 6E 74 72 6F 6C 6C 65 72`

So the final assembled sysexe msg will be :

`F0 77 77 78 0B 4D 50 43 20 4C 69 76 65 20 43 6F 6E 74 72 6F 6C 6C 65 72  F7`

7. Copy/Paste both messages into the sysex Command Windows then send the full message to port 1 of your USBMidiKlik board.

<img border="0" src="https://github.com/TheKikGen/USBMidiKliK4x4/wiki/sysexguide/sysexguide_midiox4.png?raw=true"  />

<img border="0" src="https://github.com/TheKikGen/USBMidiKliK4x4/wiki/sysexguide/sysexguide_midiox5.png?raw=true"  />

8. Quit MIDI-OX, then unplug / replug the board into the USB port of your PC. Wait 2 sec.
9. Open MIDI-OX again and you should see a "new" interface named "MPC Live Controller" in the MIDI Devices window.
10. Unplug the board, and plug it into your MPC Live USB. 
11. Boot or reboot the MPC Live (or open a new project) : you will be able to select port C and D on a midi track now. 

Full msg for easy copy paste :

`F0 77 77 78 0C 00 09 0E 08 00 00 03 0B F7 F0 77 77 78 0B 4D 50 43 20 4C 69 76 65 20 43 6F 6E 74 72 6F 6C 6C 65 72 F7`

Have fun.

## How to activate the linux console

https://github.com/TheKikGen/MPC-LiveXplore/wiki/How-to-activate-the-linux-console

