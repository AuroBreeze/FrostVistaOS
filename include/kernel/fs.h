#define VFS_DIR 0x0001
#define VFS_FILE 0x0010

#include "kernel/defs.h"
#include "kernel/stat.h"

struct super_block;
/**
 * vfs_inode_ops: Operations for a VFS node
 * */
typedef struct vfs_inode_ops {
  struct vfs_inode *(*lookup)(struct vfs_inode *dir, char *name);
  int (*create)(struct vfs_inode *dir, char *name, int mode);
  int (*link)(struct vfs_inode *old_node, struct vfs_inode *dir, char *name);
  int (*unlink)(struct vfs_inode *dir, char *name);
  int (*mkdir)(struct vfs_inode *dir, char *name, int mode);
  int (*rmdir)(struct vfs_inode *dir, char *name);
  int (*rename)(struct vfs_inode *old_dir, char *old_name,
                struct vfs_inode *new_dir, char *new_name);
  int (*stat)(struct vfs_inode *node, struct stat *st);

} vfs_inode_ops_t;

typedef struct vfs_file_ops {
  int (*read)(struct file *f, uint8 *buffer, uint32 size);
  int (*write)(struct file *f, uint8 *buffer, uint32 size);
  int (*readdir)(struct file *f,
                 struct vfs_dirent *dirent); // `readdir` requires an offset, so
                                             // it is placed on the `file` side
  int (*lseek)(struct file *f, int64 offset, int whence);
  int (*close)(struct file *f);
} vfs_file_ops_t;

typedef struct vfs_inode {
  char name[16];
  uint32 ino;    // Inode number
  uint32 count;  // Reference count
  uint32 nlinks; // Number of hard links
  struct super_block *sb;
  vfs_inode_ops_t *ops; // pointer to the operations of the node
  vfs_file_ops_t *default_f_ops;

  short type;         // type of the node
  uint64 size;        // size of the node
  void *private_data; // Pointer to specific data
} vfs_inode_t;

struct superblock_ops {
  struct vfs_node *(*alloc_inode)(struct super_block *sb);
  void (*destroy_inode)(struct vfs_node *inode);
  void (*write_super)(struct super_block *sb);
};

/**
 * super_block: Super block
 * */
struct super_block {
  uint32 dev;        // device id
  uint32 block_size; // block size
  struct superblock_ops *ops;
  struct vfs_node *root; // root of the filesystem
  void *private_data;    // Pointer to specific data
};
