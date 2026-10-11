#ifndef __AB_UPDATE_H
#define __AB_UPDATE_H

#include <stdbool.h>
#include <stdint.h>

#define SLOT_A "/boot/a"
#define SLOT_B "/boot/b"

extern uint32_t install_update(char use_slot, const char *update_file);
extern char get_active_slot(void);
extern void flip_active_slot(void);
extern bool get_slot_status(void);
extern void init_slot_status(void);

#define PATCH_MAGIC 0x50415443 // "PATC"

struct patch_header {
   uint32_t magic;
   uint32_t old_size;
   uint32_t new_size;
   uint32_t chunk_count;
};

extern int apply_patch(uint8_t *base_buf, uint32_t base_size, const uint8_t *patch_buf, uint32_t patch_size);

#endif

