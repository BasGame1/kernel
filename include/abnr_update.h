#ifndef __ABNR_UPDATE_H
#define __ABNR_UPDATE_H

#include <stdint.h>
#include <stdbool.h>

#define PATCH_MAGIC 0x50415443 // "PATC"

struct patch_header {
   uint32_t magic;
   uint32_t old_size;
   uint32_t new_size;
   uint32_t chunk_count;
};

extern int apply_patch(uint8_t *base_buf, uint32_t base_size, const uint8_t *patch_buf, uint32_t patch_size);
extern uint32_t install_update(char active_slot, const char *update_file);

#endif
