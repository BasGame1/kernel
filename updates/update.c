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

bool search_updates() {}// search_updates

void get_update() {}// get_update

void apply_update() {
   //install_update(), depends on ab, abnr or no ab
}// apply_update
