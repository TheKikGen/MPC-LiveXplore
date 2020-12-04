        __ __| |           |  /_) |     ___|             |           |
           |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
           |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
          _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/

# MPC / Force bootstrap scripts

This is a bootstrap implementation on the Akai MPCs / Force allowing system customization : linux application, data rescue, settings backup, ... 
You need to download a modded image that will launch it on the external sd card, before starting the MPC application.

1. Copy to the tkgl_bootstrap directory to the root of an sdcard/usb stick, preferably formatted with ext4 filesystem

2. Update with the last MPC / Force modded image to enable the bootstrap script :

          [MPC 2.9.0](https://drive.google.com/drive/folders/1A57y88qUesdRu_S2F8FVn3AhZaA_dDgG?usp=sharing)

          [Force 3.0.6](https://drive.google.com/drive/folders/1AqEcxZnJkUNG-8yA7DVGSTJy_sd6ijqr?usp=sharing)

   The launch script on the internal ssd will find the tkgl_bootstrap script automatically. 

3. Adapt the /tkgl_bootstrap/scripts/tkgl_bootstrap script to your needs 

   By default, the script creates an overlay on the Arp Patterns directory to allows loading of your own ones from the MPC application.

4. Place any binary in the /tkgl_bootstrap/bin

5. Test locally via ssh before running in nominal mode

6. Reboot your MPC !

A full root access to the system is granted within tkgl_bootstrap, however the file system is mounted read-only for security reasons.
I do not recommend to install software and/or to deeply customize the internal file system. You will loose everything at the next update.
Instead implement your custom app on the sdcard, to ensure isolation with the filesystem, and to preserve your work.


