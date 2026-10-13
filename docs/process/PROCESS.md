# PROCESSES

- Runs in GIDs, the priviledge ones goes from 0 to 8 or 11
- The max number of processes is 16


`void create_$NAME$_process() {
   printu("Starting $NAME$ process");
   uint32_t pid = create_process((void *)$FUNCTION_NAME$, $GID$);
   struct process *$NAME$_process = get_process_by_pid(pid);
   if (keyboard_process) {
      uint32_t gid = $NAME$_process->gid;
      uint32_t sid = generate_sid(gid, (uintptr_t)$FUNCTION_NAME$);
      $NAME$_process->sid = sid;
   }// if
   // pkcall_map_device(physical, virtual);
   printu("$NAME$ process started");
}// create_$NAME$_process`
