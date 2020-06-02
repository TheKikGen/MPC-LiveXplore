#!/usr/bin/env python3
# __ __| |           |  /_) |     ___|             |           |
#    |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
#    |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
#   _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/
#   -----------------------------------------------------------------------------
#   MPC LIVE/X/Force Image extractor, and maker
#   Credits :
#       Pierre Lule for the first version of that script
#       Mamatt for discovering that Akai img is a dtb
#
# The authors disclaim all warranties with regard to this software, including
# all implied warranties of merchantability and fitness. In no event shall
# the authors be liable for any special, indirect or consequential damages or
# any damages whatsoever resulting from loss of use of your hardware, data or
# profits, whether in an action of contract, negligence or other tortious action,
# arising out of or in connection with the use or performance of this software.
# YOU USE THIS SOFTWARE AT YOUR OWN RISK !

import hashlib
import sys
import lzma
import subprocess

def print_usage():
    print("Usage :")
    print("mpc_img_tool.py extract <Akai img> <rootfs img>")
    print("mpc_img_tool.py make-live <rootfs img> <Akai MPC Live/One/X img>")
    print("mpc_img_tool.py make-force <rootfs src img file name> <Akai MPC Force img>")

def extract(in_update_img, out_img):
    print("Extracting {} from {}:".format(out_img, in_update_img))

    fdtget = ["fdtget", "-t", "u", in_update_img, "/images/rootfs", "data"]
    print(" ".join(fdtget))
    output = subprocess.run(fdtget, stdout=subprocess.PIPE).stdout.split()

    print("Decompressing and writing...")
    lzd = lzma.LZMADecompressor(format=lzma.FORMAT_XZ)
    b = 0
    with open(out_img, "wb") as f:
        for unsigned in output:
            compressed_bytes = int(unsigned).to_bytes(4, byteorder="big", signed=False)
            b += f.write(lzd.decompress(compressed_bytes))
            if(b % 100 == 0):
                print(b," bytes written\r",end=" ")
    print(b," bytes written")
    print("done !")

def create(mpc,in_img, out_update_img):
    print("Creating {} from {}".format(out_update_img, in_img))
    if mpc == "make-force":
        print("MPC Force image maker.")
        tmpl = """/dts-v1/;
/ {{
timestamp = <0x5dee18e1>;
description = "Akai Professional FORCE upgrade image";
compatible = "inmusic,ada2";
inmusic,devices = <0x9e84040>;
inmusic,version = "3.0.4.44";
images {{
rootfs {{
description = "Root filesystem";
data = <{data}>;
partition = "rootfs";
compression = "xz";
hash {{
value = <{sha}>;
algo = "sha1";
}};
}};
}};
}};
"""
    else:
        print("MPC Live/X/One image maker.")
        tmpl = """/dts-v1/;
/ {{
	timestamp = <0x5ebac103>;
	description = "MPC upgrade image";
	compatible = "inmusic,acv5", "inmusic,acv8", "inmusic,acva", "inmusic,acvb";
	inmusic,devices = <0x9e8403a 0x9e8403b 0x9e84046 0x9e84047>;
	inmusic,version = "2.8.0.56";

	images {{

		rootfs {{
			description = "Root filesystem";
			data = <{data}>;
            partition = "rootfs";
			compression = "xz";

			hash {{
				value = <{sha}>;
				algo = "sha1";
			}};
		}};
	}};
}};
"""

    sha = hashlib.new("sha1")

    print("Reading {}...".format(in_img))
    lzc = lzma.LZMACompressor(format=lzma.FORMAT_XZ)
    with open(in_img, "rb") as f:
        compressed_in_data = lzc.compress(f.read())
        print(".",end=" ")
    compressed_in_data += lzc.flush()
    sha.update(compressed_in_data)
    compressed_in_data_words = [hex(int.from_bytes(compressed_in_data[i:i+4], byteorder="big", signed=False)) for i in range(0, len(compressed_in_data), 4)]
    sha_bytes = sha.digest()
    sha_words = [hex(int.from_bytes(sha_bytes[i:i+4], byteorder="big", signed=False)) for i in range(0, len(sha_bytes), 4)]

    dtc_input = tmpl.format(data = " ".join(compressed_in_data_words), sha = " ".join(sha_words)).encode()
    dtc_cmd = ["dtc", "-I", "dts", "-O", "dtb", "-o", out_update_img, "-"]
    print(" ".join(dtc_cmd))
    subprocess.run(dtc_cmd, input = dtc_input)

# --------------------------------------------
# main
print("---------------------------------------------------------------------")
print("MPC LIVE IMAGE EXTRACTOR")
print("https://github.com/TheKikGen/MPC-LiveXplore\n")
print("Credit : The KikGen labs, Pierre Lule, Mamatt")
print("NB : the dtc utility must be acessible from the current path !")
print("---------------------------------------------------------------------\n")

if len(sys.argv) > 1:
    if sys.argv[1] == "extract" and len(sys.argv) == 4:
        extract(sys.argv[2], sys.argv[3])
        exit()

    if (sys.argv[1] == "make-live" or sys.argv[1] == "make-force")  and len(sys.argv) == 4:
        create(sys.argv[1],sys.argv[2], sys.argv[3])
        exit()

print_usage()

exit()
