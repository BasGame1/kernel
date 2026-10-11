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
void update_process(void) {
   init_fs();
   while (1) {
      struct updates_ipc_req req;
      uint32_t sender = user_ipc_recv(0, &req);
      if (sender != (uint32_t)-1) {
         int status = -1;
         if (req.start == 1) {
            status = start_update(req.path);
         } else if (req.start == 0) {
            infon("Updates process called with start=0, waiting");
            status = -1;
         }// if
         user_ipc_send(sender, (void *)(intptr_t)status);
      }// if
   }// while
}// update_process
    
void create_update_process() {
   infon("Starting update process");
   uint32_t pid = create_process((void *)update_process, UPDATE_PROCESS_GID);
   struct process *update_process = get_process_by_pid(pid);
   if (update_process) {
      uint32_t gid = update_process->gid;
      uint32_t sid = generate_sid(gid, (uintptr_t)update_process);
      update_process->sid = sid;
   }// if
   successn("Update process started");
}// create_update_process

bool search_updates() {}// search_updates

bool start_update(char *path update_file) {
   char use_slot;
   if ( // check if file exists ! -f /boot/a/active) {
      active_slot = "a";
   } else if (// ! -f /boot/a/active) {
      active_slot = "b";
   } else {
      errorn("Error with slots, both active??: Pausing update");
      return 1;
   }// if
   if (// ! -f update_file) {
      errorn("Update file does not exist: Pausing update");
   }
   install_update(use_slot, update_file);
}// start_update
