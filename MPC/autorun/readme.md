# Akai MPC Standalone Drive Autorun Engine: Complete Architecture & Cryptographic Specification
(Based on 3.0.1 analysis. NB : autorun is dropped from 3.10).

## 1. Executive Summary
The Akai MPC standalone operating system (firmware v3.9.1, ARM 32-bit Linux) embeds a native autorun subsystem designed to detect, validate, and execute shell automation packages from removable external storage (USB flash drives, SD cards, and external SATA drives).

Rather than executing files arbitrarily, the firmware enforces a **multi-stage security gatekeeper** combining directory heuristics, JSON schema validation, user-confirmation prompts, and a **salted cryptographic SHA-256 integrity verification**.

This document provides the definitive technical specification of the autorun subsystem, reverse-engineered directly from the `MPC` binary using IDA Pro and GDB.

---

## 2. Reverse-Engineered Class Architecture & Symbols

The autorun pipeline is implemented primarily in the `ScriptRunner` and `DriveConnections` classes:

```
[ Removable Drive Inserted ]
             │
             ▼
┌────────────────────────────────────────────────────────┐
│  DriveConnections_onDriveMounted (0x2489A2C)           │
│  - Virtual member of ScriptRunner vtable (0x6D778B4)   │
│  - Iterates over active mount points (/media/*)        │
└────────────────────────────┬───────────────────────────┘
                             │
                             ▼
┌────────────────────────────────────────────────────────┐
│  DriveAutorun_checkAndExecute (0x248E8EC)              │
│  - Verifies drive connection state (*a1 != 0)          │
│  - Locates '<DriveMountPath>/autorun/' directory       │
└────────────────────────────┬───────────────────────────┘
                             │
                             ▼
┌────────────────────────────────────────────────────────┐
│  DriveAutorun_scanDirectory (0x248DD28)                │
│  - RapidJSON manifest parser: sub_248B028              │
│  - Multi-stream SHA-256 verifier: sub_248A2A0          │
└────────────────────────────┬───────────────────────────┘
                             │
              [ Integrity Verified & Confirmed ]
                             │
                             ▼
┌────────────────────────────────────────────────────────┐
│  DriveAutorun_executeScript (0x248F4C8)                │
│  - Formats: sh "<DriveMountPath>/autorun/<script>"      │
│  - Dispatches execution to local daemon on port 8080   │
└────────────────────────────────────────────────────────┘
```

### Key Functions & Virtual Addresses

| Virtual Address | File Offset | Identified Symbol | Description |
|---|---|---|---|
| **`0x248E8EC`** | `0x1FBD8EC` | `DriveAutorun_checkAndExecute` | Central coordinator: checks paths, dispatches dialog, launches script |
| **`0x248DD28`** | `0x1FBCD28` | `DriveAutorun_scanDirectory` | Parses `autorun.json`, verifies presence of `autorun.check`, queues files |
| **`0x248A2A0`** | `0x1FB92A0` | `sub_248A2A0` (SHA-256 Verifier) | Computes multi-file stream hash + salt and compares with `autorun.check` |
| **`0x9FF8B4`** | `0x52E8B4` | `sub_9FF8B4` (Salt Decryptor) | Runtime constructor that decrypts the hardcoded Akai UUID salt |
| **`0x248B028`** | `0x1FBA028` | `sub_248B028` (JSON Parser) | RapidJSON / Cereal deserializer for `autorun.json` |
| **`0x248F4C8`** | `0x1FBE4C8` | `DriveAutorun_executeScript` | Shell command formatter (`sh "<path>"`) and execution dispatcher |
| **`0x2489A2C`** | `0x1FB8A2C` | `DriveConnections_onDriveMounted`| Storage listener called on initial mount / boot (`a3 = 1`) |
| **`0x2489E4C`** | `0x1FB8E4C` | `ScriptRunner::onDriveInserted` | Interactive hotplug listener called on dynamic USB insertion (`a3 = 0`) |

---

## 3. The 5 Sequential Gating Conditions

For an autorun package to be accepted and executed by the MPC, five strict conditions must be satisfied in order:

```
[ Drive Mount ] ──► (1. /autorun/ Dir?) ──► (2. Files Exist?) ──► (3. JSON Valid?) ──► (4. SHA256 Match?) ──► (5. UI Prompt) ──► [ EXECUTION ]
                          │                       │                     │                      │                     │
                          ▼ No                    ▼ No                  ▼ No                   ▼ No                  ▼ No
                       [ IGNORE ]              [ IGNORE ]            [ IGNORE ]             [ REJECT ]            [ ABORT ]
```

### Condition 1: Directory Location
The external media must contain a directory named **`autorun`** (strictly lowercase) at its root filesystem mount point:
```
<DriveRoot>/autorun/
```

### Condition 2: Mandatory Control File Pair
Inside `<DriveRoot>/autorun/`, two specific files must exist simultaneously:
1. **`autorun.json`**: The package configuration manifest.
2. **`autorun.check`**: The cryptographic hash reference file.
*If either file is missing (`sub_E1EA68`), the scan aborts immediately without any user notification.*

### Condition 3: Manifest Schema Compliance (`autorun.json`)
The manifest must be valid JSON parsed via RapidJSON (`sub_248B028`). The MPC firmware supports both top-level objects and Cereal/Boost-serialized `"value0"` root wrappers:

```json
{
  "value0": {
    "files": [
      "payload.tar.gz",
      "libcustom.so"
    ],
    "scripts": [
      {
        "path": "install.sh",
        "askQuestion": true,
        "message": "Do you want to install this custom mod?",
        "yesButtonText": "Install",
        "noButtonText": "Cancel"
      }
    ]
  }
}
```

#### Field Specifications:
* **`scripts`** (Array of Objects, Required):
  * **`path`** (String): Relative path to the executable shell script inside the `autorun/` folder.
  * **`askQuestion`** (Boolean): 
    * `true`: Displays a confirmation modal dialog on the touchscreen.
    * `false`: Bypasses the confirmation prompt and executes directly (subject to boot-time constraints).
  * **`message`** (String, Optional): Custom text displayed in the modal prompt.
  * **`yesButtonText`** (String, Optional): Label for the positive confirmation button (e.g., `"yes"`, `"Install"`, `"Run"`).
  * **`noButtonText`** (String, Optional): Label for the cancellation button (e.g., `"no"`, `"Cancel"`).
* **`files`** (Array of Strings, Optional):
  * List of companion archives, libraries, or binaries that must accompany the script. Every file listed here is included in the cryptographic hash verification.

---

## 4. The Cryptographic Integrity Engine

The most critical barrier preventing unauthorized or corrupted script execution is implemented in **`sub_248A2A0`**.

### The Hashing Formula

The MPC calculates a single **SHA-256** checksum over a concatenated multi-file input stream (`juce::SHA256`, `sub_E96404`).

$$\text{Final Digest} = \text{SHA-256}\Big(\text{autorun.json} + \text{Script}_1 + \dots + \text{Script}_N + \text{File}_1 + \dots + \text{File}_N + \text{AkaiSecretSalt}\Big)$$

### Exact Multi-Stream Concatenation Order:
1. **`autorun.json` raw bytes FIRST** (Reverse-engineered from assembly `0x248DEF4`–`0x248DF08`).
2. **Raw bytes of each script in `"scripts"`** (in order of declaration).
3. **Raw bytes of each file in `"files"`** (in order of declaration).
4. **The Akai Secret Salt string (ASCII UTF-8)**:  
   `"f7bed1c9-7e90-4731-9194-c996666b4331"`

### Origin & Decryption of the Secret Salt
In the compiled ARM binary, the salt does not appear in cleartext. It is stored as an obfuscated 37-byte buffer in `.data` (`0x52EDF2C`) and decrypted at application startup in **`sub_9FF8B4`**:

```c
// Decompiled from sub_9FF8B4 (0x9FF8B4)
static const uint8_t encSalt[37] = {
    0xe1, 0x0c, 0xf3, 0x9c, 0x21, 0x2a, 0x14, 0xf0,
    0xaa, 0x0c, 0xf4, 0xc0, 0x75, 0x36, 0x43, 0xfe,
    0xb4, 0x0a, 0xbc, 0xc0, 0x74, 0x22, 0x43, 0xe4,
    0xe4, 0x02, 0xa8, 0xcf, 0x73, 0x2d, 0x41, 0xab,
    0xb3, 0x08, 0xa2, 0xc8, 0x45
};

static const uint8_t xorKey[8] = { 0x87, 0x3b, 0x91, 0xf9, 0x45, 0x1b, 0x77, 0xc9 };

// Decryption produces the 36-character standard UUID:
// "f7bed1c9-7e90-4731-9194-c996666b4331"
```

### Strict Formatting of `autorun.check`
The computed SHA-256 hash is compared character-by-character against the contents of `autorun.check` using `juce::CharacterFunctions::compare` (`sub_DB2678`):
* **No whitespace trimming is performed by the MPC binary.**
* If `autorun.check` contains leading spaces, trailing newlines (`\n`), or Windows carriage returns (`\r\n`), the comparison fails on the first byte mismatch.
* The file must be **exactly 64 ASCII bytes** containing the lowercase hexadecimal digest.

---

## 5. Execution Lifecycle & Operational Behaviors

### Boot-Time (Automount) vs. Hotplug Insertion
The third argument (`a3`) passed to `DriveAutorun_checkAndExecute` (`0x248E8EC`) controls user interaction:

```c
// Disassembly of sub_248E8EC (Line 211)
if ( askQuestion )
{
    if ( a3 ) 
        goto LABEL_21; // a3 == 1 -> SKIP PROMPT AND EXIT!
        
    // a3 == 0 -> DISPLAY TOUCH UI MODAL PROMPT
    v25 = sub_1968938(v44, 1, v50, v47, v48, &v49);
}
```

* **Boot-Time Mounting (`a3 = 1`):**  
  Triggered by `DriveConnections_onDriveMounted` (`0x2489A2C`) when the MPC boots with a drive already plugged in.  
  👉 If `"askQuestion": true`, **the modal prompt is suppressed** to avoid blocking firmware startup before the UI is initialized.
* **Interactive Hotplug (`a3 = 0`):**  
  Triggered by `ScriptRunner::onDriveInserted` (`0x2489E4C`) when a USB drive is inserted while the MPC is fully booted and idling.  
  👉 The touchscreen displays the confirmation prompt:  
  `"Script detected on [DriveName] - Do you want to run it?"`

### Launch Command
Once confirmed, execution is logged to stderr and dispatched via the internal system daemon (port 8080):
```bash
sh "<DriveMountPath>/autorun/<script>"
```
The script runs as **`root`**, giving it complete administrative authority over the system.

---

## 6. Official Packaging & Signing Tooling

To simplify creating, signing, and verifying autorun packages, two standalone command-line tools have been reverse-engineered and integrated into the project:

* **Python 3:** `MPC-391/mpc_autorun_signer.py` (No external dependencies required).
* **Node.js:** `MPC-391/mpc_autorun_signer.js`.

### CLI Usage Guide

#### 1. Generate and Sign a Complete USB Package:
Creates the `autorun/` directory, copies your script, generates `autorun.json`, and writes a validated `autorun.check`:
```bash
python3 MPC-391/mpc_autorun_signer.py create /media/my_usb_drive \
    --script ./my_script.sh \
    --message "Launch The KikGenLabs Plugin Manager?"
```

#### 2. Sign an Existing `autorun/` Directory:
If you have modified scripts, payloads, or `autorun.json`:
```bash
python3 MPC-391/mpc_autorun_signer.py sign /media/my_usb_drive/autorun
```

#### 3. Verify Integrity Before Testing on Hardware:
Confirms whether the firmware's SHA-256 verifier will accept the package:
```bash
python3 MPC-391/mpc_autorun_signer.py verify /media/my_usb_drive/autorun
```
*Expected Output:*
```
[*] Expected Hash (autorun.check): 926b161aa9916041002b3f0bbb09dcc0ac0993e69fb475390de5edfccf17ac90
[*] Computed Hash (files + salt) : 926b161aa9916041002b3f0bbb09dcc0ac0993e69fb475390de5edfccf17ac90
[✓] VALID: The package matches the Akai MPC firmware requirements.
```

---

## 7. Practical Applications & Best Practices

1. **Root Access & Maintenance:**
   Enables recovering corrupted settings, dumping firmware partitions, and running custom hardware diagnostics without modifying the read-only rootfs (`/dev/root`).
2. **Community Bootloaders (TKGL Bootstrap):**
   Utilized by mods to spawn background SSH servers, bridge USB MIDI controllers, or inject shared libraries via `/etc/ld.so.preload`.
3. **Hotplug Reliability:**
   Always plug the USB drive into the MPC **after** the system has booted into the main project screen if using `"askQuestion": true`.
4. **Clean Line Endings:**
   Ensure all shell scripts and JSON files use standard Unix line endings (`LF`, `\n`) rather than Windows DOS endings (`CRLF`, `\r\n`), as byte differences directly alter the SHA-256 digest.
### What is the Autorun?
The built-in `autorun` feature is a low-level system mechanism designed exclusively to execute official, digitally signed AKAI update scripts or factory content deployments. The system triggers this process from an external drive either at boot time or when a storage device is hot-plugged.






# 📂 Previous approach (DEPRECATED) : The "Static/Dynamic" Autorun Hack for AKAI MPC & Force

This page provides an in-depth breakdown of the architecture, workflow, and bypass logic used to circumvent cryptographic signature and footprint verifications within the standalone AKAI MPC / Force operating system (compatible up to firmware v3.9.1). This method allows for the execution of arbitrary root-level code without breaking the OS validation chain.

## 1. Overview & Research Approach

The AKAI standalone operating system implements a strict security check when an external storage device containing an `autorun` folder is inserted or detected at boot. The OS does not merely read the configuration manifest (`autorun.json`); it explicitly requires a matching verification key inside `autorun.check`.

### The Discovery Process
1.  **Signature Analysis:** The initial objective was to build a utility capable of recalculating and signing the `autorun.check` footprint on the fly whenever script files were updated.
2.  **The SHA-256 Roadblock:** Internal analysis revealed that the key stored inside `autorun.check` is not a basic file concatenation or a standard, easily reproducible hash. The internal routine utilizes a more complex structure incorporating unique salt values, specific header arrangements, or embedded private keys.
3.  **The Strategic Pivot (The Hack):** Rather than trying to reverse-engineer Akai's complex cryptographic signing tool for every single script revision, the strategy shifted toward a **Two-Script Architecture (Static / Dynamic)**:
    *   Generate the valid SHA-256 hash "manually", once and for all, for a strictly frozen package of core files (`autorun.json` + `tkgl_static_script.sh`).
    *   Delegate all custom user operations to a secondary sub-script completely hidden from AKAI's boot validator.

## 2. Hack Architecture & Directory Structure
---
### PREVIOUS METHOD - DEPRECAYED . USE THE SIGN SCRIPT
The MPC OS validates the integrity of the `autorun` package by verification against an `autorun.check` file. This file contains a complex **SHA-256 hash** generated over the `autorun.json` and its primary target script. Because simply concatenating the files is not enough to recreate Akai's specific signing key, a clever decoupled architecture is required:

*   **The Static Package:** A main, unmodifiable shell script (`tkgl_static_script.sh`) is declared within `autorun.json`. The SHA-256 hash for this exact package (`autorun.json` + static script) is pre-calculated "manually" to satisfy the system's `autorun.check` requirements.
*   **The Dynamic Sub-Script:** Instead of processing complex commands directly, the main static script is designed to do only one thing: execute a secondary sub-script named `tkgl_sub_script.sh`.
*   **The Benefit:** Because the system verification layer only hashes the primary files, **you can modify `tkgl_sub_script.sh` as much as you like** to run any arbitrary shell commands you want. The MPC will execute it without ever breaking the global hash validation.

You can download the zip file. It contains all the necessary scripts.


> 🔓 **Root Privileges Unlocked:** This mechanism grants root-level access to the standalone hardware, enabling features locked by InMusic, such as SSH terminal access, custom user interface themes, or mounting custom Linux utilities.

### Compatibility Note
This specific plug-and-play exploit folder structure works out of the box on FAT32-formatted USB drives or SD cards for MPC and Force standalone firmware versions up to **v2.9 / v3.9**, regardless of whether the machine was previously SSH-modded.

⚠️ **Disclaimer:** Modifying your device's filesystem carries a significant risk of bricking your unit and immediately voids the manufacturer's warranty. Proceed with caution.

---
The following diagram illustrates how the AKAI authentication system is bypassed by executing commands through an indirect sub-script call:

```text
[ USB Drive / SD Card (Root) ]
└── /autorun/
    ├── autorun.json          # Declares the static script (Do not modify)
    ├── autorun.check         # Contains the frozen SHA-256 Hash
    ├── tkgl_static_script.sh # Immutable script verified by the OS
    └── tkgl_sub_script.sh    # Your free, editable shell script
```

---

## 3. Detailed File Analysis

| File Name | Security Status | Role |
| :--- | :--- | :--- |
| `autorun.json` | 🔒 **Immutable** | Defines the system UI dialog text and sets the static execution target. |
| `autorun.check` | 🔒 **Immutable** | Holds the static 32-byte SHA-256 validation key. |
| `tkgl_static_script.sh` | 🔒 **Immutable** | Resolves directories and securely triggers the user sub-script. |
| `tkgl_sub_script.sh` | 📂 **Editable** | Holds your custom payload and arbitrary Linux shell commands. |

### A. The Manifest: `autorun.json`
This JSON file defines the system dialog text and establishes the initial execution target. **It must not be modified**, otherwise the signature key verification will fail.

```json
"scripts": [
  {
    "askQuestion": true, 
    "path": "tkgl_static_script.sh", 
    "message": "Install content? This will execute tkgl_static_script.sh", 
    "noButtonText": "no", 
    "yesButtonText": "yes"
  }
]
```
*Result:* It instructs the OS to prompt the user with a pop-up confirmation menu pointing exclusively to the static script file.

### B. The Security Lock: `autorun.check`
This validation file contains a single cryptographic footprint string:
```text
926b161aa9916041002b3f0bbb09dcc0ac0993e69fb475390de5edfccf17ac90
```
*Note:* This is the hexadecimal ASCII representation of the 32-byte SHA256 key pre-computed exclusively for the unchanged `autorun.json` and `tkgl_static_script.sh` package.

### C. The Static Pivot: `tkgl_static_script.sh`
This script acts as the core workaround mechanism. It dynamically resolves its execution directory path and evaluates whether the secondary payload script exists:

```sh
#!/bin/sh
SCRIPT_NAME=$(readlink -f "$0")
SCRIPT_DIR=$(dirname "$SCRIPT_NAME")

TKGL_SUB_SCRIPT_NAME=$SCRIPT_DIR/tkgl_sub_script.sh

echo "The KikGen Labs- Autorun for MPC family"

if [ -f "$TKGL_SUB_SCRIPT_NAME" ]
then
     . "$TKGL_SUB_SCRIPT_NAME"    # Sources and runs the sub-script
     EXIT_CODE=$?
else
    echo "Looks like there is not $SCRIPT_DIR/tkgl_sub_script.sh file here ! - exiting"
    exit 1
fi

echo "tkgl_sub_script.sh has ended with code $EXIT_CODE."
exit $EXIT_CODE
```
*Mechanism:* By using the dot symbol `. "$TKGL_SUB_SCRIPT_NAME"` (sourcing), the static script passes system control over to the custom script within the exact same shell environment, preserving full root privileges.

### D. The User Payload: `tkgl_sub_script.sh`
This is where user customization takes place. You can modify, rewrite, or inject any standard Linux commands into this file without invalidating the boot process:

```sh
#!/bin/sh
# Display msg on the console
echo "The KikGen Labs- Hello from sub script !"

# And again in a test file in /tmp
echo "The KikGen Labs- Hello from sub script !" > /tmp/autorunok2.txt
```

---

## 4. Why the Static Script CANNOT Be Modified

It is critically important to understand that you must never modify a single character, line break, or space within `autorun.json`, `autorun.check`, or `tkgl_static_script.sh`.

### Technical Breakdown:
*   The MPC OS calculates a fresh SHA-256 fingerprint of the target files when mounting the storage partition.
*   If a single byte of data is edited inside `tkgl_static_script.sh`, the **avalanche effect** of the SHA-256 hashing algorithm will output a completely unrecognized string.
*   The OS will match this new value against the static verification string embedded in `autorun.check`.
*   Because the values will no longer match, the OS security layer flags the script package as modified or corrupted and silently cancels the execution, preventing the installation dialog from rendering.

> 💡 **The Core Exploitation Logic:** The MPC security engine checks the integrity of `tkgl_static_script.sh`, but it does not monitor or recursively track external child processes or nested files loaded during runtime execution. The secondary file `tkgl_sub_script.sh` remains entirely invisible to AKAI's signature engine.

---

## 5. Future Horizons (Firmware v3.10+)

The "Static/Dynamic" deployment structure has proven completely reliable out of the box across MPC hardware models and the AKAI Force up to firmware **v3.9.1**. It remains a foundation for community-made modifications, custom patches, and SSH setups.

❗ **Important Structural System Notice:**
Development analysis suggests that this custom autorun method will face a major security overhaul and mitigation measures beginning with the **v3.10** release branch (succeeding the v3.9.1 firmware). AKAI is expected to patch this exploit path either by introducing recursive checks on called sub-shells or by changing the underlying file signature validation routines entirely. Ensure you utilize this flexibility while your devices remain on legacy v2.x / v3.9.x firmware branches.
