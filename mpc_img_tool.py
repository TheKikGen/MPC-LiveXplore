#!/usr/bin/env python3
# Do not trust this script it will brick your mpc
# credit to Pierre Lulé
# https://gist.github.com/plule/67e1c542e2f16d92ddda0ac479010c39

import hashlib
import sys
import lzma
import subprocess

def print_usage():
    print("mpc_img_tool.py extract <in update.img> <out img>")
    print("mpc_img_tool.py create <in update.img> <in img> <out update.img>")

def extract(in_update_img, out_img):
    print("Extracting {} from {}:".format(out_img, in_update_img))

    fdtget = ["fdtget", "-t", "u", in_update_img, "/images/rootfs", "data"]
    print(" ".join(fdtget))
    output = subprocess.run(fdtget, stdout=subprocess.PIPE).stdout.split()

    print("Decompressing and writing...")
    lzd = lzma.LZMADecompressor(format=lzma.FORMAT_XZ)
    with open(out_img, "wb") as f:
        for unsigned in output:
            compressed_bytes = int(unsigned).to_bytes(4, byteorder="big", signed=False)
            f.write(lzd.decompress(compressed_bytes))

def create(in_img, out_update_img):
    print("Creating {} from {}".format(out_update_img, in_img))

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
    compressed_in_data += lzc.flush()
    sha.update(compressed_in_data)
    compressed_in_data_words = [hex(int.from_bytes(compressed_in_data[i:i+4], byteorder="big", signed=False)) for i in range(0, len(compressed_in_data), 4)]
    sha_bytes = sha.digest()
    sha_words = [hex(int.from_bytes(sha_bytes[i:i+4], byteorder="big", signed=False)) for i in range(0, len(sha_bytes), 4)]
    
    dtc_input = tmpl.format(data = " ".join(compressed_in_data_words), sha = " ".join(sha_words)).encode()
    dtc_cmd = ["dtc", "-I", "dts", "-O", "dtb", "-o", out_update_img, "-"]
    print(" ".join(dtc_cmd))
    subprocess.run(dtc_cmd, input = dtc_input)

if sys.argv[1] == "extract" and len(sys.argv) == 4:
    extract(sys.argv[2], sys.argv[3])
    exit()

if sys.argv[1] == "create" and len(sys.argv) == 4:
    create(sys.argv[2], sys.argv[3])
    exit()

print_usage()
exit()
