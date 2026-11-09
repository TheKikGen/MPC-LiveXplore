#!/usr/bin/env node
/**
 * @file mpc_autorun_signer.js
 * @brief Akai MPC Standalone Autorun Package Signer & Hash Generator
 * 
 * Reverse-engineered from MPC firmware v3.9.1 (sub_248DD28, sub_248A2A0 & sub_9FF8B4).
 * Salt: "f7bed1c9-7e90-4731-9194-c996666b4331"
 * 
 * EXACT HASHING ORDER ENFORCED BY MPC FIRMWARE:
 *   1. autorun.json (raw bytes)
 *   2. Each script in "scripts" array (raw bytes, in declared order)
 *   3. Each file in "files" array (raw bytes, in declared order)
 *   4. Secret salt: "f7bed1c9-7e90-4731-9194-c996666b4331" (ASCII UTF-8)
 *   Hash = SHA256( autorun.json + scripts + files + salt )
 */

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

// Secret salt decrypted from ARM32 sub_9FF8B4
const MPC_AUTORUN_SALT = "f7bed1c9-7e90-4731-9194-c996666b4331";

/**
 * Computes the official Akai MPC SHA-256 checksum for an autorun directory.
 * @param {string} autorunDir Path to the folder containing autorun.json
 * @returns {string} 64-character lowercase hexadecimal SHA-256 hash
 */
function computeAutorunHash(autorunDir) {
    const jsonPath = path.join(autorunDir, 'autorun.json');
    if (!fs.existsSync(jsonPath)) {
        throw new Error(`Missing 'autorun.json' in ${autorunDir}`);
    }

    const jsonRaw = fs.readFileSync(jsonPath);
    const parsed = JSON.parse(jsonRaw.toString('utf8'));
    
    // Support both root-level manifest and serialized "value0" wrappers
    const manifest = parsed.value0 || parsed;

    const hash = crypto.createHash('sha256');

    // 1. MPC FIRMWARE RULE: autorun.json is hashed FIRST!
    hash.update(jsonRaw);

    // 2. Process all scripts declared in "scripts" array in order
    if (Array.isArray(manifest.scripts)) {
        for (const item of manifest.scripts) {
            const relPath = typeof item === 'string' ? item : item.path;
            if (!relPath) continue;
            const fullPath = path.join(autorunDir, relPath);
            if (!fs.existsSync(fullPath)) {
                throw new Error(`Referenced script not found: ${fullPath}`);
            }
            hash.update(fs.readFileSync(fullPath));
        }
    }

    // 3. Process all companion files declared in "files" array in order
    if (Array.isArray(manifest.files)) {
        for (const relPath of manifest.files) {
            const fullPath = path.join(autorunDir, relPath);
            if (!fs.existsSync(fullPath)) {
                throw new Error(`Referenced companion file not found: ${fullPath}`);
            }
            hash.update(fs.readFileSync(fullPath));
        }
    }

    // 4. Append the secret Akai salt string (ASCII UTF-8)
    hash.update(Buffer.from(MPC_AUTORUN_SALT, 'utf8'));

    return hash.digest('hex');
}

/**
 * Signs an autorun directory by writing the calculated hash to autorun.check
 */
function signAutorun(autorunDir) {
    console.log(`[*] Reading autorun package from: ${autorunDir}`);
    const hash = computeAutorunHash(autorunDir);
    const checkPath = path.join(autorunDir, 'autorun.check');
    fs.writeFileSync(checkPath, hash, 'utf8');

    console.log(`[+] Calculated SHA-256: ${hash}`);
    console.log(`[✓] Successfully written to: ${checkPath}`);
}

/**
 * Verifies whether an existing autorun package satisfies MPC firmware checks
 */
function verifyAutorun(autorunDir) {
    const checkPath = path.join(autorunDir, 'autorun.check');
    if (!fs.existsSync(checkPath)) {
        console.error(`[-] Verification failed: 'autorun.check' does not exist in ${autorunDir}`);
        return false;
    }

    const expectedHash = fs.readFileSync(checkPath, 'utf8').trim();
    const computedHash = computeAutorunHash(autorunDir);

    console.log(`[*] Expected Hash (autorun.check): ${expectedHash}`);
    console.log(`[*] Computed Hash (files + salt) : ${computedHash}`);

    if (expectedHash.toLowerCase() === computedHash.toLowerCase()) {
        console.log(`[✓] VALID: The package matches the Akai MPC firmware requirements.`);
        return true;
    } else {
        console.error(`[✗] INVALID: Checksum mismatch! The MPC will reject this package.`);
        return false;
    }
}

/**
 * Scaffolds and signs a brand new autorun directory
 */
function createPackage(targetDir, scriptPath, message = "Run script on MPC?", askQuestion = true) {
    if (!fs.existsSync(scriptPath)) {
        console.error(`[-] Error: Script '${scriptPath}' does not exist.`);
        process.exit(1);
    }

    const autorunDir = path.join(targetDir, 'autorun');
    if (!fs.existsSync(autorunDir)) {
        fs.mkdirSync(autorunDir, { recursive: true });
    }

    const scriptFileName = path.basename(scriptPath);
    const destScriptPath = path.join(autorunDir, scriptFileName);
    fs.copyFileSync(scriptPath, destScriptPath);

    const manifest = {
        value0: {
            files: [],
            scripts: [
                {
                    askQuestion: Boolean(askQuestion),
                    path: scriptFileName,
                    message: message,
                    noButtonText: "no",
                    yesButtonText: "yes"
                }
            ]
        }
    };

    const jsonPath = path.join(autorunDir, 'autorun.json');
    fs.writeFileSync(jsonPath, JSON.stringify(manifest, null, 2), 'utf8');

    signAutorun(autorunDir);
    console.log(`\n[✓] Complete package created in: ${autorunDir}`);
    console.log(`    Copy the 'autorun' directory to the root of your USB drive or SD card.`);
}

// --- CLI Argument Parsing ---
function printHelp() {
    console.log(`
Akai MPC Standalone Autorun Signer & Hash Generator (v3.9.1)

Usage:
  node mpc_autorun_signer.js sign <path_to_autorun_dir>
  node mpc_autorun_signer.js verify <path_to_autorun_dir>
  node mpc_autorun_signer.js create <output_drive_dir> --script <path_to_sh> [--message "text"] [--no-ask]

Commands:
  sign    Calculates the SHA-256 + salt and generates/updates autorun.check
  verify  Validates an existing autorun directory against MPC firmware logic
  create  Scaffolds a ready-to-use autorun folder with script, json, and check file

Examples:
  node mpc_autorun_signer.js sign /media/usb/autorun
  node mpc_autorun_signer.js verify /media/usb/autorun
  node mpc_autorun_signer.js create /media/usb --script ./my_mod.sh --message "Install MPC Mod?"
`);
}

function main() {
    const args = process.argv.slice(2);
    const cmd = args[0];

    if (!cmd || cmd === '-h' || cmd === '--help') {
        printHelp();
        return;
    }

    try {
        switch (cmd) {
            case 'sign': {
                const targetDir = args[1] || '.';
                signAutorun(targetDir);
                break;
            }

            case 'verify': {
                const targetDir = args[1] || '.';
                const ok = verifyAutorun(targetDir);
                process.exit(ok ? 0 : 1);
                break;
            }

            case 'create': {
                const outDir = args[1];
                let scriptPath = null;
                let message = "Run script on MPC?";
                let ask = true;

                for (let i = 2; i < args.length; i++) {
                    if (args[i] === '--script' && args[i+1]) {
                        scriptPath = args[++i];
                    } else if (args[i] === '--message' && args[i+1]) {
                        message = args[++i];
                    } else if (args[i] === '--no-ask') {
                        ask = false;
                    }
                }

                if (!outDir || !scriptPath) {
                    console.error("[-] Error: 'create' requires destination directory and --script <file>");
                    process.exit(1);
                }

                createPackage(outDir, scriptPath, message, ask);
                break;
            }

            default:
                console.error(`[-] Unknown command: ${cmd}`);
                printHelp();
                process.exit(1);
        }
    } catch (err) {
        console.error(`[-] Error: ${err.message}`);
        process.exit(1);
    }
}

if (require.main === module) {
    main();
}
