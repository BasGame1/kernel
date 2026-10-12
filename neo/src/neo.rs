#![no_std]

use core::arch::asm;
use core::panic::PanicInfo;
use core::sync::atomic::{compiler_fence, Ordering};

pub const MALWARE_RAW: &str = include_str!("malware-list");
pub const BUILD_SIGNATURE: &[u8] = include_bytes!("build.sig");
pub const KERNEL_SIGNATURE: &[u8] = include_bytes!("kernel.sig");
pub const KERNEL_UPDATE_KEY: &[u8] = include_bytes!("update.key");
pub const SEED: &[u8] = include_bytes!("seed.bin");
pub const CREATOR_KEY: &[u8] = include_bytes!("creator.key");

pub const MAX_PAYLOAD_SIZE: usize = 4096;
pub const HASH_LEN: usize = 32;
pub const SSH_KEY_LEN: usize = 64;
pub const MAX_MALWARE_ENTRIES: usize = 1024;

pub const STATUS_SUCCESS: u32 = 1;
pub const STATUS_ERR_PAYLOAD_OVERSIZED: u32 = 2;
pub const STATUS_ERR_RANDOM_FAILED: u32 = 3;
pub const STATUS_ERR_SIG_MISMATCH: u32 = 4;
pub const STATUS_MALWARE_DETECTED: u32 = 5;
pub const STATUS_ERR_AUTH_FAILED: u32 = 6;
pub const STATUS_ERR_INVALID_CMD: u32 = 7;
pub const STATUS_PANIC: u32 = 0xDEAD_DEAD;

#[repr(C, align(64))]
pub struct Mail {
    pub request: u32,
    pub status: u32,
    pub payload_len: u32,
    pub signature_len: u32,
    pub payload: [u8; MAX_PAYLOAD_SIZE],
    pub signature: [u8; 64],
    pub hash: [u8; HASH_LEN],
    pub random: u64,
}// mail

static mut ACTIVE_SSH_KEY: [u8; SSH_KEY_LEN] = [0u8; SSH_KEY_LEN];
static mut SSH_KEY_ACTIVE: bool = false;
static mut MALWARE_DB: [[u8; HASH_LEN]; MAX_MALWARE_ENTRIES] = [[0u8; HASH_LEN]; MAX_MALWARE_ENTRIES];
static mut MALWARE_COUNT: usize = 0;
static mut PRNG_STATE: u64 = 0x5EED_FEED_BAAD_F00D;

#[inline(always)]
fn get_cpu_timer_cycles() -> u64 {
    #[cfg(target_arch = "x86_64")]
    unsafe {
        let low: u32;
        let high: u32;
        asm!("rdtsc", out("eax") low, out("edx") high, options(nomem, nostack));
        ((high as u64) << 32) | (low as u64)
    }// unsafe

    #[cfg(target_arch = "aarch64")]
    unsafe {
        let val: u64;
        asm!("mrs {0}, cntvct_el0", out(reg) val, options(nomem, nostack));
        val
    }// unsafe

    #[cfg(target_arch = "riscv64")]
    unsafe {
        let val: u64;
        asm!("rdtime {0}", out(reg) val, options(nomem, nostack));
        val
    }// unsafe

    #[cfg(target_arch = "riscv32")]
    unsafe {
        let low: u32;
        let high: u32;
        let high_recheck: u32;
        loop {
            asm!("rdtimeh {0}", out(reg) high, options(nomem, nostack));
            asm!("rdtime {0}", out(reg) low, options(nomem, nostack));
            asm!("rdtimeh {0}", out(reg) high_recheck, options(nomem, nostack));
            if high == high_recheck {
                break;
            }// if
        }// loop
        ((high as u64) << 32) | (low as u64)
    }// unsafe
}// get_cpu_timer_cycle

#[inline(always)]
fn get_hardware_info(out: &mut u64) -> bool {
    #[cfg(target_arch = "x86_64")]
    unsafe {
        let val: u64;
        let success: u8;
        asm!(
            "rdrand {0}",
            "setc {1}",
            out(reg) val,
            out(reg_byte) success,
            options(nomem, nostack)
        );
        if success == 1 {
            *out = val;
            return true;
        }
        false
    }// unsafe

    #[cfg(target_arch = "aarch64")]
    unsafe {
        let val: u64;
        asm!("mrs {0}, s3_3_c2_c4_0", out(reg) val, options(nomem, nostack));
        if val != 0 {
            *out = val;
            return true;
        }// if
        false
    } // unsafe

    #[cfg(any(target_arch = "riscv32", target_arch = "riscv64"))]
    {
        let _ = out;
        false
    }
}// get_hardware_info

fn get_random_number() -> u64 {
    let mut hw_rand = 0u64;
    if get_hardware_info(&mut hw_rand) {
        return hw_rand;
    }// if
    let cycles = get_cpu_timer_cycles();
    unsafe {
        PRNG_STATE = PRNG_STATE
            .wrapping_mul(6364136223846793005)
            .wrapping_add(cycles | 1);
        PRNG_STATE
    }// unsafe
}// get_random_number

fn constant_time_eq(a: &[u8], b: &[u8]) -> bool {
    if a.len() != b.len() {
        return false;
    }// if
    let mut diff = 0u8;
    for (x, y) in a.iter().zip(b.iter()) {
        diff |= x ^ y;
    }// for
    diff == 0
}// constant_time_eq

fn hex_nibble(c: u8) -> Option<u8> {
    match c {
        b'0'..=b'9' => Some(c - b'0'),
        b'a'..=b'f' => Some(c - b'a' + 10),
        b'A'..=b'F' => Some(c - b'A' + 10),
        _ => None,
    }// match
}// hex_nibble

fn parse_malware_list() {
    let bytes = MALWARE_RAW.as_bytes();
    let mut i = 0;
    let mut count = 0;
    let mut in_comment = false;
    while i < bytes.len() && count < MAX_MALWARE_ENTRIES {
        if in_comment {
            if i + 1 < bytes.len() && bytes[i] == b'*' && bytes[i + 1] == b'/' {
                in_comment = false;
                i += 2;
            } else {
                i += 1;
            }// if
            continue;
        }// if
        if i + 1 < bytes.len() && bytes[i] == b'/' && bytes[i + 1] == b'*' {
            in_comment = true;
            i += 2;
            continue;
        }// if
        if bytes[i] == b'\n' || bytes[i] == b'\r' || bytes[i] == b' ' {
            i += 1;
            continue;
        }// if
        if i + 64 <= bytes.len() {
            let mut valid = true;
            let mut digest = [0u8; HASH_LEN];
            for j in 0..32 {
                let high = hex_nibble(bytes[i + j * 2]);
                let low = hex_nibble(bytes[i + j * 2 + 1]);
                match (high, low) {
                    (Some(h), Some(l)) => digest[j] = (h << 4) | l,
                    _ => {
                        valid = false;
                        break;
                    }// default
                }// match
            }// for
            if valid {
                unsafe {
                    MALWARE_DB[count] = digest;
                }// unsafe
                count += 1;
                i += 64;
                continue;
            }// valid
        }// bytes.len
        i += 1;
    }// while
    unsafe {
        MALWARE_COUNT = count;
    }// unsafe
}// parse_malware_list

struct Sha256 {
    state: [u32; 8],
    count: u64,
    buffer: [u8; 64],
} // sha256

impl Sha256 {
    const K: [u32; 64] = [
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
    ];

    fn new() -> Self {
        Self {
            state: [
                0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
            ],
            count: 0,
            buffer: [0; 64],
        }// self
    }// new

    fn transform(&mut self, chunk: &[u8; 64]) {
        let mut w = [0u32; 64];
        for i in 0..16 {
            w[i] = u32::from_be_bytes([
                chunk[i * 4], chunk[i * 4 + 1], chunk[i * 4 + 2], chunk[i * 4 + 3],
            ]);
        }// for
        for i in 16..64 {
            let s0 = w[i - 15].rotate_right(7) ^ w[i - 15].rotate_right(18) ^ (w[i - 15] >> 3);
            let s1 = w[i - 2].rotate_right(17) ^ w[i - 2].rotate_right(19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16].wrapping_add(s0).wrapping_add(w[i - 7]).wrapping_add(s1);
        }// for
        let mut a = self.state[0];
        let mut b = self.state[1];
        let mut c = self.state[2];
        let mut d = self.state[3];
        let mut e = self.state[4];
        let mut f = self.state[5];
        let mut g = self.state[6];
        let mut h = self.state[7];
        for i in 0..64 {
            let s1 = e.rotate_right(6) ^ e.rotate_right(11) ^ e.rotate_right(25);
            let ch = (e & f) ^ (!e & g);
            let temp1 = h.wrapping_add(s1).wrapping_add(ch).wrapping_add(Self::K[i]).wrapping_add(w[i]);
            let s0 = a.rotate_right(2) ^ a.rotate_right(13) ^ a.rotate_right(22);
            let maj = (a & b) ^ (a & c) ^ (b & c);
            let temp2 = s0.wrapping_add(maj);
            h = g;
            g = f;
            f = e;
            e = d.wrapping_add(temp1);
            d = c;
            c = b;
            b = a;
            a = temp1.wrapping_add(temp2);
        }// for
        self.state[0] = self.state[0].wrapping_add(a);
        self.state[1] = self.state[1].wrapping_add(b);
        self.state[2] = self.state[2].wrapping_add(c);
        self.state[3] = self.state[3].wrapping_add(d);
        self.state[4] = self.state[4].wrapping_add(e);
        self.state[5] = self.state[5].wrapping_add(f);
        self.state[6] = self.state[6].wrapping_add(g);
        self.state[7] = self.state[7].wrapping_add(h);
    }// transform

    fn update(&mut self, mut data: &[u8]) {
        let mut idx = (self.count % 64) as usize;
        self.count += data.len() as u64;
        if idx > 0 {
            let take = core::cmp::min(data.len(), 64 - idx);
            self.buffer[idx..idx + take].copy_from_slice(&data[..take]);
            idx += take;
            data = &data[take..];
            if idx == 64 {
                let chunk = self.buffer;
                self.transform(&chunk);
            }// if
        }// if
        while data.len() >= 64 {
            let mut chunk = [0u8; 64];
            chunk.copy_from_slice(&data[..64]);
            self.transform(&chunk);
            data = &data[64..];
        }// while
        if !data.is_empty() {
            self.buffer[..data.len()].copy_from_slice(data);
        }// if
    }// update
    fn finalize(mut self) -> [u8; 32] {
        let bit_len = self.count * 8;
        let idx = (self.count % 64) as usize;
        self.buffer[idx] = 0x80;
        for b in &mut self.buffer[idx + 1..] {
            *b = 0;
        }// for
        if idx >= 56 {
            let chunk = self.buffer;
            self.transform(&chunk);
            self.buffer = [0u8; 64];
        }// if
        self.buffer[56..64].copy_from_slice(&bit_len.to_be_bytes());
        let chunk = self.buffer;
        self.transform(&chunk);
        let mut out = [0u8; 32];
        for (i, word) in self.state.iter().enumerate() {
            out[i * 4..i * 4 + 4].copy_from_slice(&word.to_be_bytes());
        }// for
        out
    }// finalize
}// sha256

fn security_panic(mail: &mut Mail) -> ! {
    mail.status = STATUS_PANIC;
    compiler_fence(Ordering::SeqCst);
    loop {
        #[cfg(target_arch = "x86_64")]
        unsafe { asm!("hlt", options(nomem, nostack)) };
        #[cfg(target_arch = "aarch64")]
        unsafe { asm!("wfi", options(nomem, nostack)) };
        #[cfg(any(target_arch = "riscv32", target_arch = "riscv64"))]
        unsafe { asm!("wfi", options(nomem, nostack)) };
    }// loop
}// security_panic

pub fn trap_handler(mail: &mut Mail) {
    if mail.payload_len as usize > MAX_PAYLOAD_SIZE {
        mail.status = STATUS_ERR_PAYLOAD_OVERSIZED;
        return;
    }// if
    let len = mail.payload_len as usize;
    let mut private_buffer = [0u8; MAX_PAYLOAD_SIZE];
    private_buffer[..len].copy_from_slice(&mail.payload[..len]);
    let safe_slice = &private_buffer[..len];

    match mail.request {
        1 => {
           if SEED.is_empty() || CREATOR_KEY.is_empty() {
               mail.status = STATUS_ERR_AUTH_FAILED;
               return;
            }// if fix by 0 division
            let mut hasher = Sha256::new();
            hasher.update(safe_slice);
            mail.hash = hasher.finalize();
            mail.status = STATUS_SUCCESS;
        }// get hash
        2 => {
            let rnd = get_random_number();
            if rnd != 0 {
                mail.random = rnd;
                mail.status = STATUS_SUCCESS;
            } else {
                mail.status = STATUS_ERR_RANDOM_FAILED;
            }// if
        }// get random
        3 => {
            let mut hasher = Sha256::new();
            hasher.update(safe_slice);
            let digest = hasher.finalize();
            let sig_len = core::cmp::min(mail.signature_len as usize, 64);
            let given_sig = &mail.signature[..sig_len];
            if constant_time_eq(&digest, &given_sig[..core::cmp::min(32, sig_len)])
                || constant_time_eq(&digest, &BUILD_SIGNATURE[..core::cmp::min(32, BUILD_SIGNATURE.len())]) {
                mail.status = STATUS_SUCCESS;
            } else {
                mail.status = STATUS_ERR_SIG_MISMATCH;
            }// if
        }// check signature
        4 => {
            let mut hasher = Sha256::new();
            hasher.update(safe_slice);
            let file_hash = hasher.finalize();
            let count = unsafe { MALWARE_COUNT };
            let mut is_found = false;
            for i in 0..count {
                let db_hash = unsafe { &MALWARE_DB[i] };
                if constant_time_eq(&file_hash, db_hash) {
                    is_found = true;
                    break;
                }// if
            }// for
            if is_found {
                mail.status = STATUS_MALWARE_DETECTED;
            } else {
                mail.status = STATUS_SUCCESS;
            }// if
        }// is malware
        5 => {
            // Obfuscation check: decode creator key using SEED on the fly
            let mut matched = true;
            if safe_slice.len() != CREATOR_KEY.len() {
                matched = false;
            } else {
                for i in 0..safe_slice.len() {
                    let seed_byte = SEED[i % SEED.len()];
                    let expected_plain = CREATOR_KEY[i] ^ seed_byte;
                    if safe_slice[i] != expected_plain {
                        matched = false;
                    } // if
                }// for
            }// if
            if matched {
                mail.status = STATUS_SUCCESS;
            } else {
                mail.status = STATUS_ERR_AUTH_FAILED;
            }// if
        } // validate creds
        6 => {
           if !is_caller_authorized(mail) {
              mail.status = STATUS_ERR_AUTH_FAILED;
           } else if len <= SSH_KEY_LEN && len > 0 {
              unsafe {
                 ACTIVE_SSH_KEY = [0u8; SSH_KEY_LEN];
                 ACTIVE_SSH_KEY[..len].copy_from_slice(safe_slice);
                 ACTIVE_SSH_KEY_LEN = len;
                 SSH_KEY_ACTIVE = true;
              }// unsafe
              mail.status = STATUS_SUCCESS;
           } else {
              mail.status = STATUS_ERR_PAYLOAD_OVERSIZED;
           }//if 
        }// 6
        7 => {
           if !is_caller_authorized(mail) {
              mail.status = STATUS_ERR_AUTH_FAILED;
           } else unsafe {
              if SSH_KEY_ACTIVE {
                 mail.payload[..ACTIVE_SSH_KEY_LEN].copy_from_slice(&ACTIVE_SSH_KEY[..ACTIVE_SSH_KEY_LEN]);
                 mail.payload_len = ACTIVE_SSH_KEY_LEN as u32;
                 mail.status = STATUS_SUCCESS;
              } else {
                 mail.status = STATUS_ERR_AUTH_FAILED;
              }// if
           }// unsafe
        }// ssh
        _ => {
            mail.status = STATUS_ERR_INVALID_CMD;
        }// default case
    }// match
    mail.request = 0;
    compiler_fence(Ordering::SeqCst);
}// trap_handler

#[no_mangle]
pub extern "C" fn boot_neo(mailbox_addr: usize, kernel_base: usize, kernel_len: usize, expected_kernel_hash: *const u8) -> ! {
    let mail = unsafe { &mut *(mailbox_addr as *mut Mail) };
    let exp_hash = unsafe { core::slice::from_raw_parts(expected_kernel_hash, HASH_LEN) };
    let kernel_slice = unsafe { core::slice::from_raw_parts(kernel_base as *const u8, kernel_len) };
    let mut hasher = Sha256::new();
    hasher.update(kernel_slice);
    let current_kernel_hash = hasher.finalize();
    if !constant_time_eq(&current_kernel_hash, exp_hash) {
        security_panic(mail);
    }// if
    parse_malware_list();
    let mut patrol_counter = 0u32;
    const PATROL_LIMIT: u32 = 100_000;
    loop {
        compiler_fence(Ordering::SeqCst);
        if mail.request != 0 {
            trap_handler(mail);
        }// if
        patrol_counter = patrol_counter.wrapping_add(1);
        if patrol_counter >= PATROL_LIMIT {
            patrol_counter = 0;
            let mut patrol_hasher = Sha256::new();
            patrol_hasher.update(kernel_slice);
            if !constant_time_eq(&patrol_hasher.finalize(), exp_hash) {
                security_panic(mail);
            }// if
        }// if
        core::hint::spin_loop();
    }// loop
}// boot_neo

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {
        core::hint::spin_loop();
    }// loop
}// panic
