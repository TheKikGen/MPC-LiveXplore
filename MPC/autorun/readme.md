### What is the Autorun?
The built-in `autorun` feature is a low-level system mechanism designed exclusively to execute official, digitally signed AKAI update scripts or factory content deployments. The system triggers this process from an external drive either at boot time or when a storage device is hot-plugged.

### How It Works: The Sub-Script Validation Bypass
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

# 📂 Technical Analysis: The "Static/Dynamic" Autorun Hack for AKAI MPC & Force

This page provides an in-depth breakdown of the architecture, workflow, and bypass logic used to circumvent cryptographic signature and footprint verifications within the standalone AKAI MPC / Force operating system (compatible up to firmware v3.9.1). This method allows for the execution of arbitrary root-level code without breaking the OS validation chain.

---

## 1. Overview & Research Approach

The AKAI standalone operating system implements a strict security check when an external storage device containing an `autorun` folder is inserted or detected at boot. The OS does not merely read the configuration manifest (`autorun.json`); it explicitly requires a matching verification key inside `autorun.check`.

### The Discovery Process
1.  **Signature Analysis:** The initial objective was to build a utility capable of recalculating and signing the `autorun.check` footprint on the fly whenever script files were updated.
2.  **The SHA-256 Roadblock:** Internal analysis revealed that the key stored inside `autorun.check` is not a basic file concatenation or a standard, easily reproducible hash. The internal routine utilizes a more complex structure incorporating unique salt values, specific header arrangements, or embedded private keys.
3.  **The Strategic Pivot (The Hack):** Rather than trying to reverse-engineer Akai's complex cryptographic signing tool for every single script revision, the strategy shifted toward a **Two-Script Architecture (Static / Dynamic)**:
    *   Generate the valid SHA-256 hash "manually", once and for all, for a strictly frozen package of core files (`autorun.json` + `tkgl_static_script.sh`).
    *   Delegate all custom user operations to a secondary sub-script completely hidden from AKAI's boot validator.

---

## 2. Hack Architecture & Directory Structure

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


## 6. C++ SHA256 encoding extrapolaton deducted from MPC binary

```cpp
#include <cstdint>
#include <cstring>

// Stream interface deduced from the call (*(*arg2 + 0x10))(arg2, srcpp, r2_2)
struct IStream {
    virtual ~IStream() = default;
    virtual void dummy1() = 0;
    virtual void dummy2() = 0;
    virtual void dummy3() = 0;
    // Offset 0x10 (4 * 4 bytes): 5th virtual function
    virtual size_t Read(void* buffer, size_t size) = 0; 
};

// SHA-256 round constants (equivalent to data_4add388)
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// Utility functions for circular rotations
inline uint32_t rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

inline uint32_t bswap_32(uint32_t x) {
    return __builtin_bswap32(x); // __vrev32 in your NEON
}

// Main function
void sub_autorun_condensatSHA_256(uint32_t* hash_out, IStream* stream, int32_t arg3_len_lo, int32_t arg4_len_hi) {
    
    // Initialization of SHA-256 state vectors (H0 to H7)
    uint32_t H[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    // Stream size management (64-bit) for padding
    uint64_t total_length = 0;
    if (arg4_len_hi >= 0) {
        total_length = ((uint64_t)arg4_len_hi << 32) | (uint32_t)arg3_len_lo;
    } else {
        total_length = ((uint64_t)0x7fffffff << 32) | 0xffffffff;
    }

    uint8_t buffer[64];
    uint32_t W[64];
    size_t len;
    uint64_t current_len = 0;

    while (true) {
        // Read a block (max 64 bytes / 0x40)
        size_t to_read = 64;
        len = stream->Read(buffer, to_read);
        current_len += len;

        // End condition or padding (len <= 0x3f equates to a partial block)
        if (len < 64) {
            break; 
        }

        // Message Schedule preparation (Endianness swap NEON -> __vrev32)
        for (int i = 0; i < 16; ++i) {
            std::memcpy(&W[i], buffer + i * 4, 4);
            W[i] = bswap_32(W[i]);
        }

        // Message Schedule extension
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(W[i - 15], 7) ^ rotr(W[i - 15], 18) ^ (W[i - 15] >> 3);
            uint32_t s1 = rotr(W[i - 2], 17) ^ rotr(W[i - 2], 19) ^ (W[i - 2] >> 10);
            W[i] = W[i - 16] + s0 + W[i - 7] + s1;
        }

        // Round initialization
        uint32_t a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];

        // Compression loop
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t temp1 = h + S1 + ch + K[i] + W[i];
            
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = S0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        // State update
        H[0] += a; H[1] += b; H[2] += c; H[3] += d;
        H[4] += e; H[5] += f; H[6] += g; H[7] += h;
    }

    // --- PADDING PHASE (when the stream stops or end of file is reached) ---
    buffer[len] = 0x80; // Append the "1" bit
    
    // If there is no room to append the 64-bit size (len > 55)
    if (len >= 56) {
        std::memset(buffer + len + 1, 0, 64 - len - 1);
        
        // Processing the intermediate padding block
        for (int i = 0; i < 16; ++i) { W[i] = bswap_32(((uint32_t*)buffer)[i]); }
        for (int i = 16; i < 64; ++i) {
            W[i] = W[i - 16] + (rotr(W[i - 15], 7) ^ rotr(W[i - 15], 18) ^ (W[i - 15] >> 3)) + 
                   W[i - 7] + (rotr(W[i - 2], 17) ^ rotr(W[i - 2], 19) ^ (W[i - 2] >> 10));
        }

        uint32_t a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t temp1 = h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + W[i];
            uint32_t temp2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            h = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
        }
        H[0] += a; H[1] += b; H[2] += c; H[3] += d;
        H[4] += e; H[5] += f; H[6] += g; H[7] += h;

        std::memset(buffer, 0, 56);
    } else {
        std::memset(buffer + len + 1, 0, 56 - len - 1);
    }

    // Append the total length in BITS at the very end of the block (bswap NEON)
    uint64_t total_bits = total_length * 8;
    uint32_t bit_len_hi = bswap_32((uint32_t)(total_bits >> 32));
    uint32_t bit_len_lo = bswap_32((uint32_t)total_bits);
    std::memcpy(buffer + 56, &bit_len_hi, 4);
    std::memcpy(buffer + 60, &bit_len_lo, 4);

    // Processing the final block with the integrated size
    for (int i = 0; i < 16; ++i) { W[i] = bswap_32(((uint32_t*)buffer)[i]); }
    for (int i = 16; i < 64; ++i) {
        W[i] = W[i - 16] + (rotr(W[i - 15], 7) ^ rotr(W[i - 15], 18) ^ (W[i - 15] >> 3)) + 
               W[i - 7] + (rotr(W[i - 2], 17) ^ rotr(W[i - 2], 19) ^ (W[i - 2] >> 10));
    }

    uint32_t a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t temp1 = h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + W[i];
        uint32_t temp2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
    }
    H[0] += a; H[1] += b; H[2] += c; H[3] += d;
    H[4] += e; H[5] += f; H[6] += g; H[7] += h;

    // The final __vst4 instruction and unimplemented {vshr...} reorder 
    // the vector registers and write the Big Endian result to the output.
    for (int i = 0; i < 8; ++i) {
        hash_out[i] = bswap_32(H[i]);
    }
}
```

❗ **Important Structural System Notice:**
Development analysis suggests that this custom autorun method will face a major security overhaul and mitigation measures beginning with the **v3.10** release branch (succeeding the v3.9.1 firmware). AKAI is expected to patch this exploit path either by introducing recursive checks on called sub-shells or by changing the underlying file signature validation routines entirely. Ensure you utilize this flexibility while your devices remain on legacy v2.x / v3.9.x firmware branches.
