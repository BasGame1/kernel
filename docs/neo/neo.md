# Neo

## Overview
**Neo** is an isolated box for security written in Rust. Its job is to provide some security apis while making sure of the intergrity of the kernel

> [!TIP]
> Neo can be compared to android's trusty, they maintain a similar function with the difference that trusty is just for the android OS and trusty is for the kernel of every OS

Neo is responsible for:
- Verify the kernel and itself on boot and real time
- Cryptographic signature validation for kernel updates.
- Create randomized numbers
- Validates is a file is malware (know hashes table)
- Managing SSH keys

---

## APIs

The request (mail) must have this format

- In rust
```rust
#[repr(C, align(64))]
pub struct Mail {
    pub request: u32, // action to do
    pub status: u32, // status to respond
    pub payload_len: u32, // lenght of the message
    pub signature_len: u32, // lenght of the signature
    pub payload: [u8; MAX_PAYLOAD_SIZE], // message
    pub signature: [u8; 64], // Signature
    pub hash: [u8; HASH_LEN], // 32-byte SHA-256 digest
    pub random: u64, // 64-bit random entropy generated
}
```
### method againt TOCTOU
To prevent Time-Of-Check to Time-Of-Use (TOCTOU) race conditions from supervisor code modifying the mailbox during cryptographic operations, Neo immediately snapshots request data into an isolated enclave buffer (`PRIVATE_BUFFER`) before processing.

---

## Measured Boot & Runtime Attestation Loop

### Measured Boot Verification
During bootstrap, `boot_neo()` calculates the SHA-256 hash of the entire kernel memory image (`kernel_base` to `kernel_base + kernel_len`) and validates it against `expected_kernel_hash`:
- Verification is performed using `constant_time_eq()` to prevent timing side channels.
- If the signature does not match, Neo invokes `security_panic()`:
  - Sets mailbox status to `STATUS_PANIC` (`0xDEAD_DEAD`).
  - Halts the CPU via `wfi` / `hlt` in an infinite loop.

### Runtime Integrity Patrol
Once the system is running, Neo executes a patrol loop:
- Every 100,000 spin cycles, Neo recomputes the SHA-256 digest of kernel text and read-only data sections.
- If runtime tampering, memory corruption, or rootkit hooks are detected, Neo immediately halts the machine.

---

## Mailbox Command Reference

| Opcode | Operation | Input Parameters | Output | Description |
|---|---|---|---|---|
| `1` | `GET_HASH` | `payload`, `payload_len` | `hash` | Computes SHA-256 digest of supplied buffer |
| `2` | `GET_RANDOM` | None | `random` | Returns 64 bits of hardware/timer mixed entropy |
| `3` | `VERIFY_UPDATE`| `payload`, `signature` | `status` | Authenticates update payload using `update.key` |
| `4` | `IS_MALWARE` | `payload`, `payload_len` | `status` | Checks SHA-256 against malware signature database |
| `5` | `AUTH_CREATOR` | `payload` | `status` | Validates creator credentials via `SEED ^ CREATOR_KEY` |
| `6` | `SET_SSH_KEY` | `payload` (SSH key) | `status` | Stores authorized SSH key (requires authentication) |
| `7` | `GET_SSH_KEY` | None | `payload` | Returns active SSH key (requires authentication) |

---

## Status Codes
- `1` (`STATUS_SUCCESS`): Operation completed successfully.
- `2` (`STATUS_ERR_PAYLOAD_OVERSIZED`): Payload exceeds `MAX_PAYLOAD_SIZE` (4096 bytes).
- `3` (`STATUS_ERR_RANDOM_FAILED`): Entropy generation failed.
- `4` (`STATUS_ERR_SIG_MISMATCH`): Cryptographic signature verification failed.
- `5` (`STATUS_MALWARE_DETECTED`): Payload hash matches known malware entry.
- `6` (`STATUS_ERR_AUTH_FAILED`): Authentication failed or command issued unauthenticated.
- `7` (`STATUS_ERR_INVALID_CMD`): Unrecognized opcode.
- `0xDEADDEAD` (`STATUS_PANIC`): Security integrity violation detected.

---

## Side-Channel Mitigation
All string and hash comparisons inside Neo use constant-time operations to avoid timing attacks

```rust
fn constant_time_eq(a: &[u8], b: &[u8]) -> bool {
    if a.len() != b.len() {
        return false;
    }
    let mut diff = 0u8;
    for (x, y) in a.iter().zip(b.iter()) {
        diff |= x ^ y;
    }
    diff == 0
}
```

---

## Bugs
- Neo's security is tested in source code with every little change in the code by AI powered bots to find security vulnerabilities, nevertheless, to however is reading this, if you find a bug please report it inmediately
