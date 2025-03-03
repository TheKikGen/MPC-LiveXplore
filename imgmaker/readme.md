
        __ __| |           |  /_) |     ___|             |           |
           |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
           |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
          _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/


# mpcimg : image maker until MPC 2.15 and Force 3.3

This is a Python script.
Usage is : 
        
        ---------------------------------------------------------------------
        AKAI MPC/FORCE IMAGE TOOL - V1.1
        https://github.com/TheKikGen/MPC-LiveXplore
        (c) The KikGen labs.

        NB : the dtc utility must be acessible from the current path !
        ---------------------------------------------------------------------

        Usage :

        mpcimg  info [image file]

                extract  [image file in] [rootfs file out]
                         "rootfs_" is added to the rootfs file name.

                make-mpc | make-force [rootfs file in] [img file out] [version string]


# mpcimg2 : image maker from MPC and Force from V3.4

The image structure is not the same than previous firmware versions.
This is a C binary tool for Linux and WIN32/WIN64.


        AKAI MPC IMAGE TOOL - V2 - NEW MPC IMG FORMAT (FROM V3.4)
        https://github.com/TheKikGen/MPC-LiveXplore
        (c) The KikGen labs.
        
        Usage : mpcimg2 <action> <FILE>...
        Actions are :
         -i <Akai image V2 file in>
            : Display various information about the Akai image
        
         -x <Akai image V2 file in>
            : Extraction of the embedded XZ partition file.
            : Will generate a <your file name>.tmp.xz file in the current directory
        
         -r <Akai image V2 file in> <rootfs file out>
            : Uncompress the rootfs partition embedded within the Akai image
            : The <rootfs file out> extracted is ready to mount on any Linux file system
        
         -m <Akai image V2 file in> <rootfs file in> <Akai img V2 file out>
            : Make a new image by providing your own rootfs partition
            : The modified rootfs parttion size must be exactly the same as the original one
