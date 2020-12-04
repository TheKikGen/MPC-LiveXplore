        __ __| |           |  /_) |     ___|             |           |
           |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
           |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
          _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/

# MPC / Force bootstrap scripts

1. Copy to the root of an sdcard or a usb stick, preferably formatted with ext4 if you wish a read/write access Arp patterns

2. Update with the last MPC / Force modded image to enable the bootstrap script :

          [MPC 2.9.0](https://drive.google.com/drive/folders/1A57y88qUesdRu_S2F8FVn3AhZaA_dDgG?usp=sharing)

          [Force 3.0.6](https://drive.google.com/drive/folders/1AqEcxZnJkUNG-8yA7DVGSTJy_sd6ijqr?usp=sharing)

3. Adapt the /tkgl_bootstrap/scripts/tkgl_bootstrap script to your needs 

   By default, the script creates an overlay on the Arp Patterns directory to allows loading of your own ones from the MPC application.

4. Place any binary in the /tkgl_bootstrap/bin

5. Test locally via ssh before running in nominal mode

6. Reboot your MPC !


