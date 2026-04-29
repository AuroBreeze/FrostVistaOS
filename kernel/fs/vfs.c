#include "driver/hal_console.h"
#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/log.h"

vfs_inode_t *vfs_root;

/**
 * skipelem: Return a pointer to the position following the next ‘/’ and copy
 * the current segment into `name`
 *
 * Return: a pointer to the position following the next ‘/’,
 * Return 0 if the path is empty
 * */
static char *skipelem(char *path, char *name) {
  while (*path == '/')
    path++;
  if (*path == '\0')
    return 0;

  char *s = path;
  while (*path != '/' && *path != '\0')
    path++;

  int len = path - s;
  if (len >= 128)
    len = 127;
  memmove(name, s, len);
  name[len] = '\0';

  while (*path == '/')
    path++;
  return path;
}

/**
 * vfs_lookup: Look up a path in a VFS node
 * */
vfs_inode_t *vfs_lookup(vfs_inode_t *node, char *path) {
  char name[128];
  vfs_inode_t *current = node;
  while ((path = skipelem(path, name)) != 0) {
    if (!(current->type & VFS_DIR))
      return 0;

    vfs_inode_t *next = current->ops->lookup(current, name);
    if (next == 0)
      return 0;
    current = next;
  }
  return current;
}

vfs_file_ops_t uart_ops;

vfs_inode_t *create_vfs_node(char *name, uint32 flags) {
  vfs_inode_t *node = kalloc();
  if (!node)
    return 0;

  strcpy(node->name, name);
  node->type = flags;

  // test ops
  node->default_f_ops = &uart_ops;

  return node;
}

vfs_inode_t *dev_dir;
vfs_inode_t *tty_file;

// For testing purposes
vfs_inode_t *mock_finddir(vfs_inode_t *node, char *name) {
  if (node == vfs_root && strcmp(name, "dev") == 0) {
    return dev_dir;
  }
  if (node == dev_dir && strcmp(name, "tty") == 0) {
    return tty_file;
  }
  return 0; // Not found
}

vfs_inode_ops_t default_mock_ops = {.lookup = mock_finddir};

// vfs_inode_t *easyfs_lookup(vfs_inode_t *node, char *name) {
//   struct buf *b = bread(0, 1);
// }
//
// vfs_inode_ops_t easyfs_ops = {
//   .lookup = easyfs_lookup,
// };
// vfs_file_ops_t easyfs_fops = {
//   .read = easyfs_read,
//   .write = easyfs_write,
//   .readdir = easyfs_readdir,
// };

void vfs_init() {
  vfs_root = create_vfs_node("/", VFS_DIR);
  vfs_root->ops = &default_mock_ops;

  dev_dir = create_vfs_node("dev", VFS_DIR);
  dev_dir->ops = &default_mock_ops;

  tty_file = create_vfs_node("tty", VFS_FILE);
  tty_file->default_f_ops = &uart_ops;

  // At this point, the logical tree structure has been established: / -> dev ->
  // tty
}

void test_vfs() {
  LOG_INFO("Test vfs");
  vfs_inode_t *node = vfs_lookup(vfs_root, "/dev/tty");
  if (node) {
    LOG_INFO("Success find node: %s", node->name);
  } else {
    LOG_ERROR("Fail to find node");
  }
}

int uart_vfs_write(struct file *f, uint8 *buffer, uint32 size) {
  for (uint32 i = 0; i < size; i++) {
    hal_console_putc(buffer[i]);
  }
  return size;
}

// PERF: The current UART read operation uses a polling method and reads until
// the buffer is full. For the subsequent shell implementation, an
// interrupt-based method will be required, and operations such as line breaks
// will need to be handled separately.
int uart_vfs_read(struct file *f, uint8 *buffer, uint32 size) {
  int count = 0;
  for (uint32 i = 0; i < size; i++) {
    int c;
    while ((c = hal_console_getc()) <= 0) {
      yield();
    }

    buffer[i] = (uint8)c;
    count++;

    if (buffer[i] == '\r' || buffer[i] == '\n')
      break;
  }
  return count;
}

vfs_file_ops_t uart_ops = {
    .read = uart_vfs_read, .write = uart_vfs_write, .readdir = 0, .close = 0};
