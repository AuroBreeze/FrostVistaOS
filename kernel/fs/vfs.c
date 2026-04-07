#include "driver/hal_console.h"
#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/log.h"

vfs_node_t *vfs_root;

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
vfs_node_t *vfs_lookup(vfs_node_t *node, char *path) {
  char name[128];
  vfs_node_t *current = node;
  while ((path = skipelem(path, name)) != 0) {
    if (!(current->flags & VFS_DIR))
      return 0;

    vfs_node_t *next = current->ops->finddir(current, name);
    if (next == 0)
      return 0;
    current = next;
  }
  return current;
}

vfs_node_ops_t uart_ops;

vfs_node_t *create_vfs_node(char *name, uint32 flags) {
  vfs_node_t *node = kalloc();
  if (!node)
    return 0;

  strcpy(node->name, name);
  node->flags = flags;

  // test ops
  node->ops = &uart_ops;

  return node;
}

vfs_node_t *dev_dir;
vfs_node_t *tty_file;

// For testing purposes
vfs_node_t *mock_finddir(vfs_node_t *node, char *name) {
  if (node == vfs_root && strcmp(name, "dev") == 0) {
    return dev_dir;
  }
  if (node == dev_dir && strcmp(name, "tty") == 0) {
    return tty_file;
  }
  return 0; // Not found
}

vfs_node_ops_t default_mock_ops = {.finddir = mock_finddir};
void vfs_init() {
  vfs_root = create_vfs_node("/", VFS_DIR);
  vfs_root->ops = &default_mock_ops;

  dev_dir = create_vfs_node("dev", VFS_DIR);
  dev_dir->ops = &default_mock_ops;

  tty_file = create_vfs_node("tty", VFS_FILE);
  tty_file->length = 0;
  tty_file->ops = &uart_ops;


  // At this point, the logical tree structure has been established: / -> dev ->
  // tty
}

void test_vfs() {
  LOG_INFO("Test vfs");
  vfs_node_t *node = vfs_lookup(vfs_root, "/dev/tty");
  if (node) {
    LOG_INFO("Success find node: %s", node->name);
  } else {
    LOG_ERROR("Fail to find node");
  }
}

int uart_vfs_write(vfs_node_t *node, uint32 offset, uint32 size,
                   uint8 *buffer) {
  for (uint32 i = 0; i < size; i++) {
    hal_console_putc(buffer[i]);
  }
  return size;
}

int uart_vfs_read(vfs_node_t *node, uint32 offset, uint32 size, uint8 *buffer) {
  for (uint32 i = 0; i < size; i++) {
    buffer[i] = hal_console_getc();
  }
  return size;
}

vfs_node_ops_t uart_ops = {.read = uart_vfs_read,
                           .write = uart_vfs_write,
                           .finddir = 0,
                           .readdir = 0,
                           .open = 0,
                           .close = 0};
