#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define PATCH_MAGIC 0x4669677572653039

struct patch_header {
   uint32_t magic;
   uint32_t signature;
   uint32_t old_size;
   uint32_t new_size;
   uint32_t chunk_count;
};

struct chunk {
   uint32_t offset;
   uint32_t len;
   uint8_t *data;
};

static uint8_t *read_file(const char *path, uint32_t *out_size) {
   FILE *f = fopen(path, "rb");
   if (!f) {
      perror("fopen");
      return NULL;
   }// if
   fseek(f, 0, SEEK_END);
   long sz = ftell(f);
   fseek(f, 0, SEEK_SET);
   if (sz < 0) {
      fclose(f);
      return NULL;
   }// if
   uint8_t *buf = (uint8_t *)malloc(sz + 1);
   if (!buf) {
      fclose(f);
      return NULL;
   }// if
   if (fread(buf, 1, sz, f) != (size_t)sz) {
      free(buf);
      fclose(f);
      return NULL;
   }// if
   fclose(f);
   *out_size = (uint32_t)sz;
   return buf;
}// read_file

int main(int argc, char **argv) {
   if (argc < 4) {
      fprintf(stderr, "Usage: %s <old_kernel> <new_kernel> <output_patch>\n", argv[0]);
      return 1;
   }// if

   uint32_t old_size = 0;
   uint32_t new_size = 0;
   uint8_t *old_buf = read_file(argv[1], &old_size);
   uint8_t *new_buf = read_file(argv[2], &new_size);

   if (!old_buf || !new_buf) {
      fprintf(stderr, "Error reading input kernel binaries\n");
      if (old_buf) free(old_buf);
      if (new_buf) free(new_buf);
      return 1;
   }// if

   uint32_t max_chunks = new_size / 2 + 1;
   struct chunk *chunks = (struct chunk *)malloc(sizeof(struct chunk) * max_chunks);
   if (!chunks) {
      free(old_buf);
      free(new_buf);
      return 1;
   }// if

   uint32_t chunk_count = 0;
   uint32_t i = 0;

   while (i < new_size) {
      // Find start of difference
      if (i >= old_size || new_buf[i] != old_buf[i]) {
         uint32_t start = i;
         while (i < new_size && (i >= old_size || new_buf[i] != old_buf[i])) {
            i++;
         }// while
         chunks[chunk_count].offset = start;
         chunks[chunk_count].len = i - start;
         chunks[chunk_count].data = &new_buf[start];
         chunk_count++;
      } else {
         i++;
      }// if
   }// while

   FILE *out = fopen(argv[3], "wb");
   if (!out) {
      perror("fopen output");
      free(chunks);
      free(old_buf);
      free(new_buf);
      return 1;
   }// if

   struct patch_header hdr;
   hdr.magic = PATCH_MAGIC;
   hdr.old_size = old_size;
   hdr.new_size = new_size;
   hdr.chunk_count = chunk_count;

   fwrite(&hdr, sizeof(struct patch_header), 1, out);

   uint32_t total_payload = 0;
   for (uint32_t c = 0; c < chunk_count; c++) {
      fwrite(&chunks[c].offset, sizeof(uint32_t), 1, out);
      fwrite(&chunks[c].len, sizeof(uint32_t), 1, out);
      fwrite(chunks[c].data, 1, chunks[c].len, out);
      total_payload += chunks[c].len;
   }// for

   fclose(out);
   free(chunks);
   free(old_buf);
   free(new_buf);

   printf("generated %s: %u chunks, %u diff bytes (header: %zu bytes)\n",
          argv[3], chunk_count, total_payload, sizeof(struct patch_header));
   return 0;
}// main
