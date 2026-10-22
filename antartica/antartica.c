#include "../include/antartica.h"
#include "../include/gid.h"
#include "../include/memory.h"
#include "../include/kernel.h"
#include "../include/hardware.h"
#if defined(__X86__) || defined(__i386__) || defined(__x86_64__)
#include "../include/arch_x86.h"
#endif

linux_syscall_fn linux_syscall_table[LINUX_SYSCALL_COUNT];
#if defined(__ARM__) || defined(__arm__) || defined(__aarch64__)
static uintptr_t current_brk = 0x50000000;
#else
static uintptr_t current_brk = 0x40000000;
#endif
static uintptr_t user_tid_ptr = 0;
static uintptr_t mmap_alloc_base = 0x60000000;

#define MAX_ANTARTICA_FDS 64

#define FD_TYPE_UNUSED 0
#define FD_TYPE_TTY 1
#define FD_TYPE_FILE 2
#define FD_TYPE_DIR 3
#define FD_TYPE_PIPE 4

struct antartica_fd_entry {
   bool in_use;
   int type;
   uint32_t offset;
   uint32_t size;
   char path[128];
};

static struct antartica_fd_entry antartica_fds[MAX_ANTARTICA_FDS];
static char current_working_dir[128] = "/";

static inline void copy_str(char *dest, const char *src, size_t max) {
   if (!dest || max == 0) {
      return;
   }// if
   size_t i = 0;
   if (src) {
      while (src[i] && i + 1 < max) {
         dest[i] = src[i];
         i++;
      }// while
   }// if
   dest[i] = '\0';
}// copy_str

static inline bool is_user_address_range(uintptr_t addr, size_t len) {
   if (!addr || addr + len < addr) {
      return false;
   }// if
   uintptr_t end = addr + len;
#if defined(__x86_64__)
   if (addr < 0x200000ULL || end > 0x0000800000000000ULL) {
      return false;
   }// if
#elif defined(__64BIT__)
   if (end > 0x0000800000000000ULL) {
      return false;
   }// if
   if (addr < END_MEMORY && end > START_MEMORY) {
      return false;
   }// if
#else
   if (end > 0x80000000UL) {
      return false;
   }// if
   if (addr < END_MEMORY && end > START_MEMORY) {
      return false;
   }// if
#endif
   return true;
}// is_user_address_range

static void canonicalize_path(char *path) {
   if (!path || path[0] == '\0') {
      return;
   }// if
   char *src = path;
   char *dst = path;
   while (*src) {
      if (*src == '/' && src[1] == '.' && (src[2] == '/' || src[2] == '\0')) {
         src += 2;
         continue;
      }// if
      if (*src == '/' && src[1] == '.' && src[2] == '.' && (src[3] == '/' || src[3] == '\0')) {
         src += 3;
         while (dst > path && *(dst - 1) != '/') {
            dst--;
         }// while
         if (dst > path + 1 && *(dst - 1) == '/') {
            dst--;
         }// if
         continue;
      }// if
      if (*src == '/' && dst > path && *(dst - 1) == '/') {
         src++;
         continue;
      }// if
      *dst++ = *src++;
   }// while
   if (dst == path) {
      *dst++ = '/';
   }// if
   *dst = '\0';
}// canonicalize_path

static void resolve_full_path(long dfd, const char *user_path, char *out, size_t out_len) {
   if (!user_path || !out || out_len == 0) {
      if (out && out_len > 0) {
         out[0] = '\0';
      }// if
      return;
   }// if
   if (user_path[0] == '/') {
      copy_str(out, user_path, out_len);
      canonicalize_path(out);
      return;
   }// if
   const char *base = current_working_dir;
   if (dfd >= 0 && dfd < MAX_ANTARTICA_FDS && antartica_fds[dfd].in_use && antartica_fds[dfd].type == FD_TYPE_DIR) {
      base = antartica_fds[dfd].path;
   }// if
   size_t idx = 0;
   while (base[idx] && idx + 1 < out_len) {
      out[idx] = base[idx];
      idx++;
   }// while
   if (idx > 0 && out[idx - 1] != '/' && idx + 1 < out_len) {
      out[idx++] = '/';
   }// if
   size_t j = 0;
   while (user_path[j] && idx + 1 < out_len) {
      out[idx++] = user_path[j++];
   }// while
   out[idx] = '\0';
   canonicalize_path(out);
}// resolve_full_path

static void fill_linux_stat(uint32_t ino, const struct ext2_inode *ext_ino, struct linux_stat *st) {
   memset(st, 0, sizeof(struct linux_stat));
   st->st_dev = 1;
   st->st_ino = ino;
   st->st_mode = ext_ino->inode_mode;
   st->st_nlink = ext_ino->inode_links_count ? ext_ino->inode_links_count : 1;
   st->st_uid = ext_ino->inode_uid;
   st->st_gid = ext_ino->inode_gid;
   st->st_rdev = 0;
   st->st_size = ext_ino->inode_size;
   st->st_blksize = 1024;
   st->st_blocks = ext_ino->inode_blocks;
   st->st_atime = ext_ino->inode_atime;
   st->st_mtime = ext_ino->inode_mtime;
   st->st_ctime = ext_ino->inode_ctime;
}// fill_linux_stat

static void fill_tty_stat(struct linux_stat *st) {
   memset(st, 0, sizeof(struct linux_stat));
   st->st_dev = 5;
   st->st_ino = 1;
   st->st_mode = 0020666;
   st->st_nlink = 1;
   st->st_uid = 0;
   st->st_gid = 0;
   st->st_rdev = 0x8801;
   st->st_size = 0;
   st->st_blksize = 1024;
   st->st_blocks = 0;
}// fill_tty_stat

long sys_arch_prctl(long code, long addr, long a2, long a3, long a4, long a5) {
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
#if defined(__x86_64__)
   if (code == 0x1001) {
      if (!is_user_address_range((uintptr_t)addr, 8)) {
         return -LINUX_EFAULT;
      }// if
      uint32_t low = (uint32_t)(uintptr_t)addr;
      uint32_t high = (uint32_t)((uint64_t)(uintptr_t)addr >> 32);
      asm volatile("wrmsr" : : "c"(0xC0000101), "a"(low), "d"(high));
      return 0;
   }// if
   if (code == 0x1002) {
      if (!is_user_address_range((uintptr_t)addr, 8)) {
         return -LINUX_EFAULT;
      }// if
      uint32_t low = (uint32_t)(uintptr_t)addr;
      uint32_t high = (uint32_t)((uint64_t)(uintptr_t)addr >> 32);
      asm volatile("wrmsr" : : "c"(0xC0000100), "a"(low), "d"(high));
      return 0;
   }// if
   if (code == 0x1003) {
      if (!is_user_address_range((uintptr_t)addr, sizeof(uint64_t))) {
         return -LINUX_EFAULT;
      }// if
      uint32_t low, high;
      asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(0xC0000100));
      uint64_t val = ((uint64_t)high << 32) | low;
      *(uint64_t *)(uintptr_t)addr = val;
      return 0;
   }// if
   if (code == 0x1004) {
      if (!is_user_address_range((uintptr_t)addr, sizeof(uint64_t))) {
         return -LINUX_EFAULT;
      }// if
      uint32_t low, high;
      asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(0xC0000101));
      uint64_t val = ((uint64_t)high << 32) | low;
      *(uint64_t *)(uintptr_t)addr = val;
      return 0;
   }// if
#else
   (void)code;
   (void)addr;
#endif
   return -LINUX_EINVAL;
}// sys_arch_prctl

long sys_set_tid_address(long tidptr, long a1, long a2, long a3, long a4, long a5) {
   (void)a1;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   if (tidptr && !is_user_address_range((uintptr_t)tidptr, sizeof(int))) {
      return -LINUX_EFAULT;
   }// if
   user_tid_ptr = (uintptr_t)tidptr;
   return current_pid > 0 ? current_pid : 1;
}// sys_set_tid_address

long sys_brk(long brk_val, long a1, long a2, long a3, long a4, long a5) {
   (void)a1;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   if (brk_val == 0 || (uintptr_t)brk_val <= current_brk) {
      return (long)current_brk;
   }// if
   uintptr_t new_brk = (uintptr_t)brk_val;
   if (!is_user_address_range(new_brk, PAGE)) {
      return (long)current_brk;
   }// if
   uintptr_t start_page = (current_brk + PAGE - 1) & ~(PAGE - 1);
   uintptr_t end_page = (new_brk + PAGE - 1) & ~(PAGE - 1);
   if (end_page < start_page) {
      return (long)current_brk;
   }// if
   for (uintptr_t va = start_page; va < end_page; va += PAGE) {
      uintptr_t pa = allocate_memory();
      if (!pa) {
         return (long)current_brk;
      }// if
      map_virtual_memory(root_directory, va, pa, PTE_RW | PTE_U | PTE_V);
   }// for
   current_brk = new_brk;
   return (long)current_brk;
}// sys_brk

long sys_read(long fd, long buf, long count, long a3, long a4, long a5) {
   (void)a3;
   (void)a4;
   (void)a5;
   if (count == 0) {
      return 0;
   }// if
   if (count < 0 || (size_t)count > (1024 * 1024)) {
      return -LINUX_EINVAL;
   }// if
   if (!is_user_address_range((uintptr_t)buf, (size_t)count)) {
      return -LINUX_EFAULT;
   }// if
   if (fd < 0 || fd >= MAX_ANTARTICA_FDS || !antartica_fds[fd].in_use) {
      return -LINUX_EBADF;
   }// if
   if (antartica_fds[fd].type == FD_TYPE_DIR) {
      return -LINUX_EISDIR;
   }// if
   if (antartica_fds[fd].type == FD_TYPE_TTY) {
      char *dst = (char *)(uintptr_t)buf;
      long i = 0;
      while (i < count) {
         char ch = get_char();
         if (ch == 4) {
            return i;
         }// if
         if (ch == '\r') {
            ch = '\n';
         }// if
         dst[i++] = ch;
         char echo[2] = { ch, '\0' };
         print_character(echo);
         if (ch == '\n') {
            break;
         }// if
      }// while
      return i;
   }// if
   if (antartica_fds[fd].type == FD_TYPE_FILE) {
      int rd = ext2_read(antartica_fds[fd].path, (void *)(uintptr_t)buf, (uint32_t)count, antartica_fds[fd].offset);
      if (rd > 0) {
         antartica_fds[fd].offset += (uint32_t)rd;
         return rd;
      }// if
      return 0;
   }// if
   if (antartica_fds[fd].type == FD_TYPE_PIPE) {
      return 0;
   }// if
   return -LINUX_EBADF;
}// sys_read

long sys_pread64(long fd, long buf, long count, long offset, long a4, long a5) {
   (void)a4;
   (void)a5;
   if (count == 0) {
      return 0;
   }// if
   if (count < 0 || (size_t)count > (1024 * 1024) || offset < 0) {
      return -LINUX_EINVAL;
   }// if
   if (!is_user_address_range((uintptr_t)buf, (size_t)count)) {
      return -LINUX_EFAULT;
   }// if
   if (fd < 0 || fd >= MAX_ANTARTICA_FDS || !antartica_fds[fd].in_use) {
      return -LINUX_EBADF;
   }// if
   if (antartica_fds[fd].type == FD_TYPE_DIR) {
      return -LINUX_EISDIR;
   }// if
   if (antartica_fds[fd].type == FD_TYPE_FILE) {
      int rd = ext2_read(antartica_fds[fd].path, (void *)(uintptr_t)buf, (uint32_t)count, (uint32_t)offset);
      if (rd > 0) {
         return rd;
      }// if
      return 0;
   }// if
   return -LINUX_ESPIPE;
}// sys_pread64

long sys_write(long fd, long buf, long count, long a3, long a4, long a5) {
   (void)a3;
   (void)a4;
   (void)a5;
   if (count == 0) {
      return 0;
   }// if
   if (count < 0 || (size_t)count > (1024 * 1024)) {
      return -LINUX_EINVAL;
   }// if
   if (!is_user_address_range((uintptr_t)buf, (size_t)count)) {
      return -LINUX_EFAULT;
   }// if
   if (fd < 0 || fd >= MAX_ANTARTICA_FDS || !antartica_fds[fd].in_use) {
      return -LINUX_EBADF;
   }// if
   if (antartica_fds[fd].type == FD_TYPE_DIR) {
      return -LINUX_EISDIR;
   }// if
   if (antartica_fds[fd].type == FD_TYPE_TTY) {
      const char *str = (const char *)(uintptr_t)buf;
      for (long i = 0; i < count; i++) {
         char c[2] = { str[i], '\0' };
         print_character(c);
      }// for
      return count;
   }// if
   if (antartica_fds[fd].type == FD_TYPE_FILE) {
      int wr = ext2_write(antartica_fds[fd].path, (const void *)(uintptr_t)buf, (uint32_t)count, antartica_fds[fd].offset);
      if (wr > 0) {
         antartica_fds[fd].offset += (uint32_t)wr;
         if (antartica_fds[fd].offset > antartica_fds[fd].size) {
            antartica_fds[fd].size = antartica_fds[fd].offset;
         }// if
         return wr;
      }// if
      return 0;
   }// if
   if (antartica_fds[fd].type == FD_TYPE_PIPE) {
      return count;
   }// if
   return -LINUX_EBADF;
}// sys_write

long sys_writev(long fd, long vec, long vlen, long a3, long a4, long a5) {
   (void)a3;
   (void)a4;
   (void)a5;
   if (vlen == 0) {
      return 0;
   }// if
   if (vlen < 0 || vlen > 1024) {
      return -LINUX_EINVAL;
   }// if
   if (!is_user_address_range((uintptr_t)vec, sizeof(struct linux_iovec) * vlen)) {
      return -LINUX_EFAULT;
   }// if
   if (fd < 0 || fd >= MAX_ANTARTICA_FDS || !antartica_fds[fd].in_use) {
      return -LINUX_EBADF;
   }// if
   struct linux_iovec *iov = (struct linux_iovec *)(uintptr_t)vec;
   long total = 0;
   for (long i = 0; i < vlen; i++) {
      long res = sys_write(fd, (long)(uintptr_t)iov[i].iov_base, (long)iov[i].iov_len, 0, 0, 0);
      if (res < 0) {
         return res;
      }// if
      total += res;
   }// for
   return total;
}// sys_writev

long sys_ioctl(long fd, long request, long arg, long a3, long a4, long a5) {
   (void)fd;
   (void)a3;
   (void)a4;
   (void)a5;
   if (request == 0x5401) {
      if (!is_user_address_range((uintptr_t)arg, sizeof(struct linux_termios))) {
         return -LINUX_EFAULT;
      }// if
      struct linux_termios *term = (struct linux_termios *)(uintptr_t)arg;
      memset(term, 0, sizeof(struct linux_termios));
      term->c_iflag = 0x4500;
      term->c_oflag = 0x0005;
      term->c_cflag = 0x04bf;
      term->c_lflag = 0x8a3b;
      term->c_cc[0] = 3;
      term->c_cc[1] = 28;
      term->c_cc[2] = 127;
      term->c_cc[3] = 21;
      term->c_cc[4] = 4;
      term->c_ispeed = 38400;
      term->c_ospeed = 38400;
      return 0;
   }// if
   if (request == 0x5413) {
      if (!is_user_address_range((uintptr_t)arg, sizeof(struct linux_winsize))) {
         return -LINUX_EFAULT;
      }// if
      struct linux_winsize *ws = (struct linux_winsize *)(uintptr_t)arg;
      ws->ws_row = 24;
      ws->ws_col = 80;
      ws->ws_xpixel = 640;
      ws->ws_ypixel = 480;
      return 0;
   }// if
   if (request == 0x5402 || request == 0x5403 || request == 0x5404 || request == 0x5410 || request == 0x5414) {
      return 0;
   }// if
   if (request == 0x540F) {
      if (!is_user_address_range((uintptr_t)arg, sizeof(int))) {
         return -LINUX_EFAULT;
      }// if
      *(int *)(uintptr_t)arg = 1;
      return 0;
   }// if
   if (request == 0x541B) {
      if (!is_user_address_range((uintptr_t)arg, sizeof(int))) {
         return -LINUX_EFAULT;
      }// if
#if defined(__x86_64__) || defined(__i386__)
      *(int *)(uintptr_t)arg = (inb(COM1_PORT + 5) & 1) ? 1 : 0;
#else
      *(int *)(uintptr_t)arg = 0;
#endif
      return 0;
   }// if
   return -LINUX_ENOTTY;
}// sys_ioctl

long sys_close(long fd, long a1, long a2, long a3, long a4, long a5) {
   (void)a1;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   if (fd < 0 || fd >= MAX_ANTARTICA_FDS) {
      return -LINUX_EBADF;
   }// if
   antartica_fds[fd].in_use = false;
   antartica_fds[fd].type = FD_TYPE_UNUSED;
   antartica_fds[fd].offset = 0;
   antartica_fds[fd].size = 0;
   antartica_fds[fd].path[0] = '\0';
   return 0;
}// sys_close

long sys_exit(long status, long a1, long a2, long a3, long a4, long a5) {
   (void)status;
   (void)a1;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   infon("Linux process exit");
   struct process *curr = get_process_by_pid_fast((uint32_t)current_pid);
   if (curr) {
      curr->state = PROCESS_UNUSED;
   }// if
   while (1) {
#if defined(__x86_64__) || defined(__i386__)
      asm volatile("hlt");
#elif defined(__RISCV__) || defined(__riscv)
      asm volatile("wfi");
#else
      asm volatile("nop");
#endif
   }// while
   return 0;
}// sys_exit

long sys_exit_group(long status, long a1, long a2, long a3, long a4, long a5) {
   return sys_exit(status, a1, a2, a3, a4, a5);
}// sys_exit_group

long sys_getrandom(long buf, long buflen, long flags, long a3, long a4, long a5) {
   (void)flags;
   (void)a3;
   (void)a4;
   (void)a5;
   if (buflen <= 0) {
      return 0;
   }// if
   if (!is_user_address_range((uintptr_t)buf, (size_t)buflen)) {
      return -LINUX_EFAULT;
   }// if
   uint8_t *p = (uint8_t *)(uintptr_t)buf;
   for (long i = 0; i < buflen; i++) {
      p[i] = (uint8_t)(i ^ 0x5A);
   }// for
   return buflen;
}// sys_getrandom

long sys_mmap(long addr, long length, long prot, long flags, long fd, long offset) {
   (void)prot;
   (void)flags;
   (void)fd;
   (void)offset;
   if (length <= 0 || (size_t)length > (1024ULL * 1024ULL * 1024ULL)) {
      return -LINUX_EINVAL;
   }// if
   if (addr && ((uintptr_t)addr & (PAGE - 1)) != 0) {
      return -LINUX_EINVAL;
   }// if
   size_t aligned_len = (length + PAGE - 1) & ~(PAGE - 1);
   if (aligned_len < (size_t)length || aligned_len == 0) {
      return -LINUX_EINVAL;
   }// if
   uintptr_t map_addr = addr ? (uintptr_t)addr : mmap_alloc_base;
   if (!is_user_address_range(map_addr, aligned_len)) {
      return -LINUX_ENOMEM;
   }// if
   for (uintptr_t va = map_addr; va < map_addr + aligned_len; va += PAGE) {
      uintptr_t pa = allocate_memory();
      if (!pa) {
         return -LINUX_ENOMEM;
      }// if
      memset((void *)pa, 0, PAGE);
      map_virtual_memory(root_directory, va, pa, PTE_RW | PTE_U | PTE_V);
   }// for
   if (!(flags & 0x20) && fd >= 0 && fd < MAX_ANTARTICA_FDS && antartica_fds[fd].in_use && antartica_fds[fd].type == FD_TYPE_FILE && offset >= 0) {
      uint32_t to_read = (uint32_t)length;
      if ((uint32_t)offset < antartica_fds[fd].size) {
         uint32_t avail = antartica_fds[fd].size - (uint32_t)offset;
         if (to_read > avail) {
            to_read = avail;
         }// if
         ext2_read(antartica_fds[fd].path, (void *)map_addr, to_read, (uint32_t)offset);
      }// if
   }// if
   if (!addr) {
      mmap_alloc_base += aligned_len;
   }// if
   return (long)map_addr;
}// sys_mmap

long sys_munmap(long addr, long length, long a2, long a3, long a4, long a5) {
   (void)addr;
   (void)length;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   return 0;
}// sys_munmap

long sys_mprotect(long addr, long len, long prot, long a3, long a4, long a5) {
   (void)prot;
   (void)a3;
   (void)a4;
   (void)a5;
   if (len < 0 || !is_user_address_range((uintptr_t)addr, (size_t)len)) {
      return -LINUX_EFAULT;
   }// if
   return 0;
}// sys_mprotect

struct rlimit64 {
   uint64_t rlim_cur;
   uint64_t rlim_max;
};

long sys_prlimit64(long pid, long resource, long new_limit, long old_limit, long a4, long a5) {
   (void)pid;
   (void)new_limit;
   (void)a4;
   (void)a5;
   if (old_limit) {
      if (!is_user_address_range((uintptr_t)old_limit, sizeof(struct rlimit64))) {
         return -LINUX_EFAULT;
      }// if
      struct rlimit64 *lim = (struct rlimit64 *)(uintptr_t)old_limit;
      if (resource == 3) {
         lim->rlim_cur = 8 * 1024 * 1024;
         lim->rlim_max = 8 * 1024 * 1024;
      } else if (resource == 7) {
         lim->rlim_cur = 1024;
         lim->rlim_max = 4096;
      } else {
         lim->rlim_cur = ~0ULL;
         lim->rlim_max = ~0ULL;
      }// else
   }// if
   return 0;
}// sys_prlimit64

long sys_set_robust_list(long head, long len, long a2, long a3, long a4, long a5) {
   (void)head;
   (void)len;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   return 0;
}// sys_set_robust_list

long sys_uname(long buf, long a1, long a2, long a3, long a4, long a5) {
   (void)a1;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   if (!is_user_address_range((uintptr_t)buf, sizeof(struct linux_utsname))) {
      return -LINUX_EFAULT;
   }// if
   struct linux_utsname *u = (struct linux_utsname *)(uintptr_t)buf;
   memset(u, 0, sizeof(struct linux_utsname));
   memcpy(u->sysname, "Linux", 6);
#if defined(__FOR_ARCH__)
   memcpy(u->nodename, "archlinux", 10);
#elif defined(__FOR_DEBIAN__)
   memcpy(u->nodename, "debian", 7);
#elif defined(__FOR_UBUNTU__)
   memcpy(u->nodename, "ubuntu", 7);
#elif defined(__FOR_VOID__)
   memcpy(u->nodename, "void", 5);
#elif defined(__FOR_MANJARO__)
   memcpy(u->nodename, "manjaro", 8);
#elif defined(__FOR_FEDORA__)
   memcpy(u->nodename, "fedora", 7);
#elif defined(__FOR_OPENSUSE__)
   memcpy(u->nodename, "opensuse", 9);
#elif defined(__FOR_ROCKY__)
   memcpy(u->nodename, "rocky", 6);
#elif defined(__FOR_ALPINE__)
   memcpy(u->nodename, "alpine", 7);
#else
   memcpy(u->nodename, "antartica", 10);
#endif
   memcpy(u->release, "6.1.0", 6);
   memcpy(u->version, "Antartica", 10);
#if defined(__x86_64__) || defined(__64BIT__)
   memcpy(u->machine, "x86_64", 7);
#elif defined(__i386__) || defined(__X86__)
   memcpy(u->machine, "i686", 5);
#elif defined(__riscv) || defined(__RISCV__)
#if defined(__64BIT__) || defined(__riscv64)
   memcpy(u->machine, "riscv64", 8);
#else
   memcpy(u->machine, "riscv32", 8);
#endif
#else
   memcpy(u->machine, "unknown", 8);
#endif
   memcpy(u->domainname, "localdomain", 12);
   return 0;
}// sys_uname

long sys_getuid(long a0, long a1, long a2, long a3, long a4, long a5) {
   (void)a0;
   (void)a1;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   return 0;
}// sys_getuid

long sys_geteuid(long a0, long a1, long a2, long a3, long a4, long a5) {
   (void)a0;
   (void)a1;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   return 0;
}// sys_geteuid

long sys_getgid(long a0, long a1, long a2, long a3, long a4, long a5) {
   (void)a0;
   (void)a1;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   return 0;
}// sys_getgid

long sys_getegid(long a0, long a1, long a2, long a3, long a4, long a5) {
   (void)a0;
   (void)a1;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   return 0;
}// sys_getegid

long sys_getpid(long a0, long a1, long a2, long a3, long a4, long a5) {
   (void)a0;
   (void)a1;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   return current_pid > 0 ? current_pid : 1;
}// sys_getpid

long sys_getppid(long a0, long a1, long a2, long a3, long a4, long a5) {
   (void)a0;
   (void)a1;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   return 0;
}// sys_getppid

#if defined(__64BIT__) || defined(__x86_64__) || defined(__aarch64__) || defined(__riscv64)
#define SIGACTION_SIZE 32
#else
#define SIGACTION_SIZE 20
#endif

long sys_rt_sigaction(long signum, long act, long oldact, long sigsetsize, long a4, long a5) {
   (void)signum;
   (void)a4;
   (void)a5;
   if (sigsetsize != 8) {
      return -LINUX_EINVAL;
   }// if
   if (act && !is_user_address_range((uintptr_t)act, SIGACTION_SIZE)) {
      return -LINUX_EFAULT;
   }// if
   if (oldact && !is_user_address_range((uintptr_t)oldact, SIGACTION_SIZE)) {
      return -LINUX_EFAULT;
   }// if
   if (oldact) {
      memset((void *)(uintptr_t)oldact, 0, SIGACTION_SIZE);
   }// if
   return 0;
}// sys_rt_sigaction

long sys_rt_sigprocmask(long how, long set, long oldset, long sigsetsize, long a4, long a5) {
   (void)how;
   (void)a4;
   (void)a5;
   if (sigsetsize != 8) {
      return -LINUX_EINVAL;
   }// if
   if (set && !is_user_address_range((uintptr_t)set, 8)) {
      return -LINUX_EFAULT;
   }// if
   if (oldset && !is_user_address_range((uintptr_t)oldset, 8)) {
      return -LINUX_EFAULT;
   }// if
   if (oldset) {
      memset((void *)(uintptr_t)oldset, 0, 8);
   }// if
   return 0;
}// sys_rt_sigprocmask

static uint32_t resolve_symlink_ino(char *full_path, struct ext2_inode *inode_out) {
   uint32_t ino = ext2_resolve_path(full_path);
   if (ino == 0) {
      return 0;
   }// if
   if (ext2_read_inode(ino, inode_out) != 0) {
      return 0;
   }// if
   int hops = 0;
   while ((inode_out->inode_mode & 0xF000) == 0xA000 && hops++ < 8) {
      char target[128];
      size_t tlen = inode_out->inode_size < 127 ? inode_out->inode_size : 127;
      if (inode_out->inode_size <= 60) {
         memcpy(target, inode_out->inode_block, tlen);
      } else {
         ext2_read(full_path, (void *)target, (uint32_t)tlen, 0);
      }// else
      target[tlen] = '\0';
      char next_path[128];
      if (target[0] == '/') {
         copy_str(next_path, target, sizeof(next_path));
      } else {
         copy_str(next_path, full_path, sizeof(next_path));
         int last_slash = -1;
         for (int j = 0; next_path[j]; j++) {
            if (next_path[j] == '/') {
               last_slash = j;
            }// if
         }// for
         if (last_slash >= 0) {
            next_path[last_slash + 1] = '\0';
         } else {
            next_path[0] = '\0';
         }// else
         size_t nlen = strlen(next_path);
         copy_str(next_path + nlen, target, sizeof(next_path) - nlen);
      }// else
      canonicalize_path(next_path);
      uint32_t next_ino = ext2_resolve_path(next_path);
      if (next_ino == 0 || ext2_read_inode(next_ino, inode_out) != 0) {
         break;
      }// if
      ino = next_ino;
      copy_str(full_path, next_path, 128);
   }// while
   if ((inode_out->inode_mode & 0xF000) == 0xA000) {
      return 0;
   }// if
   return ino;
}// resolve_symlink_ino

long sys_openat(long dfd, long filename, long flags, long mode, long a4, long a5) {
   (void)mode;
   (void)a4;
   (void)a5;
   if (!filename || !is_user_address_range((uintptr_t)filename, 1)) {
      return -LINUX_EFAULT;
   }// if
   char full_path[128];
   resolve_full_path(dfd, (const char *)(uintptr_t)filename, full_path, sizeof(full_path));
   if (full_path[0] == '\0') {
      return -LINUX_ENOENT;
   }// if
   if (strcmp(full_path, "/dev/null") == 0 || strcmp(full_path, "/dev/tty") == 0 || strcmp(full_path, "/dev/console") == 0) {
      for (int i = 3; i < MAX_ANTARTICA_FDS; i++) {
         if (!antartica_fds[i].in_use) {
            antartica_fds[i].in_use = true;
            antartica_fds[i].type = FD_TYPE_TTY;
            antartica_fds[i].offset = 0;
            antartica_fds[i].size = 0;
            copy_str(antartica_fds[i].path, full_path, sizeof(antartica_fds[i].path));
            return i;
         }// if
      }// for
      return -LINUX_EMFILE;
   }// if
   struct ext2_inode inode;
   uint32_t ino = resolve_symlink_ino(full_path, &inode);
   if (ino == 0) {
      if (flags & 0x40) {
         ino = ext2_create_file(full_path);
         if (ino == 0) {
            return -LINUX_ENOENT;
         }// if
         if (ext2_read_inode(ino, &inode) != 0) {
            return -LINUX_EIO;
         }// if
      } else {
         return -LINUX_ENOENT;
      }// else
   }// if
   bool is_dir = (inode.inode_mode & 0xF000) == 0x4000;
   for (int i = 3; i < MAX_ANTARTICA_FDS; i++) {
      if (!antartica_fds[i].in_use) {
         antartica_fds[i].in_use = true;
         antartica_fds[i].type = is_dir ? FD_TYPE_DIR : FD_TYPE_FILE;
         antartica_fds[i].offset = 0;
         antartica_fds[i].size = inode.inode_size;
         copy_str(antartica_fds[i].path, full_path, sizeof(antartica_fds[i].path));
         return i;
      }// if
   }// for
   return -LINUX_EMFILE;
}// sys_openat

long sys_open(long filename, long flags, long mode, long a3, long a4, long a5) {
   (void)a3;
   (void)a4;
   (void)a5;
   return sys_openat(-100, filename, flags, mode, 0, 0);
}// sys_open

long sys_fstat(long fd, long statbuf, long a2, long a3, long a4, long a5) {
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   if (fd < 0 || fd >= MAX_ANTARTICA_FDS || !antartica_fds[fd].in_use) {
      return -LINUX_EBADF;
   }// if
   if (!statbuf || !is_user_address_range((uintptr_t)statbuf, sizeof(struct linux_stat))) {
      return -LINUX_EFAULT;
   }// if
   if (antartica_fds[fd].type == FD_TYPE_TTY) {
      fill_tty_stat((struct linux_stat *)(uintptr_t)statbuf);
      return 0;
   }// if
   uint32_t ino = ext2_resolve_path(antartica_fds[fd].path);
   if (ino == 0) {
      return -LINUX_ENOENT;
   }// if
   struct ext2_inode inode;
   if (ext2_read_inode(ino, &inode) != 0) {
      return -LINUX_EIO;
   }// if
   fill_linux_stat(ino, &inode, (struct linux_stat *)(uintptr_t)statbuf);
   return 0;
}// sys_fstat

long sys_newfstatat(long dfd, long filename, long statbuf, long flag, long a4, long a5) {
   (void)flag;
   (void)a4;
   (void)a5;
   if (!statbuf || !is_user_address_range((uintptr_t)statbuf, sizeof(struct linux_stat))) {
      return -LINUX_EFAULT;
   }// if
   if (!filename) {
      return sys_fstat(dfd, statbuf, 0, 0, 0, 0);
   }// if
   if (!is_user_address_range((uintptr_t)filename, 1)) {
      return -LINUX_EFAULT;
   }// if
   const char *fn = (const char *)(uintptr_t)filename;
   if (fn[0] == '\0') {
      return sys_fstat(dfd, statbuf, 0, 0, 0, 0);
   }// if
   char full_path[128];
   resolve_full_path(dfd, fn, full_path, sizeof(full_path));
   if (strcmp(full_path, "/dev/null") == 0 || strcmp(full_path, "/dev/tty") == 0 || strcmp(full_path, "/dev/console") == 0) {
      fill_tty_stat((struct linux_stat *)(uintptr_t)statbuf);
      return 0;
   }// if
   uint32_t ino = 0;
   struct ext2_inode inode;
   if (!(flag & 0x100)) {
      ino = resolve_symlink_ino(full_path, &inode);
   } else {
      ino = ext2_resolve_path(full_path);
      if (ino != 0 && ext2_read_inode(ino, &inode) != 0) {
         ino = 0;
      }// if
   }// else
   if (ino == 0) {
      return -LINUX_ENOENT;
   }// if
   fill_linux_stat(ino, &inode, (struct linux_stat *)(uintptr_t)statbuf);
   return 0;
}// sys_newfstatat

long sys_stat(long filename, long statbuf, long a2, long a3, long a4, long a5) {
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   return sys_newfstatat(-100, filename, statbuf, 0, 0, 0);
}// sys_stat

long sys_faccessat(long dfd, long filename, long mode, long a3, long a4, long a5) {
   (void)a3;
   (void)a4;
   (void)a5;
   if (!filename || !is_user_address_range((uintptr_t)filename, 1)) {
      return -LINUX_EFAULT;
   }// if
   char full_path[128];
   resolve_full_path(dfd, (const char *)(uintptr_t)filename, full_path, sizeof(full_path));
   if (strcmp(full_path, "/dev/null") == 0 || strcmp(full_path, "/dev/tty") == 0 || strcmp(full_path, "/dev/console") == 0) {
      return 0;
   }// if
   struct ext2_inode inode;
   uint32_t ino = resolve_symlink_ino(full_path, &inode);
   if (ino == 0) {
      return -LINUX_ENOENT;
   }// if
   if ((mode & 1) && !(inode.inode_mode & 0111)) {
      return -LINUX_EACCES;
   }// if
   return 0;
}// sys_faccessat

long sys_access(long filename, long mode, long a2, long a3, long a4, long a5) {
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   return sys_faccessat(-100, filename, mode, 0, 0, 0);
}// sys_access

long sys_getcwd(long buf, long size, long a2, long a3, long a4, long a5) {
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   if (size <= 0) {
      return -LINUX_EINVAL;
   }// if
   size_t len = strlen(current_working_dir) + 1;
   if ((size_t)size < len) {
      return -LINUX_ERANGE;
   }// if
   if (!is_user_address_range((uintptr_t)buf, len)) {
      return -LINUX_EFAULT;
   }// if
   memcpy((void *)(uintptr_t)buf, current_working_dir, len);
   return (long)len;
}// sys_getcwd

long sys_chdir(long filename, long a1, long a2, long a3, long a4, long a5) {
   (void)a1;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   if (!filename || !is_user_address_range((uintptr_t)filename, 1)) {
      return -LINUX_EFAULT;
   }// if
   char full_path[128];
   resolve_full_path(-100, (const char *)(uintptr_t)filename, full_path, sizeof(full_path));
   uint32_t ino = ext2_resolve_path(full_path);
   if (ino == 0) {
      return -LINUX_ENOENT;
   }// if
   struct ext2_inode inode;
   if (ext2_read_inode(ino, &inode) != 0) {
      return -LINUX_EIO;
   }// if
   if ((inode.inode_mode & 0xF000) != 0x4000) {
      return -LINUX_ENOTDIR;
   }// if
   copy_str(current_working_dir, full_path, sizeof(current_working_dir));
   return 0;
}// sys_chdir

long sys_lseek(long fd, long offset, long whence, long a3, long a4, long a5) {
   (void)a3;
   (void)a4;
   (void)a5;
   if (fd < 0 || fd >= MAX_ANTARTICA_FDS || !antartica_fds[fd].in_use) {
      return -LINUX_EBADF;
   }// if
   if (antartica_fds[fd].type == FD_TYPE_TTY) {
      return -LINUX_ESPIPE;
   }// if
   int64_t new_off = 0;
   if (whence == 0) {
      new_off = offset;
   } else if (whence == 1) {
      new_off = (int64_t)antartica_fds[fd].offset + offset;
   } else if (whence == 2) {
      new_off = (int64_t)antartica_fds[fd].size + offset;
   } else {
      return -LINUX_EINVAL;
   }// else
   if (new_off < 0) {
      return -LINUX_EINVAL;
   }// if
   antartica_fds[fd].offset = (uint32_t)new_off;
   return (long)new_off;
}// sys_lseek

long sys_dup(long oldfd, long a1, long a2, long a3, long a4, long a5) {
   (void)a1;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   if (oldfd < 0 || oldfd >= MAX_ANTARTICA_FDS || !antartica_fds[oldfd].in_use) {
      return -LINUX_EBADF;
   }// if
   for (int i = 0; i < MAX_ANTARTICA_FDS; i++) {
      if (!antartica_fds[i].in_use) {
         antartica_fds[i] = antartica_fds[oldfd];
         return i;
      }// if
   }// for
   return -LINUX_EMFILE;
}// sys_dup

long sys_dup2(long oldfd, long newfd, long a2, long a3, long a4, long a5) {
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   if (oldfd < 0 || oldfd >= MAX_ANTARTICA_FDS || !antartica_fds[oldfd].in_use) {
      return -LINUX_EBADF;
   }// if
   if (newfd < 0 || newfd >= MAX_ANTARTICA_FDS) {
      return -LINUX_EBADF;
   }// if
   if (oldfd == newfd) {
      return newfd;
   }// if
   antartica_fds[newfd] = antartica_fds[oldfd];
   return newfd;
}// sys_dup2

long sys_dup3(long oldfd, long newfd, long flags, long a3, long a4, long a5) {
   (void)flags;
   (void)a3;
   (void)a4;
   (void)a5;
   if (oldfd == newfd) {
      return -LINUX_EINVAL;
   }// if
   return sys_dup2(oldfd, newfd, 0, 0, 0, 0);
}// sys_dup3

long sys_fcntl(long fd, long cmd, long arg, long a3, long a4, long a5) {
   (void)a3;
   (void)a4;
   (void)a5;
   if (fd < 0 || fd >= MAX_ANTARTICA_FDS || !antartica_fds[fd].in_use) {
      return -LINUX_EBADF;
   }// if
   if (cmd == 0 || cmd == 1030) {
      int start = arg >= 0 ? (int)arg : 0;
      for (int i = start; i < MAX_ANTARTICA_FDS; i++) {
         if (!antartica_fds[i].in_use) {
            antartica_fds[i] = antartica_fds[fd];
            return i;
         }// if
      }// for
      return -LINUX_EMFILE;
   }// if
   if (cmd == 1 || cmd == 2 || cmd == 3 || cmd == 4) {
      return 0;
   }// if
   return 0;
}// sys_fcntl

long sys_poll(long ufds, long nfds, long timeout_msecs, long a3, long a4, long a5) {
   (void)timeout_msecs;
   (void)a3;
   (void)a4;
   (void)a5;
   if (nfds == 0) {
      return 0;
   }// if
   if (nfds < 0 || nfds > 1024) {
      return -LINUX_EINVAL;
   }// if
   if (!is_user_address_range((uintptr_t)ufds, sizeof(struct linux_pollfd) * (size_t)nfds)) {
      return -LINUX_EFAULT;
   }// if
   struct linux_pollfd *fds = (struct linux_pollfd *)(uintptr_t)ufds;
   long ready = 0;
   for (long i = 0; i < nfds; i++) {
      fds[i].revents = 0;
      int fd = fds[i].fd;
      if (fd >= 0 && fd < MAX_ANTARTICA_FDS && antartica_fds[fd].in_use) {
         if (antartica_fds[fd].type == FD_TYPE_TTY) {
#if defined(__x86_64__) || defined(__i386__)
            if ((fds[i].events & 0x0001) && (inb(COM1_PORT + 5) & 1)) {
               fds[i].revents |= 0x0001;
               ready++;
            }// if
#endif
            if (fds[i].events & 0x0004) {
               fds[i].revents |= 0x0004;
               ready++;
            }// if
         } else {
            if (fds[i].events & 0x0001) {
               fds[i].revents |= 0x0001;
               ready++;
            }// if
            if (fds[i].events & 0x0004) {
               fds[i].revents |= 0x0004;
               ready++;
            }// if
         }// else
      }// if
   }// for
   return ready;
}// sys_poll

long sys_ppoll(long ufds, long nfds, long tsp, long sigmask, long sigsetsize, long a5) {
   (void)tsp;
   (void)sigmask;
   (void)sigsetsize;
   (void)a5;
   return sys_poll(ufds, nfds, 0, 0, 0, 0);
}// sys_ppoll

long sys_pipe2(long fildes, long flags, long a2, long a3, long a4, long a5) {
   (void)flags;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   if (!fildes || !is_user_address_range((uintptr_t)fildes, sizeof(int) * 2)) {
      return -LINUX_EFAULT;
   }// if
   int fds_found[2] = { -1, -1 };
   int count = 0;
   for (int i = 3; i < MAX_ANTARTICA_FDS && count < 2; i++) {
      if (!antartica_fds[i].in_use) {
         fds_found[count++] = i;
      }// if
   }// for
   if (count < 2) {
      return -LINUX_EMFILE;
   }// if
   for (int i = 0; i < 2; i++) {
      int fd = fds_found[i];
      antartica_fds[fd].in_use = true;
      antartica_fds[fd].type = FD_TYPE_PIPE;
      antartica_fds[fd].offset = 0;
      antartica_fds[fd].size = 0;
      copy_str(antartica_fds[fd].path, "/dev/pipe", sizeof(antartica_fds[fd].path));
   }// for
   int *user_fds = (int *)(uintptr_t)fildes;
   user_fds[0] = fds_found[0];
   user_fds[1] = fds_found[1];
   return 0;
}// sys_pipe2

long sys_pipe(long fildes, long a1, long a2, long a3, long a4, long a5) {
   (void)a1;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   return sys_pipe2(fildes, 0, 0, 0, 0, 0);
}// sys_pipe

long sys_readlinkat(long dfd, long path, long buf, long bufsiz, long a4, long a5) {
   (void)a4;
   (void)a5;
   if (bufsiz <= 0) {
      return -LINUX_EINVAL;
   }// if
   if (!path || !is_user_address_range((uintptr_t)path, 1)) {
      return -LINUX_EFAULT;
   }// if
   if (!buf || !is_user_address_range((uintptr_t)buf, (size_t)bufsiz)) {
      return -LINUX_EFAULT;
   }// if
   char full_path[128];
   resolve_full_path(dfd, (const char *)(uintptr_t)path, full_path, sizeof(full_path));
   if (strcmp(full_path, "/proc/self/exe") == 0) {
      const char *self_exe = "/bin/busybox";
      size_t len = strlen(self_exe);
      if (len > (size_t)bufsiz) {
         len = (size_t)bufsiz;
      }// if
      memcpy((void *)(uintptr_t)buf, self_exe, len);
      return (long)len;
   }// if
   uint32_t ino = ext2_resolve_path(full_path);
   if (ino == 0) {
      return -LINUX_ENOENT;
   }// if
   struct ext2_inode inode;
   if (ext2_read_inode(ino, &inode) != 0) {
      return -LINUX_EIO;
   }// if
   if ((inode.inode_mode & 0xF000) != 0xA000) {
      return -LINUX_EINVAL;
   }// if
   size_t len = inode.inode_size;
   if (len > (size_t)bufsiz) {
      len = (size_t)bufsiz;
   }// if
   if (inode.inode_size <= 60) {
      memcpy((void *)(uintptr_t)buf, inode.inode_block, len);
   } else {
      ext2_read(full_path, (void *)(uintptr_t)buf, (uint32_t)len, 0);
   }// else
   return (long)len;
}// sys_readlinkat

long sys_readlink(long path, long buf, long bufsiz, long a3, long a4, long a5) {
   (void)a3;
   (void)a4;
   (void)a5;
   return sys_readlinkat(-100, path, buf, bufsiz, 0, 0);
}// sys_readlink

long sys_sysinfo(long info, long a1, long a2, long a3, long a4, long a5) {
   (void)a1;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   if (!info || !is_user_address_range((uintptr_t)info, sizeof(struct linux_sysinfo))) {
      return -LINUX_EFAULT;
   }// if
   struct linux_sysinfo *si = (struct linux_sysinfo *)(uintptr_t)info;
   memset(si, 0, sizeof(struct linux_sysinfo));
   si->uptime = 100;
   si->totalram = 256 * 1024 * 1024;
   si->freeram = 128 * 1024 * 1024;
   si->procs = 1;
   si->mem_unit = 1;
   return 0;
}// sys_sysinfo

long sys_clock_gettime(long which_clock, long tp, long a2, long a3, long a4, long a5) {
   (void)which_clock;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   if (!tp || !is_user_address_range((uintptr_t)tp, sizeof(struct linux_timespec))) {
      return -LINUX_EFAULT;
   }// if
   struct linux_timespec *ts = (struct linux_timespec *)(uintptr_t)tp;
   ts->tv_sec = 1700000000;
   ts->tv_nsec = 0;
   return 0;
}// sys_clock_gettime

long sys_gettimeofday(long tv, long tz, long a2, long a3, long a4, long a5) {
   (void)tz;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   if (tv) {
      if (!is_user_address_range((uintptr_t)tv, sizeof(struct linux_timeval))) {
         return -LINUX_EFAULT;
      }// if
      struct linux_timeval *t = (struct linux_timeval *)(uintptr_t)tv;
      t->tv_sec = 1700000000;
      t->tv_usec = 0;
   }// if
   return 0;
}// sys_gettimeofday

long sys_time(long tloc, long a1, long a2, long a3, long a4, long a5) {
   (void)a1;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   if (tloc && is_user_address_range((uintptr_t)tloc, sizeof(long))) {
      *(long *)(uintptr_t)tloc = 1700000000;
   }// if
   return 1700000000;
}// sys_time

long sys_setpgid(long pid, long pgid, long a2, long a3, long a4, long a5) {
   (void)pid;
   (void)pgid;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   return 0;
}// sys_setpgid

long sys_getpgrp(long a0, long a1, long a2, long a3, long a4, long a5) {
   (void)a0;
   (void)a1;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   return 1;
}// sys_getpgrp

long sys_getpgid(long pid, long a1, long a2, long a3, long a4, long a5) {
   (void)pid;
   (void)a1;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   return 1;
}// sys_getpgid

long sys_kill(long pid, long sig, long a2, long a3, long a4, long a5) {
   (void)pid;
   (void)sig;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   return 0;
}// sys_kill

long sys_execve(long filename, long argv, long envp, long a3, long a4, long a5) {
   (void)envp;
   (void)a3;
   (void)a4;
   (void)a5;
   if (!filename || !is_user_address_range((uintptr_t)filename, 1)) {
      return -LINUX_EFAULT;
   }// if
   char full_path[128];
   resolve_full_path(-100, (const char *)(uintptr_t)filename, full_path, sizeof(full_path));
   struct ext2_inode inode;
   uint32_t ino = resolve_symlink_ino(full_path, &inode);
   if (ino == 0) {
      return -LINUX_ENOENT;
   }// if
   if (!(inode.inode_mode & 0111)) {
      return -LINUX_EACCES;
   }// if
   static char kargv_storage[16][128];
   static char *kargv[17];
   int argc = 0;
   if (argv && is_user_address_range((uintptr_t)argv, sizeof(uintptr_t))) {
      uintptr_t *uargv = (uintptr_t *)(uintptr_t)argv;
      while (argc < 16) {
         if (!is_user_address_range((uintptr_t)&uargv[argc], sizeof(uintptr_t)) || uargv[argc] == 0) {
            break;
         }// if
         if (is_user_address_range(uargv[argc], 1)) {
            copy_str(kargv_storage[argc], (const char *)uargv[argc], 128);
            kargv[argc] = kargv_storage[argc];
            argc++;
         } else {
            break;
         }// else
      }// while
   }// if
   kargv[argc] = NULL;
   extern uintptr_t live_elf_start;
   extern size_t live_elf_len;
   if (live_elf_start && live_elf_len > 0) {
#if defined(__x86_64__)
      const void *elf_ptr = (const void *)(0xFFFF800000000000ULL + live_elf_start);
#else
      const void *elf_ptr = (const void *)live_elf_start;
#endif
      antartica_elf_exec(elf_ptr, live_elf_len, kargv, 0);
      return 0;
   }// if
   return -LINUX_ENOEXEC;
}// sys_execve

long sys_setsid(long a0, long a1, long a2, long a3, long a4, long a5) {
   (void)a0;
   (void)a1;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   return 1;
}// sys_setsid

long sys_getsid(long pid, long a1, long a2, long a3, long a4, long a5) {
   (void)pid;
   (void)a1;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   return 1;
}// sys_getsid

long sys_sigaltstack(long ss, long oss, long a2, long a3, long a4, long a5) {
   (void)ss;
   (void)oss;
   (void)a2;
   (void)a3;
   (void)a4;
   (void)a5;
   return 0;
}// sys_sigaltstack

long sys_wait4(long upid, long stat_addr, long options, long ru, long a4, long a5) {
   (void)upid;
   (void)stat_addr;
   (void)options;
   (void)ru;
   (void)a4;
   (void)a5;
   return -LINUX_ECHILD;
}// sys_wait4

long sys_getdents64(long fd, long dirent, long count, long a3, long a4, long a5) {
   (void)a3;
   (void)a4;
   (void)a5;
   if (count <= 0) {
      return -LINUX_EINVAL;
   }// if
   if (fd < 0 || fd >= MAX_ANTARTICA_FDS || !antartica_fds[fd].in_use) {
      return -LINUX_EBADF;
   }// if
   if (antartica_fds[fd].type != FD_TYPE_DIR) {
      return -LINUX_ENOTDIR;
   }// if
   if (!dirent || !is_user_address_range((uintptr_t)dirent, (size_t)count)) {
      return -LINUX_EFAULT;
   }// if
   uint32_t offset = antartica_fds[fd].offset;
   if (offset >= antartica_fds[fd].size) {
      return 0;
   }// if
   static uint8_t dir_buf[1024];
   int rd = ext2_read(antartica_fds[fd].path, dir_buf, sizeof(dir_buf), offset);
   if (rd <= 0) {
      return 0;
   }// if
   uint32_t buf_pos = 0;
   uint32_t written = 0;
   while (buf_pos + 8 <= (uint32_t)rd) {
      struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(dir_buf + buf_pos);
      if (entry->rec_len < 8 || buf_pos + entry->rec_len > (uint32_t)rd) {
         break;
      }// if
      uint32_t next_offset = offset + buf_pos + entry->rec_len;
      if (entry->inode != 0) {
         uint16_t name_len = entry->name_len;
         uint16_t reclen = (uint16_t)((19 + name_len + 1 + 7) & ~7);
         if (written + reclen > (uint32_t)count) {
            break;
         }// if
         struct linux_dirent64 *d = (struct linux_dirent64 *)((uintptr_t)dirent + written);
         d->d_ino = entry->inode;
         d->d_off = next_offset;
         d->d_reclen = reclen;
         uint8_t dtype = 0;
         if (entry->file_type == 1) {
            dtype = 8;
         } else if (entry->file_type == 2) {
            dtype = 4;
         } else if (entry->file_type == 3) {
            dtype = 2;
         } else if (entry->file_type == 4) {
            dtype = 6;
         } else if (entry->file_type == 5) {
            dtype = 1;
         } else if (entry->file_type == 6) {
            dtype = 12;
         } else if (entry->file_type == 7) {
            dtype = 10;
         }// else if
         d->d_type = dtype;
         memcpy(d->d_name, entry->name, name_len);
         d->d_name[name_len] = '\0';
         for (size_t p = 19 + name_len + 1; p < reclen; p++) {
            ((char *)d)[p] = 0;
         }// for
         written += reclen;
      }// if
      buf_pos += entry->rec_len;
      antartica_fds[fd].offset = next_offset;
   }// while
   if (written == 0 && buf_pos == 0) {
      return -LINUX_EINVAL;
   }// if
   return (long)written;
}// sys_getdents64

long antartica_x86_syscall(long sc, long a0, long a1, long a2, long a3, long a4, long a5) {
   if ((unsigned long)sc >= LINUX_SYSCALL_COUNT) {
      return -LINUX_ENOSYS;
   }// if
   linux_syscall_fn handler = linux_syscall_table[sc];
   if (!handler) {
      return -LINUX_ENOSYS;
   }// if
   return handler(a0, a1, a2, a3, a4, a5);
}// antartica_x86_syscall

void antartica_init(void) {
   infon("Init antartica");
   for (int i = 0; i < LINUX_SYSCALL_COUNT; i++) {
      linux_syscall_table[i] = NULL;
   }// for
   for (int i = 0; i < 3; i++) {
      antartica_fds[i].in_use = true;
      antartica_fds[i].type = FD_TYPE_TTY;
      antartica_fds[i].offset = 0;
      antartica_fds[i].size = 0;
      copy_str(antartica_fds[i].path, "/dev/tty", sizeof(antartica_fds[i].path));
   }// for
   for (int i = 3; i < MAX_ANTARTICA_FDS; i++) {
      antartica_fds[i].in_use = false;
      antartica_fds[i].type = FD_TYPE_UNUSED;
      antartica_fds[i].offset = 0;
      antartica_fds[i].size = 0;
      antartica_fds[i].path[0] = '\0';
   }// for
#if (defined(__X86__) || defined(__i386__) || defined(__x86_64__)) && (defined(__64BIT__) || !defined(__32BIT__))
   linux_syscall_table[0] = sys_read;
   linux_syscall_table[1] = sys_write;
   linux_syscall_table[2] = sys_open;
   linux_syscall_table[3] = sys_close;
   linux_syscall_table[4] = sys_stat;
   linux_syscall_table[5] = sys_fstat;
   linux_syscall_table[6] = sys_stat;
   linux_syscall_table[7] = sys_poll;
   linux_syscall_table[8] = sys_lseek;
   linux_syscall_table[9] = sys_mmap;
   linux_syscall_table[10] = sys_mprotect;
   linux_syscall_table[11] = sys_munmap;
   linux_syscall_table[12] = sys_brk;
   linux_syscall_table[13] = sys_rt_sigaction;
   linux_syscall_table[14] = sys_rt_sigprocmask;
   linux_syscall_table[16] = sys_ioctl;
   linux_syscall_table[17] = sys_pread64;
   linux_syscall_table[20] = sys_writev;
   linux_syscall_table[21] = sys_access;
   linux_syscall_table[22] = sys_pipe;
   linux_syscall_table[32] = sys_dup;
   linux_syscall_table[33] = sys_dup2;
   linux_syscall_table[39] = sys_getpid;
   linux_syscall_table[59] = sys_execve;
   linux_syscall_table[60] = sys_exit;
   linux_syscall_table[61] = sys_wait4;
   linux_syscall_table[62] = sys_kill;
   linux_syscall_table[63] = sys_uname;
   linux_syscall_table[72] = sys_fcntl;
   linux_syscall_table[79] = sys_getcwd;
   linux_syscall_table[80] = sys_chdir;
   linux_syscall_table[89] = sys_readlink;
   linux_syscall_table[96] = sys_gettimeofday;
   linux_syscall_table[99] = sys_sysinfo;
   linux_syscall_table[102] = sys_getuid;
   linux_syscall_table[104] = sys_geteuid;
   linux_syscall_table[107] = sys_getgid;
   linux_syscall_table[108] = sys_getegid;
   linux_syscall_table[109] = sys_setpgid;
   linux_syscall_table[110] = sys_getppid;
   linux_syscall_table[111] = sys_getpgrp;
   linux_syscall_table[112] = sys_setsid;
   linux_syscall_table[121] = sys_getpgid;
   linux_syscall_table[124] = sys_getsid;
   linux_syscall_table[131] = sys_sigaltstack;
   linux_syscall_table[158] = sys_arch_prctl;
   linux_syscall_table[186] = sys_getpid;
   linux_syscall_table[201] = sys_time;
   linux_syscall_table[217] = sys_getdents64;
   linux_syscall_table[218] = sys_set_tid_address;
   linux_syscall_table[228] = sys_clock_gettime;
   linux_syscall_table[231] = sys_exit_group;
   linux_syscall_table[257] = sys_openat;
   linux_syscall_table[262] = sys_newfstatat;
   linux_syscall_table[267] = sys_readlinkat;
   linux_syscall_table[269] = sys_faccessat;
   linux_syscall_table[271] = sys_ppoll;
   linux_syscall_table[273] = sys_set_robust_list;
   linux_syscall_table[292] = sys_dup3;
   linux_syscall_table[293] = sys_pipe2;
   linux_syscall_table[302] = sys_prlimit64;
   linux_syscall_table[318] = sys_getrandom;
#elif defined(__X86__) || defined(__i386__)
   linux_syscall_table[1] = sys_exit;
   linux_syscall_table[3] = sys_read;
   linux_syscall_table[4] = sys_write;
   linux_syscall_table[6] = sys_close;
   linux_syscall_table[20] = sys_getpid;
   linux_syscall_table[24] = sys_getuid;
   linux_syscall_table[45] = sys_brk;
   linux_syscall_table[47] = sys_getgid;
   linux_syscall_table[49] = sys_geteuid;
   linux_syscall_table[50] = sys_getegid;
   linux_syscall_table[54] = sys_ioctl;
   linux_syscall_table[64] = sys_getppid;
   linux_syscall_table[90] = sys_mmap;
   linux_syscall_table[91] = sys_munmap;
   linux_syscall_table[122] = sys_uname;
   linux_syscall_table[125] = sys_mprotect;
   linux_syscall_table[146] = sys_writev;
   linux_syscall_table[174] = sys_rt_sigaction;
   linux_syscall_table[175] = sys_rt_sigprocmask;
   linux_syscall_table[224] = sys_getpid;
   linux_syscall_table[252] = sys_exit_group;
   linux_syscall_table[258] = sys_set_tid_address;
   linux_syscall_table[355] = sys_getrandom;
#else
   linux_syscall_table[29] = sys_ioctl;
   linux_syscall_table[57] = sys_close;
   linux_syscall_table[63] = sys_read;
   linux_syscall_table[64] = sys_write;
   linux_syscall_table[66] = sys_writev;
   linux_syscall_table[93] = sys_exit;
   linux_syscall_table[94] = sys_exit_group;
   linux_syscall_table[96] = sys_set_tid_address;
   linux_syscall_table[134] = sys_rt_sigaction;
   linux_syscall_table[135] = sys_rt_sigprocmask;
   linux_syscall_table[160] = sys_uname;
   linux_syscall_table[172] = sys_getpid;
   linux_syscall_table[173] = sys_getppid;
   linux_syscall_table[174] = sys_getuid;
   linux_syscall_table[175] = sys_geteuid;
   linux_syscall_table[176] = sys_getgid;
   linux_syscall_table[177] = sys_getegid;
   linux_syscall_table[178] = sys_getpid;
   linux_syscall_table[214] = sys_brk;
   linux_syscall_table[215] = sys_munmap;
   linux_syscall_table[222] = sys_mmap;
   linux_syscall_table[226] = sys_mprotect;
   linux_syscall_table[278] = sys_getrandom;
#endif
   successn("Antartica initialized with 467 syscall entry table");
#if defined(__FOR_ARCH__)
   infon("Antartica target: Arch Linux");
#elif defined(__FOR_DEBIAN__)
   infon("Antartica target: Debian");
#elif defined(__FOR_UBUNTU__)
   infon("Antartica target: Ubuntu");
#elif defined(__FOR_VOID__)
   infon("Antartica target: Void Linux");
#elif defined(__FOR_MANJARO__)
   infon("Antartica target: Manjaro Linux");
#elif defined(__FOR_FEDORA__)
   infon("Antartica target: Fedora");
#elif defined(__FOR_OPENSUSE__)
   infon("Antartica target: openSUSE");
#elif defined(__FOR_ROCKY__)
   infon("Antartica target: Rocky Linux");
#elif defined(__FOR_ALPINE__)
   infon("Antartica target: Alpine Linux");
#endif
}// antartica_init

void antartica_process(void) {
   infon("antartica running");
   while (1) {
      struct antartica_ipc_req req;
      uint32_t sender = user_ipc_recv(0, &req);
      if (sender != (uint32_t)-1) {
         long ret = -LINUX_ENOSYS;
         if (req.syscall_nr < LINUX_SYSCALL_COUNT) {
            linux_syscall_fn handler = linux_syscall_table[req.syscall_nr];
            if (handler) {
               ret = handler(req.args[0], req.args[1], req.args[2], req.args[3], req.args[4], req.args[5]);
            }// if
         }// if
         struct antartica_ipc_resp resp;
         memset(&resp, 0, sizeof(resp));
         resp.status = ret;
         user_ipc_send(sender, &resp);
      }// if
   }// while
}// antartica_process

void create_antartica_process(void) {
   antartica_init();
   infon("starting antartica");
   uint32_t pid = create_process((void *)antartica_process, ANTARTICA_GID);
   struct process *proc = get_process_by_pid(pid);
   if (proc) {
      uint32_t gid = proc->gid;
      uint32_t sid = generate_sid(gid, (uintptr_t)antartica_process);
      proc->sid = sid;
   }// if
   successn("antartica created");
}// create_antartica_process

long antartica_syscall_dispatch(struct trap_frame *frame) {
   uint32_t sc = (uint32_t)frame->a7;
   if (sc >= LINUX_SYSCALL_COUNT) {
      errorn("antartica invalid syscall index");
      frame->a0 = (uintptr_t)-LINUX_ENOSYS;
      return -LINUX_ENOSYS;
   }// if

   linux_syscall_fn handler = linux_syscall_table[sc];
   if (!handler) {
      frame->a0 = (uintptr_t)-LINUX_ENOSYS;
      return -LINUX_ENOSYS;
   }// if

   long ret = handler(frame->a0, frame->a1, frame->a2, frame->a3, frame->a4, frame->a5);
   frame->a0 = (uintptr_t)ret;
   return ret;
}// antartica_syscall_dispatch
