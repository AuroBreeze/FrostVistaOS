#define VFS_DIR 0x0001
#define VFS_FILE 0x0010

#include "kernel/defs.h"
#include "kernel/stat.h"

/**
 * vfs_node_ops: Operations for a VFS node
 * */
typedef struct vfs_node_ops {
  int (*read)(struct vfs_node *node, uint32 offset, uint32 size, uint8 *buffe);
  int (*write)(struct vfs_node *node, uint32 offset, uint32 size,
               uint8 *buffer);
  struct vfs_node *(*finddir)(struct vfs_node *node, char *name);
  struct vfs_dirent *(*readdir)(struct vfs_node *node, uint32 index);
  void (*close)(struct vfs_node *node);
  void (*open)(struct vfs_node *node);
  int (*stat)(struct vfs_node *node, struct stat *stat);
} vfs_node_ops_t;

typedef struct vfs_node {
  char name[128];       // the name of the node
  uint32 mask;          // the permissions of the node
  uint32 uid;           // user id
  uint32 gid;           // group id
  uint32 flags;         // type of the node
  uint32 length;        // the size of the node
  vfs_node_ops_t *ops;  // pointer to the operations of the node
  struct vfs_node *ptr; // the pointer to the node
} vfs_node_t;
