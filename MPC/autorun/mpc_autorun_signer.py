#!/usr/bin/env python3
"""
@file mpc_autorun_signer.py
@brief Akai MPC Standalone Autorun Package Signer & Hash Generator (Python 3)

Reverse-engineered from MPC firmware v3.9.1 (sub_248DD28, sub_248A2A0 & sub_9FF8B4).
Salt: "f7bed1c9-7e90-4731-9194-c996666b4331"

EXACT HASHING ORDER ENFORCED BY MPC FIRMWARE:
  1. autorun.json (raw bytes)
  2. Each script in "scripts" array (raw bytes, in declared order)
  3. Each file in "files" array (raw bytes, in declared order)
  4. Secret salt: "f7bed1c9-7e90-4731-9194-c996666b4331" (ASCII UTF-8)
  Hash = SHA256( autorun.json + scripts + files + salt )
"""

import sys
import os
import json
import hashlib
import argparse

MPC_AUTORUN_SALT = "f7bed1c9-7e90-4731-9194-c996666b4331"

def compute_autorun_hash(autorun_dir: str) -> str:
    json_path = os.path.join(autorun_dir, "autorun.json")
    if not os.path.exists(json_path):
        raise FileNotFoundError(f"Missing 'autorun.json' in {autorun_dir}")

    with open(json_path, "rb") as jf:
        json_raw = jf.read()

    parsed = json.loads(json_raw.decode("utf-8"))
    manifest = parsed.get("value0", parsed)

    hasher = hashlib.sha256()

    # 1. MPC FIRMWARE RULE: autorun.json is hashed FIRST!
    hasher.update(json_raw)

    # 2. Process all scripts declared in "scripts" array in order
    scripts = manifest.get("scripts", [])
    for item in scripts:
        rel_path = item if isinstance(item, str) else item.get("path")
        if not rel_path:
            continue
        full_path = os.path.join(autorun_dir, rel_path)
        if not os.path.exists(full_path):
            raise FileNotFoundError(f"Referenced script not found: {full_path}")
        with open(full_path, "rb") as sf:
            hasher.update(sf.read())

    # 3. Process all companion files declared in "files" array in order
    files = manifest.get("files", [])
    for rel_path in files:
        full_path = os.path.join(autorun_dir, rel_path)
        if not os.path.exists(full_path):
            raise FileNotFoundError(f"Referenced companion file not found: {full_path}")
        with open(full_path, "rb") as ff:
            hasher.update(ff.read())

    # 4. Append secret Akai salt string
    hasher.update(MPC_AUTORUN_SALT.encode("utf-8"))

    return hasher.hexdigest().lower()

def sign_package(autorun_dir: str):
    print(f"[*] Reading autorun package from: {autorun_dir}")
    digest = compute_autorun_hash(autorun_dir)
    check_path = os.path.join(autorun_dir, "autorun.check")
    with open(check_path, "w", encoding="utf-8") as f:
        f.write(digest)
    print(f"[+] Calculated SHA-256: {digest}")
    print(f"[✓] Successfully written to: {check_path}")

def verify_package(autorun_dir: str) -> bool:
    check_path = os.path.join(autorun_dir, "autorun.check")
    if not os.path.exists(check_path):
        print(f"[-] Verification failed: 'autorun.check' does not exist in {autorun_dir}", file=sys.stderr)
        return False

    with open(check_path, "r", encoding="utf-8") as f:
        expected = f.read().strip()

    computed = compute_autorun_hash(autorun_dir)
    print(f"[*] Expected Hash (autorun.check): {expected}")
    print(f"[*] Computed Hash (files + salt) : {computed}")

    if expected.lower() == computed.lower():
        print(f"[✓] VALID: The package matches the Akai MPC firmware requirements.")
        return True
    else:
        print(f"[✗] INVALID: Checksum mismatch! The MPC will reject this package.", file=sys.stderr)
        return False

def create_package(target_dir: str, script_path: str, message: str = "Run script on MPC?", ask: bool = True):
    if not os.path.exists(script_path):
        print(f"[-] Error: Script '{script_path}' does not exist.", file=sys.stderr)
        sys.exit(1)

    autorun_dir = os.path.join(target_dir, "autorun")
    os.makedirs(autorun_dir, exist_ok=True)

    script_name = os.path.basename(script_path)
    dest_path = os.path.join(autorun_dir, script_name)
    with open(script_path, "rb") as src, open(dest_path, "wb") as dst:
        dst.write(src.read())

    manifest = {
        "value0": {
            "files": [],
            "scripts": [
                {
                    "askQuestion": ask,
                    "path": script_name,
                    "message": message,
                    "noButtonText": "no",
                    "yesButtonText": "yes"
                }
            ]
        }
    }

    json_path = os.path.join(autorun_dir, "autorun.json")
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)

    sign_package(autorun_dir)
    print(f"\n[✓] Complete package created in: {autorun_dir}")
    print("    Copy the 'autorun' directory to the root of your USB drive or SD card.")

def main():
    parser = argparse.ArgumentParser(description="Akai MPC Standalone Autorun Signer (v3.9.1)")
    subparsers = parser.add_subparsers(dest="command", required=True)

    sign_p = subparsers.add_parser("sign", help="Sign an existing autorun directory")
    sign_p.add_argument("dir", nargs="?", default=".", help="Path to autorun directory")

    verif_p = subparsers.add_parser("verify", help="Verify an existing autorun directory")
    verif_p.add_argument("dir", nargs="?", default=".", help="Path to autorun directory")

    create_p = subparsers.add_parser("create", help="Create and sign a new autorun package")
    create_p.add_argument("target_dir", help="Target drive directory (where autorun/ will be created)")
    create_p.add_argument("--script", required=True, help="Path to the shell script to package")
    create_p.add_argument("--message", default="Run script on MPC?", help="Prompt message on Touch UI")
    create_p.add_argument("--no-ask", action="store_true", help="Do not prompt user with modal dialog")

    args = parser.parse_args()

    if args.command == "sign":
        sign_package(args.dir)
    elif args.command == "verify":
        ok = verify_package(args.dir)
        sys.exit(0 if ok else 1)
    elif args.command == "create":
        create_package(args.target_dir, args.script, args.message, not args.no_ask)

if __name__ == "__main__":
    main()
