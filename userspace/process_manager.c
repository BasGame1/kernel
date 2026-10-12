#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "../include/kernel.h"
#include "../include/userspace.h"
#include "../include/gid.h"
#include "../include/memory.h"

struct process processes[MAX_PROCESSES];
int current_pid = -1;

bool check_gid_permission(uint32_t sender_pid, uint32_t receiver_pid) {
   struct process *sender = get_process_by_pid(sender_pid);
   struct process *receiver = get_process_by_pid(receiver_pid);
   if (!sender || !receiver) {
      errorn("pid is not being used");
      return false;
   }// if
   if (sender->state == PROCESS_UNUSED || receiver->state == PROCESS_UNUSED) {
      errorn("pid is not in use");
      return false;
   }// if
   if (receiver->gid == SYSTEM_GID || sender->gid == SYSTEM_GID || receiver->gid == sender->gid) {
      successn("Receiver and sender are on the same gid or receiver is system");
      return true;
   } else if (receiver->gid == SYSTEM_GID || sender->gid == SYSTEM_GID || receiver->gid == sender->gid || receiver->gid == KEYBOARD_GID || (receiver->gid == VIRTIO_GID && sender->gid == FS_GID) || (receiver->gid == FS_GID && sender->gid == UPDATE_PROCESS_GID) || receiver->gid == UPDATE_PROCESS_GID || receiver->gid == ANTARTICA_GID || sender->gid == ANTARTICA_GID) {
       successn("Interaction is allowed due being an exception");
       return true;
    } else {
      errorn("Receiver and sender communication blocked due to different gid");
      return false;
   }// if
}// check_gid_permissions

uint32_t get_pid_by_gid(uint32_t gid) {
   for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
      if (processes[i].state != PROCESS_UNUSED && processes[i].gid == gid) {
         return processes[i].pid;
      }// if
   }// for
   return (uint32_t)-1;
}// get_pid_by_gid

struct process *get_process_by_pid(uint32_t pid) {
   for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
      if (processes[i].state != PROCESS_UNUSED && processes[i].pid == pid) {
         return &processes[i];
      }// if
   }// for
   return NULL;
}// get_process_by_pid

struct process *get_process_by_frame(void *frame) {
   for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
      if (processes[i].state != PROCESS_UNUSED && processes[i].frame == (struct trap_frame *)frame) {
         return &processes[i];
      }//if
   }// for
   return NULL;
}// get_process_by_frame

uint32_t get_free_pid(void) {
   static uint32_t next_pid = MIN_PID;
   for (uint32_t i = 0; i < (MAX_PID - MIN_PID); i++) {
      uint32_t candidate = next_pid++;
      if (next_pid >= MAX_PID) next_pid = MIN_PID;
      if (get_process_by_pid(candidate) == NULL) {
         return candidate;
      }// if
   }// for
   errorn("No free pids found for new process");
   return (uint32_t)-1;
}// get_free_pid

uint32_t create_process(void *entry, uint32_t gid) {
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
      if (user_stack_page) free_memory((uintptr_t *)user_stack_page);
      if (kernel_stack_page) free_memory((uintptr_t *)kernel_stack_page);
      return (uint32_t)-1;
   }// if
   uintptr_t user_stack = user_stack_page + 4096;
   uintptr_t kernel_stack = kernel_stack_page + 4096;
   extern uintptr_t *root_directory;
   if (root_directory) {
      map_virtual_memory(root_directory, user_stack_page, user_stack_page, (PTE_RWX | PTE_U));
   }
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
   struct process *current_proc = get_process_by_frame(current_frame);
   if (!current_proc && current_pid != -1) {
      current_proc = get_process_by_pid((uint32_t)current_pid);
   }// if

   if (current_proc && current_proc->state == PROCESS_RUNNING) {
      current_proc->frame = current_frame;
      current_proc->state = PROCESS_READY;
   }// if

   int start = 0;
   if (current_proc) {
      for (int i = 0; i < MAX_PROCESSES; i++) {
         if (&processes[i] == current_proc) {
            start = (i + 1) % MAX_PROCESSES;
            break;
         }// if
      }// for
   }// if

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
