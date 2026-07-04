#include "asm/defs.h"
#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/log.h"
#include "tmpfs.h"
#include "asm/mm.h"
#include "core/proc.h"

// PERF: tmpfs metadata objects are much smaller than one page, so allocating
// each one with kalloc() wastes most of the page. Until a slab allocator or a
// page-backed object allocator exists, fixed-size pools keep the probe small
// and deterministic. The tradeoff is a hard object limit.
#define TMPFS_POOL_SIZE 64

static struct tmpfs_dirent dirent_pool[TMPFS_POOL_SIZE];
static int dirent_used[TMPFS_POOL_SIZE];
static struct tmpfs_inode inode_pool[TMPFS_POOL_SIZE];
static int inode_used[TMPFS_POOL_SIZE];
static struct tmpfs_file file_pool[TMPFS_POOL_SIZE];
static int file_used[TMPFS_POOL_SIZE];

struct tmpfs_dirent *tmpfs_alloc_dirent(void)
{
	for (int i = 0; i < TMPFS_POOL_SIZE; i++) {
		if (!dirent_used[i]) {
			dirent_used[i] = 1;
			memset(&dirent_pool[i], 0, sizeof(dirent_pool[i]));
			return &dirent_pool[i];
		}
	}
	return 0;
}

void tmpfs_free_dirent(struct tmpfs_dirent *de)
{
	if (de == 0)
		return;
	for (int i = 0; i < TMPFS_POOL_SIZE; i++) {
		if (de == &dirent_pool[i]) {
			memset(de, 0, sizeof(*de));
			dirent_used[i] = 0;
			return;
		}
	}
}

struct tmpfs_inode *tmpfs_alloc_inode(void)
{
	for (int i = 0; i < TMPFS_POOL_SIZE; i++) {
		if (!inode_used[i]) {
			inode_used[i] = 1;
			memset(&inode_pool[i], 0, sizeof(inode_pool[i]));
			return &inode_pool[i];
		}
	}
	return 0;
}

void tmpfs_free_inode(struct tmpfs_inode *inode)
{
	if (inode == 0)
		return;
	for (int i = 0; i < TMPFS_POOL_SIZE; i++) {
		if (inode == &inode_pool[i]) {
			memset(inode, 0, sizeof(*inode));
			inode_used[i] = 0;
			return;
		}
	}
}

struct tmpfs_file *tmpfs_alloc_file(void)
{
	for (int i = 0; i < TMPFS_POOL_SIZE; i++) {
		if (!file_used[i]) {
			file_used[i] = 1;
			memset(&file_pool[i], 0, sizeof(file_pool[i]));
			return &file_pool[i];
		}
	}
	return 0;
}

void tmpfs_free_file(struct tmpfs_file *file)
{
	if (file == 0)
		return;
	for (int i = 0; i < TMPFS_POOL_SIZE; i++) {
		if (file == &file_pool[i]) {
			memset(file, 0, sizeof(*file));
			file_used[i] = 0;
			return;
		}
	}
}

struct tmpfs_dirent *tmpfs_find_dirent_ino(uint32 ino,
					   struct tmpfs_dirent *base)
{
	if (base == 0)
		return 0;

	struct tmpfs_dirent *stack[32] = {0};
	int sp = 0;

	stack[sp++] = base;

	while (sp > 0) {
		struct tmpfs_dirent *first = stack[--sp];
		struct tmpfs_dirent *entry = first;

		while (1) {
			if (entry->ino == ino)
				return entry;

			struct tmpfs_inode *tip = entry->inode;
			if (tip != 0 && tip->type == VFS_DIR &&
			    tip->dir.children != 0) {
				if (sp >= 32)
					return 0;
				stack[sp++] = tip->dir.children;
			}

			struct list_head *next = entry->list.next;
			if (next == &first->list)
				break;

			entry = container_of(next, struct tmpfs_dirent, list);
		}
	}

	return 0;
}

struct tmpfs_dirent *tmpfs_lookup_child(struct vfs_inode *dir, char *name)
{
	if (dir == 0 || dir->type != VFS_DIR)
		return 0;
	if (name == 0 || name[0] == '\0' || name[0] == '/')
		return 0;
	struct tmpfs_dirent *child = (struct tmpfs_dirent *) dir->private_data;
	if (child == 0)
		return 0;

	struct tmpfs_dirent *entry = child;
	while (1) {
		if (!namecmp(entry->name, name))
			return entry;

		struct list_head *next = entry->list.next;
		if (next == &child->list)
			break;

		entry = container_of(next, struct tmpfs_dirent, list);
	}

	return 0;
}

int tmpfs_vfs_read(struct file *f, uint8 *buffer, uint32 size)
{
	if (f == 0 || f->node == 0 || buffer == 0) {
		return -1;
	}

	// NOTE: At this stage, the CPU is not yet initialized, so sleeplocks
	// cannot be used.
	int n = tmpfs_read(f->node, 0, (uint64) buffer, f->offset, size);

	return n;
}

/**
 * tmpfs_read - Read data from a tmpfs file inode
 *
 * Context: Resolve the tmpfs_dirent for ip->ino, then read from the file page
 * list starting at off. The file data pages are page-sized tmpfs_file nodes.
 *
 * Return: number of bytes read on success, -1 on invalid input or inconsistent
 * tmpfs metadata.
 * */
int tmpfs_read(struct vfs_inode *ip, int user_dst, uint64 dst, uint32 off,
	       uint32 size)
{
	if (ip == 0)
		return -1;
	if (off > ip->size || off + size < off) {
		return -1;
	}
	if (size == 0)
		return 0;

	// If the amount of data read exceeds the space allocated to the current
	// inode
	if (off + size > ip->size) {
		size = ip->size - off;
	}

	struct tmpfs_dirent *rt = tmpfs_get_root_dirent();
	struct tmpfs_dirent *tmp = tmpfs_find_dirent_ino(ip->ino, rt);
	if (tmp == 0) {
		LOG_WARN("tmpfs_read: tmp == 0");
		return -1;
	}
	struct tmpfs_inode *tip = tmp->inode;
	if (tip == 0 || tip->type != VFS_FILE) {
		LOG_WARN("tmpfs_read: tip->type != VFS_FILE or tip == 0");
		return -1;
	}
	if (tip->size == 0)
		return 0;
	struct tmpfs_file *first = tip->files.files;
	if (first == 0) {
		LOG_WARN("tmpfs_read: first == 0");
		return -1;
	}
	struct tmpfs_file *file = first;

	// Calculate the page offset and move to that location.
	uint npages = off / PGSIZE;
	for (int i = 0; i < npages; i++) {
		if (file == 0)
			return -1;
		struct list_head *next = file->list.next;
		// Offset exceeds the actual file size.
		if (next == &first->list)
			return -1;
		file = container_of(next, struct tmpfs_file, list);
	}

	uint32 tot;
	uint32 m;
	for (tot = 0; tot < size; tot += m, off += m, dst += m) {
		if (file == 0)
			return tot == size ? tot : -1;

		uint64 addr = (uint64) file->space;
		if (addr == 0)
			return -1;

		// Read only up to the end of the current page; the loop
		// advances to the next tmpfs_file node when more bytes remain.
		m = (size - tot) > (PGSIZE - (off % PGSIZE))
			? PGSIZE - (off % PGSIZE)
			: size - tot;

		if (user_dst) {
			struct Process *proc = get_proc();
			if (copyout(proc->pagetable, (void *) dst,
				    (uint64) (addr + (off % PGSIZE)),
				    (int) m) < 0) {
				return -1;
			}
		} else {
			memmove((void *) dst,
				(const void *) (addr + (off % PGSIZE)), m);
		}

		struct list_head *next = file->list.next;
		if (next == &first->list) {
			// Because the function returns early before the for
			// loop applies + m, the return value needs to add it
			// explicitly.
			if (tot + m < size)
				return -1;
			return tot + m;
		}

		file = container_of(next, struct tmpfs_file, list);
	}

	return tot;
}

/**
 * tmpfs_vfs_create - Create a child inode in a tmpfs directory
 *
 * Context: VFS passes a parent directory inode and a single leaf name, not a
 * full path. Allocate tmpfs metadata, initialize it according to mode, and link
 * the new dirent into the parent directory's circular child list.
 *
 * @dir: Parent directory vfs_inode.
 * @name: Leaf name to create under dir.
 * @mode: VFS_FILE or VFS_DIR.
 *
 * Return: 0 on success, -1 on invalid input, duplicate name, allocation
 * failure, or unsupported mode.
 * */
int tmpfs_vfs_create(struct vfs_inode *dir, char *name, int mode)
{
	if (dir == 0 || dir->type != VFS_DIR || name == 0 || name[0] == '\0')
		return -1;

	// Find the tmpfs_dirent pointed to by the current vfs_inode.
	struct tmpfs_dirent *tmp =
	    tmpfs_find_dirent_ino(dir->ino, tmpfs_get_root_dirent());
	if (tmp == 0 || tmp->inode->type != VFS_DIR) {
		LOG_WARN(
		    "tmpfs_create: tmp == 0 || tmp->inode->type != VFS_DIR");
		return -1;
	}
	// Check if it already exists
	struct tmpfs_dirent *existing = tmpfs_lookup_child(dir, name);
	if (existing != 0) {
		LOG_WARN("tmpfs_create: entry already exists");
		return -1;
	}

	struct tmpfs_dirent *entry_dirent = tmpfs_alloc_dirent();
	struct tmpfs_inode *entry_inode = tmpfs_alloc_inode();
	if (entry_dirent == 0 || entry_inode == 0)
		goto failed;

	// Initialize directory entry
	init_list(&entry_dirent->list);
	strncpy(entry_dirent->name, name, DIRSIZ);
	entry_dirent->name[DIRSIZ - 1] = '\0';
	entry_dirent->ino = alloc_ino();
	entry_dirent->parent = tmp;
	entry_dirent->inode = entry_inode;

	// Initialize inode
	entry_inode->type = mode;
	entry_inode->size = 0;

	if (mode == VFS_FILE) {
		struct tmpfs_file *file = tmpfs_alloc_file();
		if (file == 0)
			goto failed;
		init_list(&file->list);
		entry_inode->files.files = file;
	} else if (mode == VFS_DIR) {
		// Under the current design, `children` should point to an
		// existing `dirent`; otherwise, it should be 0.
		entry_inode->dir.children = 0;
	} else {
		goto failed;
	}

	struct tmpfs_dirent *child = tmp->inode->dir.children;
	if (child == 0) {
		init_list(&entry_dirent->list);
		tmp->inode->dir.children = entry_dirent;
		dir->private_data = entry_dirent;
		return 0;
	}

	struct list_head *prev = child->list.prev;
	// Under the current design, `children` should point to an existing
	// `dirent`; otherwise, it should be 0.
	entry_dirent->list.next = &child->list;
	entry_dirent->list.prev = prev;
	prev->next = &entry_dirent->list;
	child->list.prev = &entry_dirent->list;

	tmp->inode->dir.children = entry_dirent;
	dir->private_data = entry_dirent;
	return 0;

failed:
	tmpfs_free_inode(entry_inode);
	tmpfs_free_dirent(entry_dirent);

	return -1;
}
