#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "../include/kernel.h"
#include "../include/userspace.h"
#include "../include/gid.h"
#include "../include/memory.h"

#ifndef PTE_RW
   #define PTE_RW 0x06
#endif

struct process processes[MAX_PROCESSES];
int current_pid = -1;
int current_process_idx = -1;
int direct_handoff_idx = -1;
uint8_t pid_to_slot[PID_SLOT_RANGE];
struct gid_cache_entry gid_cache = { .gid = 0, .pid = 0, .valid = false };

static void init_pid_slot_table(void) {
   for (uint32_t i = 0; i < PID_SLOT_RANGE; i++) {
      pid_to_slot[i] = PID_SLOT_UNUSED;
   }// for
}// init_pid_slot_table

static inline void register_pid_slot(uint32_t pid, uint8_t slot) {
   if (pid >= MIN_PID && pid < MAX_PID) {
      pid_to_slot[pid - MIN_PID] = slot;
   }// if
}// register_pid_slott

static inline void unregister_pid_slot(uint32_t pid) {
   if (pid >= MIN_PID && pid < MAX_PID) {
      pid_to_slot[pid - MIN_PID] = PID_SLOT_UNUSED;
   }
}

bool check_gid_permission(uint32_t sender_pid, uint32_t receiver_pid) {
   struct process *sender = get_process_by_pid_fast(sender_pid);
   struct process *receiver = get_process_by_pid_fast(receiver_pid);
   if (!sender || !receiver) {
      errorn("pid is not being used");
      return false;
   }// if
   if (sender->state == PROCESS_UNUSED || receiver->state == PROCESS_UNUSED) {
      errorn("pid is not in use");
      return false;
   }// if
#ifndef LINUX_MODULE_GID
   #define LINUX_MODULE_GID 16
#endif
   if (receiver->gid == sender->gid || receiver->gid == SYSTEM_GID || sender->gid == SYSTEM_GID) {
      return true;
   } else if ((receiver->gid == KEYBOARD_GID && sender->gid == SYSTEM_GID) || 
         (receiver->gid == VIRTIO_GID && sender->gid == FS_GID) || 
         (receiver->gid == FS_GID && sender->gid == UPDATE_PROCESS_GID) || 
         (sender->gid == FS_GID && receiver->gid == UPDATE_PROCESS_GID) || 
         receiver->gid == ANTARTICA_GID || 
         sender->gid == ANTARTICA_GID || 
         receiver->gid == LINUX_MODULE_GID || 
         sender->gid == LINUX_MODULE_GID) {
      return true;
   } else {
      errorn("Receiver and sender communication blocked due to different gid");
      return false;
   }// if
   return false;
}// check_gid_permission

uint32_t get_pid_by_gid(uint32_t gid) {
   if (gid_cache.valid && gid_cache.gid == gid) {
      struct process *cached = get_process_by_pid_fast(gid_cache.pid);
      if (cached && cached->gid == gid && cached->state != PROCESS_UNUSED) {
         return gid_cache.pid;
      }// if
      gid_cache.valid = false;
   }// if

   for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
      if (processes[i].state != PROCESS_UNUSED && processes[i].gid == gid) {
         gid_cache.gid = gid;
         gid_cache.pid = processes[i].pid;
         gid_cache.valid = true;
         return processes[i].pid;
      }// if
   }// for
   return (uint32_t)-1;
}// get_pid_by_gid

struct process *get_process_by_pid(uint32_t pid) {
   return get_process_by_pid_fast(pid);
}// get_process_by_pid

struct process *get_process_by_frame(void *frame) {
   if (!frame) {
      return NULL;
   }// if
   for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
      if (processes[i].state != PROCESS_UNUSED && processes[i].frame == (struct trap_frame *)frame) {
         return &processes[i];
      }// if
   }// for
   return NULL;
}// get_process_by_frame

uint32_t get_free_pid(void) {
   static uint32_t next_pid = MIN_PID;
   for (uint32_t i = 0; i < (MAX_PID - MIN_PID); i++) {
      uint32_t candidate = next_pid++;
      if (next_pid >= MAX_PID) {
         next_pid = MIN_PID;
      }// if
      if (pid_to_slot[candidate - MIN_PID] == PID_SLOT_UNUSED) {
         return candidate;
      }// if
   }// for
   errorn("No free pids found for new process");
   return (uint32_t)-1;
}// get_free_pid

uint32_t create_process(void *entry, uint32_t gid) {
   static bool pid_table_initialized = false;
   if (!pid_table_initialized) {
      init_pid_slot_table();
      pid_table_initialized = true;
   }

   int unoccupied_process = -1;
   for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
      if (processes[i].state == PROCESS_UNUSED) {
         unoccupied_process = (int)i;
         break;
      }// if
   }// for
   if (unoccupied_process == -1) {
      errorn("All process slots are occupied");
      return (uint32_t)-1;
   }// if

   uint32_t free_pid = get_free_pid();
   if (free_pid == (uint32_t)-1) {
      errorn("Cant create new process due to lack of pids");
      return (uint32_t)-1;
   }// if

   uintptr_t user_stack_page = allocate_memory();
   uintptr_t kernel_stack_page = allocate_memory();
   if (!user_stack_page || !kernel_stack_page) {
      errorn("Cant allocate stack pages for new process");
      if (user_stack_page) {
         free_memory((uintptr_t *)user_stack_page);
      }// if
      if (kernel_stack_page) {
         free_memory((uintptr_t *)kernel_stack_page);
      }// if
      return (uint32_t)-1;
   }// if
   uintptr_t user_stack = user_stack_page + 4096;
   uintptr_t kernel_stack = kernel_stack_page + 4096;
   extern uintptr_t *root_directory;
   if (root_directory) {
      map_virtual_memory(root_directory, user_stack_page, user_stack_page, (PTE_RW | PTE_U));
   }// if
   struct trap_frame *frame = (struct trap_frame *)(kernel_stack - sizeof(struct trap_frame));
   memset(frame, 0, sizeof(struct trap_frame));
   frame->mepc = (uintptr_t)entry;
   frame->sp = user_stack;
   
   processes[unoccupied_process].sid = generate_sid(gid, (uintptr_t)entry);
   processes[unoccupied_process].pid = free_pid;
   processes[unoccupied_process].gid = gid;
   processes[unoccupied_process].state = PROCESS_READY;
   processes[unoccupied_process].kernel_stack = kernel_stack;
   processes[unoccupied_process].user_stack = user_stack;
   processes[unoccupied_process].frame = frame;
   processes[unoccupied_process].ipc_target_pid = 0;
   processes[unoccupied_process].ipc_msg_buf = NULL;

   register_pid_slot(free_pid, (uint8_t)unoccupied_process);
   invalidate_gid_cache();

   success("Created new process with pid: ");
   print_dec(free_pid);
   printn("");
   return free_pid;
}// create_process

uint32_t get_same_status_pid(uint32_t pid, uint32_t state) {
   for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
      if (processes[i].state == (p_state_t)state && processes[i].pid != pid) {
         return processes[i].pid;
      }// if
   }// for
   return (uint32_t)-1;
}// get_same_status_pid

struct trap_frame *schedule(struct trap_frame *current_frame) {
   struct process *current_proc = NULL;

   if (current_process_idx >= 0 && current_process_idx < MAX_PROCESSES) {
      current_proc = &processes[current_process_idx];
      /* Verify the index is still valid */
      if (current_proc->state == PROCESS_UNUSED || (int)current_proc->pid != current_pid) {
         current_proc = NULL;
         current_process_idx = -1;
      }// if
   }// if
   if (!current_proc && current_pid != -1) {
      current_proc = get_process_by_frame(current_frame);
      if (!current_proc) {
         current_proc = get_process_by_pid_fast((uint32_t)current_pid);
      }
      /* Recover the index */
      if (current_proc) {
         for (int i = 0; i < MAX_PROCESSES; i++) {
            if (&processes[i] == current_proc) {
               current_process_idx = i;
               break;
            }// if
         }// for
      }// if
   }// if

   if (current_proc && current_proc->state == PROCESS_RUNNING) {
      current_proc->frame = current_frame;
      current_proc->state = PROCESS_READY;
   }// if
   if (direct_handoff_idx >= 0 && direct_handoff_idx < MAX_PROCESSES) {
      int h_idx = direct_handoff_idx;
      direct_handoff_idx = -1;
      if (processes[h_idx].state == PROCESS_READY) {
         processes[h_idx].state = PROCESS_RUNNING;
         current_pid = (int)processes[h_idx].pid;
         current_process_idx = h_idx;
#if defined(__RISCV__) || defined(__riscv)
         asm volatile("csrw mscratch, %0" :: "r"(processes[h_idx].kernel_stack));
#endif
         return processes[h_idx].frame;
      }// if
   }// if
   int start = (current_process_idx >= 0) ? ((current_process_idx + 1) % MAX_PROCESSES) : 0;

   int next_idx = -1;
   for (int i = 0; i < MAX_PROCESSES; i++) {
      int idx = (start + i) % MAX_PROCESSES;
      if (processes[idx].state == PROCESS_READY) {
         next_idx = idx;
         break;
      }// if
   }// for

   if (next_idx != -1) {
      processes[next_idx].state = PROCESS_RUNNING;
      current_pid = (int)processes[next_idx].pid;
      current_process_idx = next_idx;
#if defined(__RISCV__) || defined(__riscv)
      asm volatile("csrw mscratch, %0" :: "r"(processes[next_idx].kernel_stack));
#endif
      return processes[next_idx].frame;
   }// if

   if (current_proc && current_proc->state != PROCESS_BLOCKED_RECV && current_proc->state != PROCESS_BLOCKED_SEND) {
      current_proc->state = PROCESS_RUNNING;
   }// if
   return current_frame;
}// schedule
