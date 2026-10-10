# EXT2 filesystem
- This kernel implements a basic ext2 driver as default filesystem
- Under the ext2 filesystem you have virtio
- Functions:
  `ext2_read_block(uint32_t block, void *buffer)` - For reading an ext2 block, used by higher level wrappers (like read())
  `ext2_write_block(uint32_t block, void *buffer)` - For writing an ext2 block, used by higher level wrappers (like write())
  `init_ext2()` - For starting the ext2 driver, must be call only once
  `ext2_read_inode(uint32_t inode_no, struct ext2_inode *out)` [uint32_t] - For reading an inode instead of a block, has return type to see the status of the read
  `ext2_write_inode(uint32_t inode_no, struct ext2_inode *in)` [uint32_t] - For writing an inode instead of a block, has return type to see the status of the write
  `ext2_resolve_path(uint32_t dir_inode_no, const char *name)` [uint32_t] - For finding the inode of a path, has return type to see the status
  `ext2_lookup(uint32_t dir_inode_no, const char *name)' [uint32_t] - For seeing if a path exits
