#include <stdint.h>
#include <stdbool.h>

#include "../include/kernel.h"
#include "../include/userspace.h"
#include "../include/gid.h"

#ifdef __USE_AB__
   #include "../include/ab_update.h"
#else
   #ifdef __USE_AB_NO_REBOOT__
      #include "../include/abnr_update.h"
   #else
      #warning("No AB or ABRN selected, updates will be normal");
      #include "../include/noab_update.h"
   #endif
#endif

bool start_update(const char *update_file);
char get_active_slot(void);
void flip_active_slot(void);
bool get_slot_status(void);
void init_slot_status(void);

int apply_patch(uint8_t *base_buf, uint32_t base_size, const uint8_t *patch_buf, uint32_t patch_size) {
   if (patch_size < sizeof(struct patch_header)) {
      errorn("Patch file too small");
      return -1;
   }// if
   const struct patch_header *hdr = (const struct patch_header *)patch_buf;
   if (hdr->magic != PATCH_MAGIC) {
      errorn("Invalid patch magic");
      return -1;
   }// if
   if (hdr->old_size != base_size) {
      errorn("Patch base size mismatch");
      return -1;
   }// if
   const uint8_t *ptr = patch_buf + sizeof(struct patch_header);
   for (uint32_t i = 0; i < hdr->chunk_count; i++) {
      uint32_t offset = *(const uint32_t *)ptr; ptr += 4;
      uint32_t len = *(const uint32_t *)ptr; ptr += 4;
      memcpy(base_buf + offset, ptr, len);
      ptr += len;
   }// for
   return (int)hdr->new_size;
}// apply_patch

void init_slot_status(void) {
   char buf[4] = {0};
   int res = read("/boot/slotstatus", buf, 2, 0);
   if (res <= 0) {
      infon("First boot: initializing /boot/slotstatus to a0");
      write("/boot/slotstatus", "a0", 2, 0);
   }// if
}// init_slot_status

char get_active_slot(void) {
   char buf[4] = {0};
   int res = read("/boot/slotstatus", buf, 2, 0);
   if (res <= 0) {
      init_slot_status();
      return 'a';
   }// if
   return buf[0];
}// get_active_slot

void flip_active_slot(void) {
   char current = get_active_slot();
   char next = (current == 'a') ? 'b' : 'a';
   char buf[4];
   buf[0] = next;
   buf[1] = '0';
   buf[2] = '\0';
   write("/boot/slotstatus", buf, 2, 0);
}// flip_active_slot

bool get_slot_status(void) {
   char buf[4] = {0};
   int res = read("/boot/slotstatus", buf, 2, 0);
   if (res <= 0) {
      init_slot_status();
      return true;
   }// if
   return (buf[1] == '1');
}// get_slot_status

bool start_update(const char *update_file) {
   char test_byte = 0;
   if (read(update_file, &test_byte, 1, 0) <= 0) {
      errorn("Update file does not exist: Pausing update");
      return 1;
   }// if
   char active = get_active_slot();
   char target_slot = (active == 'a') ? 'b' : 'a';
   uint32_t status = install_update(target_slot, update_file);
   return status;
}// start_update

void update_process(void) {
   while (1) {
      struct updates_ipc_req req;
      uint32_t sender = user_ipc_recv(0, &req);
      if (sender != (uint32_t)-1) {
         int status = -1;
         if (req.start == 1) {
            status = start_update(req.path);
         } else if (req.start == 0) {
            infon("Updates process called with start=0, waiting");
            status = 1;
         }// if
         user_ipc_send(sender, (void *)(intptr_t)status);
      }// if
   }// while
}// update_process
    
void create_update_process(void) {
   init_slot_status();
   infon("Starting update process");
   uint32_t pid = create_process((void *)update_process, UPDATE_PROCESS_GID);
   struct process *update_proc = get_process_by_pid(pid);
   if (update_proc) {
      uint32_t gid = update_proc->gid;
      uint32_t sid = generate_sid(gid, (uintptr_t)update_process);
      update_proc->sid = sid;
   }// if
   successn("Update process started");
}// create_update_process

