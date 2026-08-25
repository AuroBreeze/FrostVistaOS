#include "kernel/defs.h"
#include "kernel/string.h"
#include "kernel/fs.h"
#define LOG_MODULE "TMPFS TEST"

#include "kernel/test.h"
#include "tmpfs.h"

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

/* Fill the root vfs_inode from the mounted tmpfs root. */
static struct vfs_inode *tmpfs_root_vfs_inode()
{
	struct tmpfs_dir_entry *root = tmpfs_get_root_dir_entry();
	if (root == 0 || root->inode == 0)
		return 0;
	return tmpfs_fill_vfs_inode(TMPFS_ROOT_INO, root->inode, VFS_DIR);
}

static int test_tmpfs_root_init()
{
	TEST_ASSERT(tmpfs_get_root_dir_entry() != 0, "root dir entry is NULL");
	TEST_ASSERT(tmpfs_get_root_sb() != 0, "root super block is NULL");
	TEST_ASSERT(tmpfs_root() != 0, "tmpfs root vfs_inode is NULL");
	TEST_ASSERT(get_vfs_inode_ops() != 0, "inode ops is NULL");
	TEST_ASSERT(get_vfs_file_ops() != 0, "file ops is NULL");
	TEST_ASSERT(get_vfs_superblock_ops() != 0, "superblock ops is NULL");
	return 0;
}

static int test_tmpfs_create_and_lookup()
{
	struct vfs_inode *inode = tmpfs_root_vfs_inode();
	TEST_ASSERT(inode != 0, "fill root inode failed");

	TEST_ASSERT(tmpfs_vfs_create(inode, "test", VFS_DIR) == 0,
		    "Create test dir failed");

	struct vfs_inode *lp = tmpfs_vfs_lookup(inode, "test", 0);
	TEST_ASSERT(lp != 0, "Lookup test dir failed");
	TEST_ASSERT(lp->type == VFS_DIR, "lookup type mismatch");
	return 0;
}

static int test_tmpfs_create_file()
{
	struct vfs_inode *inode = tmpfs_root_vfs_inode();
	TEST_ASSERT(inode != 0, "fill root inode failed");

	TEST_ASSERT(tmpfs_vfs_create(inode, "file", VFS_FILE) == 0,
		    "Create file failed");

	struct vfs_inode *lp = tmpfs_vfs_lookup(inode, "file", 0);
	TEST_ASSERT(lp != 0, "Lookup file failed");
	TEST_ASSERT(lp->type == VFS_FILE, "lookup type mismatch");
	TEST_ASSERT(lp->nlinks == 1, "new file nlinks should be 1");
	return 0;
}

static int test_tmpfs_duplicate_name()
{
	struct vfs_inode *inode = tmpfs_root_vfs_inode();
	TEST_ASSERT(inode != 0, "fill root inode failed");

	TEST_ASSERT(tmpfs_vfs_create(inode, "dup", VFS_DIR) == 0,
		    "First create dup failed");
	TEST_ASSERT(tmpfs_vfs_create(inode, "dup", VFS_DIR) == -1,
		    "Second create dup should fail");
	return 0;
}

static int test_tmpfs_lookup_not_found()
{
	struct vfs_inode *inode = tmpfs_root_vfs_inode();
	TEST_ASSERT(inode != 0, "fill root inode failed");

	TEST_ASSERT(tmpfs_vfs_lookup(inode, "notexist", 0) == 0,
		    "Lookup of missing name should return 0");
	return 0;
}

static int test_tmpfs_invalid_args()
{
	TEST_ASSERT(tmpfs_vfs_create(0, "x", VFS_FILE) == -1,
		    "create(NULL dir) should fail");

	struct vfs_inode *inode = tmpfs_root_vfs_inode();
	TEST_ASSERT(inode != 0, "fill root inode failed");

	TEST_ASSERT(tmpfs_vfs_create(inode, 0, VFS_FILE) == -1,
		    "create(NULL name) should fail");
	TEST_ASSERT(tmpfs_vfs_create(inode, "", VFS_FILE) == -1,
		    "create(empty name) should fail");
	TEST_ASSERT(tmpfs_vfs_lookup(0, "x", 0) == 0,
		    "lookup(NULL dir) should fail");
	TEST_ASSERT(tmpfs_fill_vfs_inode(TMPFS_ROOT_INO, 0, VFS_DIR) == 0,
		    "fill(NULL inode) should fail");
	return 0;
}

static int test_tmpfs_ino_unique()
{
	struct vfs_inode *inode = tmpfs_root_vfs_inode();
	TEST_ASSERT(inode != 0, "fill root inode failed");

	static const char *names[] = {"ino_a", "ino_b", "ino_c"};
	uint32 inos[3] = {0};

	for (int i = 0; i < 3; i++) {
		TEST_ASSERT(
		    tmpfs_vfs_create(inode, (char *) names[i], VFS_FILE) == 0,
		    "Create ino test file failed");
		struct vfs_inode *lp =
		    tmpfs_vfs_lookup(inode, (char *) names[i], 0);
		TEST_ASSERT(lp != 0, "Lookup ino test file failed");
		inos[i] = lp->ino;
	}

	TEST_ASSERT(inos[0] != inos[1] && inos[1] != inos[2] &&
			inos[0] != inos[2],
		    "inode numbers should be unique");

	// A second lookup of the same name must return the same inode number.
	struct vfs_inode *lp = tmpfs_vfs_lookup(inode, "ino_b", 0);
	TEST_ASSERT(lp != 0 && lp->ino == inos[1],
		    "lookup of same file should return same ino");
	return 0;
}

/* Create a fresh directory under the tmpfs root and return its vfs_inode. */
static struct vfs_inode *tmpfs_make_dir(char *name)
{
	struct vfs_inode *root = tmpfs_root_vfs_inode();
	if (root == 0)
		return 0;
	if (tmpfs_vfs_create(root, name, VFS_DIR) != 0)
		return 0;
	return tmpfs_vfs_lookup(root, name, 0);
}

static int test_tmpfs_readdir_empty()
{
	struct vfs_inode *dir = tmpfs_make_dir("rdir_empty");
	TEST_ASSERT(dir != 0, "create rdir_empty failed");

	struct file f = {0};
	f.type = FILE_VFS_NODE;
	f.node = dir;
	f.offset = 0;

	struct vfs_dirent de;
	TEST_ASSERT(tmpfs_vfs_readdir(&f, &de) == 0,
		    "readdir of empty dir should return 0");
	TEST_ASSERT(f.offset == 0, "empty dir offset should stay 0");
	return 0;
}

static int test_tmpfs_readdir_list()
{
	struct vfs_inode *dir = tmpfs_make_dir("rdir_list");
	TEST_ASSERT(dir != 0, "create rdir_list failed");

	TEST_ASSERT(tmpfs_vfs_create(dir, "f1", VFS_FILE) == 0,
		    "create f1 failed");
	TEST_ASSERT(tmpfs_vfs_create(dir, "f2", VFS_DIR) == 0,
		    "create f2 failed");

	struct file f = {0};
	f.type = FILE_VFS_NODE;
	f.node = dir;
	f.offset = 0;

	int n = 0;
	uint32 inos[2] = {0};
	struct vfs_dirent de;
	int r;
	while ((r = tmpfs_vfs_readdir(&f, &de)) == 1) {
		TEST_ASSERT(n < 2, "more entries than created");

		if (de.name[0] == 'f' && de.name[1] == '1' &&
		    de.name[2] == '\0') {
			TEST_ASSERT(de.type == VFS_FILE, "f1 type mismatch");
			struct vfs_inode *lp = tmpfs_vfs_lookup(dir, "f1", 0);
			TEST_ASSERT(lp != 0 && lp->ino == de.ino,
				    "f1 ino mismatch");
		} else if (de.name[0] == 'f' && de.name[1] == '2' &&
			   de.name[2] == '\0') {
			TEST_ASSERT(de.type == VFS_DIR, "f2 type mismatch");
			struct vfs_inode *lp = tmpfs_vfs_lookup(dir, "f2", 0);
			TEST_ASSERT(lp != 0 && lp->ino == de.ino,
				    "f2 ino mismatch");
		} else {
			TEST_ASSERT(0, "unexpected entry name");
		}
		inos[n++] = de.ino;
	}
	TEST_ASSERT(r == 0, "readdir should end with 0");
	TEST_ASSERT(n == 2, "expected 2 entries");
	TEST_ASSERT(inos[0] != inos[1], "entry inos should differ");
	TEST_ASSERT(f.offset == 2 * (uint64) sizeof(struct tmpfs_dir_entry),
		    "offset should advance per entry");
	return 0;
}

static int test_tmpfs_readdir_invalid()
{
	TEST_ASSERT(tmpfs_vfs_readdir(0, 0) == -1,
		    "readdir(NULL f) should fail");

	struct file f = {0};
	f.type = FILE_VFS_NODE;
	struct vfs_dirent de;

	TEST_ASSERT(tmpfs_vfs_readdir(&f, &de) == -1,
		    "readdir(NULL node) should fail");

	struct vfs_inode *dir = tmpfs_make_dir("rdir_bad");
	TEST_ASSERT(dir != 0, "create rdir_bad failed");
	f.node = dir;

	TEST_ASSERT(tmpfs_vfs_readdir(&f, 0) == -1,
		    "readdir(NULL dirent) should fail");

	f.type = FILE_PIPE;
	TEST_ASSERT(tmpfs_vfs_readdir(&f, &de) == -1,
		    "readdir(non VFS_NODE file) should fail");
	f.type = FILE_VFS_NODE;

	// A non-directory node must be rejected.
	TEST_ASSERT(tmpfs_vfs_create(dir, "x", VFS_FILE) == 0,
		    "create x failed");
	struct vfs_inode *file_node = tmpfs_vfs_lookup(dir, "x", 0);
	TEST_ASSERT(file_node != 0, "lookup x failed");
	f.node = file_node;
	TEST_ASSERT(tmpfs_vfs_readdir(&f, &de) == -1,
		    "readdir(non-dir node) should fail");
	return 0;
}

static int test_tmpfs_mkdir()
{
	struct vfs_inode *root = tmpfs_root_vfs_inode();
	TEST_ASSERT(root != 0, "fill root inode failed");

	TEST_ASSERT(tmpfs_vfs_mkdir(root, "m1", 0) == 0, "mkdir m1 failed");

	struct vfs_inode *lp = tmpfs_vfs_lookup(root, "m1", 0);
	TEST_ASSERT(lp != 0, "lookup m1 failed");
	TEST_ASSERT(lp->type == VFS_DIR, "mkdir type mismatch");
	return 0;
}

static int test_tmpfs_mkdir_duplicate()
{
	struct vfs_inode *root = tmpfs_root_vfs_inode();
	TEST_ASSERT(root != 0, "fill root inode failed");

	TEST_ASSERT(tmpfs_vfs_mkdir(root, "mdup", 0) == 0,
		    "first mkdir mdup failed");
	TEST_ASSERT(tmpfs_vfs_mkdir(root, "mdup", 0) == -1,
		    "second mkdir mdup should fail");
	return 0;
}

static int test_tmpfs_mkdir_invalid()
{
	TEST_ASSERT(tmpfs_vfs_mkdir(0, "x", 0) == -1,
		    "mkdir(NULL dir) should fail");

	struct vfs_inode *root = tmpfs_root_vfs_inode();
	TEST_ASSERT(root != 0, "fill root inode failed");

	TEST_ASSERT(tmpfs_vfs_mkdir(root, 0, 0) == -1,
		    "mkdir(NULL name) should fail");
	TEST_ASSERT(tmpfs_vfs_mkdir(root, "", 0) == -1,
		    "mkdir(empty name) should fail");
	return 0;
}

static int test_tmpfs_mkdir_nlinks()
{
	struct vfs_inode *root = tmpfs_root_vfs_inode();
	TEST_ASSERT(root != 0, "fill root inode failed");

	uint32 before = root->nlinks;
	TEST_ASSERT(tmpfs_vfs_mkdir(root, "mlink", 0) == 0,
		    "mkdir mlink failed");

	// Re-fill the root inode: nlinks is refreshed from tmpfs_inode.
	struct vfs_inode *root2 = tmpfs_root_vfs_inode();
	TEST_ASSERT(root2 != 0, "refill root failed");
	TEST_ASSERT(root2->nlinks == before + 1,
		    "parent nlinks should increase by 1");
	return 0;
}

static int test_tmpfs_mkdir_subfile()
{
	struct vfs_inode *root = tmpfs_root_vfs_inode();
	TEST_ASSERT(root != 0, "fill root inode failed");

	TEST_ASSERT(tmpfs_vfs_mkdir(root, "msub", 0) == 0, "mkdir msub failed");
	struct vfs_inode *sub = tmpfs_vfs_lookup(root, "msub", 0);
	TEST_ASSERT(sub != 0 && sub->type == VFS_DIR, "lookup msub failed");

	// The fresh directory must be usable: create a file inside it.
	TEST_ASSERT(tmpfs_vfs_create(sub, "inner", VFS_FILE) == 0,
		    "create inner failed");
	struct vfs_inode *inner = tmpfs_vfs_lookup(sub, "inner", 0);
	TEST_ASSERT(inner != 0 && inner->type == VFS_FILE,
		    "lookup inner failed");
	return 0;
}

/* ---- tmpfs write tests ---- */

/* Forward declaration: tmpfs_data_at is defined below tmpfs_mem_eq_at. */
static const uint8 *tmpfs_data_at(struct vfs_inode *file, uint64 off);

/* The kernel has no memcmp; compare byte-by-byte (1 = equal). */
static int tmpfs_mem_eq(const void *a, const void *b, uint64 n)
{
	const uint8 *pa = (const uint8 *) a;
	const uint8 *pb = (const uint8 *) b;
	for (uint64 i = 0; i < n; i++) {
		if (pa[i] != pb[i])
			return 0;
	}
	return 1;
}

/* Compare @n bytes at file offset @off against @data, page by page, since
 * consecutive logical blocks are not contiguous in memory. */
static int tmpfs_mem_eq_at(struct vfs_inode *file, uint64 off, const void *data,
			   uint64 n)
{
	const uint8 *p = (const uint8 *) data;
	uint64 remain = n;
	while (remain > 0) {
		uint64 len = min(PGSIZE - off % PGSIZE, remain);
		if (!tmpfs_mem_eq(tmpfs_data_at(file, off), p, len))
			return 0;
		p += len;
		off += len;
		remain -= len;
	}
	return 1;
}

static struct vfs_inode *tmpfs_make_file(char *name)
{
	struct vfs_inode *root = tmpfs_root_vfs_inode();
	if (root == 0)
		return 0;
	if (tmpfs_vfs_create(root, name, VFS_FILE) != 0)
		return 0;
	return tmpfs_vfs_lookup(root, name, 0);
}

/* Write through a synthetic file descriptor at an explicit offset. */
static int tmpfs_write_at(struct vfs_inode *file, uint64 off, const char *buf,
			  uint32 size)
{
	struct file f = {0};
	f.type = FILE_VFS_NODE;
	f.node = file;
	f.offset = off;
	return tmpfs_vfs_write(&f, (uint8 *) buf, size);
}

/* Resolve the data-page address holding byte @off (mirror of tmpfs_bmap). */
static const uint8 *tmpfs_data_at(struct vfs_inode *file, uint64 off)
{
	struct tmpfs_inode *inode = (struct tmpfs_inode *) file->private_data;
	uint32 bn = off / PGSIZE;
	uint64 addr;

	if (bn < TMPFS_NDIRECT) {
		addr = inode->blocks[bn];
	} else if (bn < TMPFS_NDIRECT + TMPFS_NINDIRECT) {
		uint64 *ind = (uint64 *) inode->blocks[TMPFS_NDIRECT];
		addr = ind[bn - TMPFS_NDIRECT];
	} else {
		uint64 *dind = (uint64 *) inode->blocks[TMPFS_NDIRECT + 1];
		uint32 idx = bn - TMPFS_NDIRECT - TMPFS_NINDIRECT;
		uint64 *ind = (uint64 *) dind[idx / TMPFS_NINDIRECT];
		addr = ind[idx % TMPFS_NINDIRECT];
	}
	return (const uint8 *) addr + off % PGSIZE;
}

static int test_tmpfs_write_basic()
{
	TEST_LOG("write_basic: make_file");
	struct vfs_inode *file = tmpfs_make_file("w_basic");
	TEST_ASSERT(file != 0, "create w_basic failed");
	TEST_LOG("write_basic: file created, calling write");

	char buf[] = "hello";
	TEST_ASSERT(tmpfs_write_at(file, 0, buf, sizeof(buf) - 1) ==
			(int) (sizeof(buf) - 1),
		    "write 5 bytes failed");
	TEST_LOG("write_basic: write returned");

	struct tmpfs_inode *inode = (struct tmpfs_inode *) file->private_data;
	TEST_ASSERT(inode->size == sizeof(buf) - 1, "size should be 5");
	TEST_ASSERT(inode->blocks[0] != 0,
		    "direct block 0 should be allocated");
	TEST_ASSERT(tmpfs_mem_eq_at(file, 0, buf, sizeof(buf) - 1) != 0,
		    "data mismatch");
	TEST_LOG("write_basic: verified");
	return 0;
}

static int test_tmpfs_write_cross_page()
{
	struct vfs_inode *file = tmpfs_make_file("w_cross");
	TEST_ASSERT(file != 0, "create w_cross failed");

	static char buf[PGSIZE + 100];
	for (int i = 0; i < (int) sizeof(buf); i++)
		buf[i] = 'A' + (i % 26);

	TEST_ASSERT(tmpfs_write_at(file, 0, buf, sizeof(buf)) ==
			(int) sizeof(buf),
		    "cross-page write failed");

	struct tmpfs_inode *inode = (struct tmpfs_inode *) file->private_data;
	TEST_ASSERT(inode->blocks[0] != 0 && inode->blocks[1] != 0,
		    "both pages should be allocated");
	TEST_ASSERT(inode->size == sizeof(buf), "size should be PGSIZE+100");
	TEST_ASSERT(tmpfs_mem_eq_at(file, 0, buf, sizeof(buf)) != 0,
		    "cross-page data mismatch");
	return 0;
}

static int test_tmpfs_write_offset()
{
	struct vfs_inode *file = tmpfs_make_file("w_off");
	TEST_ASSERT(file != 0, "create w_off failed");

	char buf[50];
	memset(buf, 'x', sizeof(buf));
	TEST_ASSERT(tmpfs_write_at(file, 100, buf, sizeof(buf)) ==
			(int) sizeof(buf),
		    "offset write failed");

	static const char zero[100] = {0};
	TEST_ASSERT(tmpfs_mem_eq_at(file, 0, zero, 100) != 0,
		    "unwritten area should stay zero");
	TEST_ASSERT(tmpfs_mem_eq_at(file, 100, buf, sizeof(buf)) != 0,
		    "offset data mismatch");

	struct tmpfs_inode *inode = (struct tmpfs_inode *) file->private_data;
	TEST_ASSERT(inode->size == 100 + sizeof(buf), "size should be 150");
	return 0;
}

static int test_tmpfs_write_level1()
{
	struct vfs_inode *file = tmpfs_make_file("w_l1");
	TEST_ASSERT(file != 0, "create w_l1 failed");

	static char buf[2 * PGSIZE + 10];
	for (int i = 0; i < (int) sizeof(buf); i++)
		buf[i] = 'a' + (i % 26);

	uint64 off = (uint64) TMPFS_NDIRECT * PGSIZE;
	TEST_ASSERT(tmpfs_write_at(file, off, buf, sizeof(buf)) ==
			(int) sizeof(buf),
		    "level-1 write failed");

	struct tmpfs_inode *inode = (struct tmpfs_inode *) file->private_data;
	TEST_ASSERT(inode->blocks[TMPFS_NDIRECT] != 0,
		    "indirect page should be allocated");
	TEST_ASSERT(inode->size == off + sizeof(buf),
		    "size should cover level-1 write");
	TEST_ASSERT(tmpfs_mem_eq_at(file, off, buf, sizeof(buf)) != 0,
		    "level-1 data mismatch");
	return 0;
}

static int test_tmpfs_write_level2()
{
	struct vfs_inode *file = tmpfs_make_file("w_l2");
	TEST_ASSERT(file != 0, "create w_l2 failed");

	static char buf[PGSIZE + 10];
	for (int i = 0; i < (int) sizeof(buf); i++)
		buf[i] = '0' + (i % 10);

	uint64 off = (uint64) (TMPFS_NDIRECT + TMPFS_NINDIRECT) * PGSIZE;
	TEST_ASSERT(tmpfs_write_at(file, off, buf, sizeof(buf)) ==
			(int) sizeof(buf),
		    "level-2 write failed");

	struct tmpfs_inode *inode = (struct tmpfs_inode *) file->private_data;
	TEST_ASSERT(inode->blocks[TMPFS_NDIRECT + 1] != 0,
		    "double-indirect page should be allocated");
	TEST_ASSERT(inode->size == off + sizeof(buf),
		    "size should cover level-2 write");
	TEST_ASSERT(tmpfs_mem_eq_at(file, off, buf, sizeof(buf)) != 0,
		    "level-2 data mismatch");
	return 0;
}

static int test_tmpfs_write_invalid()
{
	TEST_ASSERT(tmpfs_vfs_write(0, 0, 0) == -1,
		    "write(NULL f) should fail");

	struct file f = {0};
	f.type = FILE_VFS_NODE;
	TEST_ASSERT(tmpfs_vfs_write(&f, 0, 0) == -1,
		    "write(NULL node) should fail");

	struct vfs_inode *file = tmpfs_make_file("w_inv");
	TEST_ASSERT(file != 0, "create w_inv failed");
	f.node = file;

	uint8 buf[8] = {0};
	TEST_ASSERT(tmpfs_vfs_write(&f, 0, sizeof(buf)) == -1,
		    "write(NULL buffer) should fail");

	f.offset = (uint64) TMPFS_MAXFILE * PGSIZE;
	TEST_ASSERT(tmpfs_vfs_write(&f, buf, 1) == -1,
		    "write beyond MAXFILE should fail");

	f.offset = ~0ULL - 5;
	TEST_ASSERT(tmpfs_vfs_write(&f, buf, 10) == -1,
		    "write with overflowing offset should fail");
	return 0;
}

static int test_tmpfs_write_size_max()
{
	struct vfs_inode *file = tmpfs_make_file("w_size");
	TEST_ASSERT(file != 0, "create w_size failed");

	TEST_ASSERT(tmpfs_write_at(file, 0, "a", 1) == 1,
		    "write 1 byte failed");
	struct tmpfs_inode *inode = (struct tmpfs_inode *) file->private_data;
	TEST_ASSERT(inode->size == 1, "size should be 1");

	// Overwrite at the start: size must take the max, not shrink.
	TEST_ASSERT(tmpfs_write_at(file, 0, "bb", 2) == 2,
		    "overwrite 2 bytes failed");
	TEST_ASSERT(inode->size == 2, "size should stay 2");

	// A later offset extends the file.
	TEST_ASSERT(tmpfs_write_at(file, 100, "c", 1) == 1,
		    "extend write failed");
	TEST_ASSERT(inode->size == 101, "size should extend to 101");
	return 0;
}

static int test_tmpfs_write_fill_direct()
{
	struct vfs_inode *file = tmpfs_make_file("w_fill0");
	TEST_ASSERT(file != 0, "create w_fill0 failed");

	static char buf[TMPFS_NDIRECT * PGSIZE];
	for (int i = 0; i < (int) sizeof(buf); i++)
		buf[i] = 'A' + (i % 26);

	TEST_ASSERT(tmpfs_write_at(file, 0, buf, sizeof(buf)) ==
			(int) sizeof(buf),
		    "fill 10 direct blocks failed");

	struct tmpfs_inode *inode = (struct tmpfs_inode *) file->private_data;
	// All 10 direct slots used, the single-indirect slot untouched.
	for (int i = 0; i < TMPFS_NDIRECT; i++)
		TEST_ASSERT(inode->blocks[i] != 0, "direct block should exist");
	TEST_ASSERT(inode->blocks[TMPFS_NDIRECT] == 0,
		    "indirect slot must stay 0 at exact 10-block fill");
	TEST_ASSERT(inode->size == sizeof(buf), "size should be 10 pages");
	TEST_ASSERT(tmpfs_mem_eq_at(file, 0, buf, sizeof(buf)),
		    "fill-direct data mismatch");
	return 0;
}

static int test_tmpfs_write_boundary_l1()
{
	struct vfs_inode *file = tmpfs_make_file("w_bd_l1");
	TEST_ASSERT(file != 0, "create w_bd_l1 failed");

	static char buf[2 * PGSIZE];
	for (int i = 0; i < (int) sizeof(buf); i++)
		buf[i] = 'B' + (i % 26);

	// Blocks 9 (last direct) and 10/11 (single indirect).
	uint64 off = (uint64) (TMPFS_NDIRECT - 1) * PGSIZE;
	TEST_ASSERT(tmpfs_write_at(file, off, buf, sizeof(buf)) ==
			(int) sizeof(buf),
		    "boundary 0->1 write failed");

	struct tmpfs_inode *inode = (struct tmpfs_inode *) file->private_data;
	TEST_ASSERT(inode->blocks[TMPFS_NDIRECT - 1] != 0 &&
			inode->blocks[TMPFS_NDIRECT] != 0,
		    "both direct and indirect slots used");
	TEST_ASSERT(tmpfs_mem_eq_at(file, off, buf, sizeof(buf)),
		    "boundary 0->1 data mismatch");
	return 0;
}

static int test_tmpfs_write_fill_indirect()
{
	struct vfs_inode *file = tmpfs_make_file("w_fill1");
	TEST_ASSERT(file != 0, "create w_fill1 failed");

	// Fill all 512 single-indirect blocks, one page at a time so the
	// test buffer stays small.
	static char buf[PGSIZE];
	uint64 off = (uint64) TMPFS_NDIRECT * PGSIZE;
	for (int i = 0; i < TMPFS_NINDIRECT; i++) {
		memset(buf, 'a' + (i % 26), sizeof(buf));
		TEST_ASSERT(tmpfs_write_at(file, off + (uint64) i * PGSIZE, buf,
					   sizeof(buf)) == (int) sizeof(buf),
			    "fill indirect block failed");
	}

	struct tmpfs_inode *inode = (struct tmpfs_inode *) file->private_data;
	uint64 *indirect = (uint64 *) inode->blocks[TMPFS_NDIRECT];
	TEST_ASSERT(indirect != 0, "indirect page missing");
	for (int i = 0; i < TMPFS_NINDIRECT; i++)
		TEST_ASSERT(indirect[i] != 0, "indirect slot should exist");
	TEST_ASSERT(inode->blocks[TMPFS_NDIRECT + 1] == 0,
		    "double-indirect slot must stay 0 at exact 512-block fill");
	TEST_ASSERT(inode->size ==
			(uint64) (TMPFS_NDIRECT + TMPFS_NINDIRECT) * PGSIZE,
		    "size should cover all indirect blocks");
	return 0;
}

static int test_tmpfs_write_boundary_l2()
{
	struct vfs_inode *file = tmpfs_make_file("w_bd_l2");
	TEST_ASSERT(file != 0, "create w_bd_l2 failed");

	static char buf[2 * PGSIZE];
	for (int i = 0; i < (int) sizeof(buf); i++)
		buf[i] = 'C' + (i % 26);

	// Blocks 521 (last single-indirect) and 522/523 (first
	// double-indirect).
	uint64 off = (uint64) (TMPFS_NDIRECT + TMPFS_NINDIRECT - 1) * PGSIZE;
	TEST_ASSERT(tmpfs_write_at(file, off, buf, sizeof(buf)) ==
			(int) sizeof(buf),
		    "boundary 1->2 write failed");

	struct tmpfs_inode *inode = (struct tmpfs_inode *) file->private_data;
	uint64 *indirect = (uint64 *) inode->blocks[TMPFS_NDIRECT];
	uint64 *dindirect = (uint64 *) inode->blocks[TMPFS_NDIRECT + 1];
	TEST_ASSERT(indirect != 0 && dindirect != 0,
		    "indirect and double-indirect pages exist");
	TEST_ASSERT(indirect[TMPFS_NINDIRECT - 1] != 0,
		    "last indirect slot should exist");
	TEST_ASSERT(dindirect[0] != 0, "first dindirect entry should exist");
	TEST_ASSERT(tmpfs_mem_eq_at(file, off, buf, sizeof(buf)),
		    "boundary 1->2 data mismatch");
	return 0;
}

static int test_tmpfs_write_boundary_idx()
{
	struct vfs_inode *file = tmpfs_make_file("w_bd_idx");
	TEST_ASSERT(file != 0, "create w_bd_idx failed");

	static char buf[2 * PGSIZE];
	for (int i = 0; i < (int) sizeof(buf); i++)
		buf[i] = 'D' + (i % 26);

	// Blocks 1033 (dindirect idx 0, last slot) and 1034 (idx 1, first
	// slot).
	uint64 off =
	    (uint64) (TMPFS_NDIRECT + TMPFS_NINDIRECT + TMPFS_NINDIRECT - 1) *
	    PGSIZE;
	TEST_ASSERT(tmpfs_write_at(file, off, buf, sizeof(buf)) ==
			(int) sizeof(buf),
		    "boundary idx write failed");

	struct tmpfs_inode *inode = (struct tmpfs_inode *) file->private_data;
	uint64 *dindirect = (uint64 *) inode->blocks[TMPFS_NDIRECT + 1];
	TEST_ASSERT(dindirect != 0, "double-indirect page missing");
	TEST_ASSERT(dindirect[0] != 0 && dindirect[1] != 0,
		    "both indirect pages across idx boundary exist");
	uint64 *ind0 = (uint64 *) dindirect[0];
	uint64 *ind1 = (uint64 *) dindirect[1];
	TEST_ASSERT(ind0[TMPFS_NINDIRECT - 1] != 0,
		    "last slot of idx 0 should exist");
	TEST_ASSERT(ind1[0] != 0, "first slot of idx 1 should exist");
	TEST_ASSERT(inode->size == off + sizeof(buf),
		    "size should cover idx boundary write");
	TEST_ASSERT(tmpfs_mem_eq_at(file, off, buf, sizeof(buf)),
		    "idx boundary data mismatch");
	return 0;
}

/* ---- tmpfs read tests ---- */

static int tmpfs_read_at(struct vfs_inode *file, uint64 off, char *buf,
			 uint32 size)
{
	struct file f = {0};
	f.type = FILE_VFS_NODE;
	f.node = file;
	f.offset = off;
	return tmpfs_vfs_read(&f, (uint8 *) buf, size);
}

static int test_tmpfs_read_roundtrip()
{
	struct vfs_inode *file = tmpfs_make_file("r_round");
	TEST_ASSERT(file != 0, "create r_round failed");

	static char wbuf[PGSIZE + 100];
	for (int i = 0; i < (int) sizeof(wbuf); i++)
		wbuf[i] = 'A' + (i % 26);
	TEST_ASSERT(tmpfs_write_at(file, 0, wbuf, sizeof(wbuf)) ==
			(int) sizeof(wbuf),
		    "write for roundtrip failed");

	static char rbuf[PGSIZE + 100];
	TEST_ASSERT(tmpfs_read_at(file, 0, rbuf, sizeof(rbuf)) ==
			(int) sizeof(rbuf),
		    "read roundtrip failed");
	TEST_ASSERT(tmpfs_mem_eq(rbuf, wbuf, sizeof(wbuf)),
		    "roundtrip data mismatch");
	return 0;
}

static int test_tmpfs_read_eof()
{
	struct vfs_inode *file = tmpfs_make_file("r_eof");
	TEST_ASSERT(file != 0, "create r_eof failed");

	TEST_ASSERT(tmpfs_write_at(file, 0, "hello", 5) == 5, "write failed");

	static char rbuf[100];
	TEST_ASSERT(tmpfs_read_at(file, 0, rbuf, sizeof(rbuf)) == 5,
		    "read past EOF should clamp to file size");
	TEST_ASSERT(tmpfs_mem_eq(rbuf, "hello", 5), "eof data mismatch");

	TEST_ASSERT(tmpfs_read_at(file, 5, rbuf, sizeof(rbuf)) == 0,
		    "read at EOF should return 0");

	struct tmpfs_inode *inode = (struct tmpfs_inode *) file->private_data;
	TEST_ASSERT(inode->size == 5, "read must not change size");
	return 0;
}

static int test_tmpfs_read_level1()
{
	struct vfs_inode *file = tmpfs_make_file("r_l1");
	TEST_ASSERT(file != 0, "create r_l1 failed");

	static char wbuf[2 * PGSIZE + 10];
	for (int i = 0; i < (int) sizeof(wbuf); i++)
		wbuf[i] = 'b' + (i % 26);

	uint64 off = (uint64) TMPFS_NDIRECT * PGSIZE;
	TEST_ASSERT(tmpfs_write_at(file, off, wbuf, sizeof(wbuf)) ==
			(int) sizeof(wbuf),
		    "write level1 failed");

	static char rbuf[2 * PGSIZE + 10];
	TEST_ASSERT(tmpfs_read_at(file, off, rbuf, sizeof(rbuf)) ==
			(int) sizeof(rbuf),
		    "read level1 failed");
	TEST_ASSERT(tmpfs_mem_eq(rbuf, wbuf, sizeof(wbuf)),
		    "level1 read data mismatch");
	return 0;
}

static int test_tmpfs_read_invalid()
{
	TEST_ASSERT(tmpfs_vfs_read(0, 0, 0) == -1, "read(NULL f) should fail");

	struct file f = {0};
	f.type = FILE_VFS_NODE;
	TEST_ASSERT(tmpfs_vfs_read(&f, 0, 0) == -1,
		    "read(NULL node) should fail");

	struct vfs_inode *file = tmpfs_make_file("r_inv");
	TEST_ASSERT(file != 0, "create r_inv failed");
	f.node = file;

	TEST_ASSERT(tmpfs_vfs_read(&f, 0, 10) == -1,
		    "read(NULL buffer) should fail");

	// Offset past the file end: EOF, not an error.
	TEST_ASSERT(tmpfs_read_at(file, 100, (char *) &f, 1) == 0,
		    "read past EOF should return 0");
	return 0;
}

static int test_tmpfs_unlink()
{
	struct vfs_inode *root = tmpfs_root_vfs_inode();
	TEST_ASSERT(root != 0, "fill root failed");

	TEST_ASSERT(tmpfs_vfs_create(root, "u1", VFS_FILE) == 0,
		    "create u1 failed");
	TEST_ASSERT(tmpfs_vfs_unlink(root, "u1") == 0, "unlink u1 failed");
	TEST_ASSERT(tmpfs_vfs_lookup(root, "u1", 0) == 0,
		    "u1 should be gone after unlink");
	return 0;
}

static int test_tmpfs_unlink_notfound()
{
	struct vfs_inode *root = tmpfs_root_vfs_inode();
	TEST_ASSERT(root != 0, "fill root failed");

	TEST_ASSERT(tmpfs_vfs_unlink(root, "no_such_file") == -1,
		    "unlink missing name should fail");
	TEST_ASSERT(tmpfs_vfs_unlink(root, "") == -1,
		    "unlink empty name should fail");
	TEST_ASSERT(tmpfs_vfs_unlink(0, "x") == -1,
		    "unlink(NULL dir) should fail");
	return 0;
}

static int test_tmpfs_unlink_dir()
{
	struct vfs_inode *root = tmpfs_root_vfs_inode();
	TEST_ASSERT(root != 0, "fill root failed");

	TEST_ASSERT(tmpfs_vfs_mkdir(root, "udir", 0) == 0, "mkdir udir failed");
	TEST_ASSERT(tmpfs_vfs_unlink(root, "udir") == -1,
		    "unlink a directory should fail");

	// The directory must remain.
	struct vfs_inode *lp = tmpfs_vfs_lookup(root, "udir", 0);
	TEST_ASSERT(lp != 0 && lp->type == VFS_DIR,
		    "directory should remain after failed unlink");
	return 0;
}

static int test_tmpfs_unlink_with_data()
{
	struct vfs_inode *file = tmpfs_make_file("u_data");
	TEST_ASSERT(file != 0, "create u_data failed");

	static char buf[2 * PGSIZE];
	for (int i = 0; i < (int) sizeof(buf); i++)
		buf[i] = 'e' + (i % 26);
	TEST_ASSERT(tmpfs_write_at(file, 0, buf, sizeof(buf)) ==
			(int) sizeof(buf),
		    "write u_data failed");

	struct vfs_inode *root = tmpfs_root_vfs_inode();
	TEST_ASSERT(root != 0, "fill root failed");
	TEST_ASSERT(tmpfs_vfs_unlink(root, "u_data") == 0,
		    "unlink u_data failed");
	TEST_ASSERT(tmpfs_vfs_lookup(root, "u_data", 0) == 0,
		    "u_data should be gone");
	return 0;
}

static int test_tmpfs_unlink_open()
{
	// xv6-style: unlink of an open file keeps its contents until the
	// last reference drops (destroy_inode frees them on slot recycle).
	struct vfs_inode *file = tmpfs_make_file("u_open");
	TEST_ASSERT(file != 0, "create u_open failed");
	TEST_ASSERT(tmpfs_write_at(file, 0, "data", 4) == 4, "write failed");

	struct vfs_inode *root = tmpfs_root_vfs_inode();
	TEST_ASSERT(root != 0, "fill root failed");
	TEST_ASSERT(tmpfs_vfs_unlink(root, "u_open") == 0, "unlink failed");

	// Gone from the directory, but the open reference still sees the data.
	TEST_ASSERT(tmpfs_vfs_lookup(root, "u_open", 0) == 0,
		    "u_open should be gone from dir");
	struct tmpfs_inode *ti = (struct tmpfs_inode *) file->private_data;
	TEST_ASSERT(ti != 0 && ti->size == 4, "open file keeps contents");

	// Dropping the last reference frees the contents (deferred free).
	tmpfs_destroy_inode(file);
	return 0;
}

static int test_tmpfs_stat()
{
	struct vfs_inode *root = tmpfs_root_vfs_inode();
	TEST_ASSERT(root != 0, "fill root failed");
	struct stat st;

	TEST_ASSERT(tmpfs_vfs_stat(root, &st) == 0, "stat root failed");
	TEST_ASSERT(st.type == VFS_DIR, "root type should be dir");
	TEST_ASSERT(st.nlink >= 1, "root nlink should be >= 1");

	TEST_ASSERT(tmpfs_vfs_stat(0, &st) == -1, "stat(NULL) should fail");

	struct vfs_inode *file = tmpfs_make_file("st_file");
	TEST_ASSERT(file != 0, "create st_file failed");
	TEST_ASSERT(tmpfs_write_at(file, 0, "data", 4) == 4, "write failed");

	TEST_ASSERT(tmpfs_vfs_stat(file, &st) == 0, "stat file failed");
	TEST_ASSERT(st.type == VFS_FILE && st.size == 4 && st.nlink == 1,
		    "stat file fields mismatch");
	return 0;
}

static int test_tmpfs_truncate()
{
	struct vfs_inode *file = tmpfs_make_file("tr_file");
	TEST_ASSERT(file != 0, "create tr_file failed");

	static char buf[2 * PGSIZE];
	for (int i = 0; i < (int) sizeof(buf); i++)
		buf[i] = 't' + (i % 20);
	TEST_ASSERT(tmpfs_write_at(file, 0, buf, sizeof(buf)) ==
			(int) sizeof(buf),
		    "write before truncate failed");

	struct tmpfs_inode *ti = (struct tmpfs_inode *) file->private_data;
	TEST_ASSERT(ti->size == sizeof(buf), "size before truncate");

	TEST_ASSERT(tmpfs_vfs_truncate(file, 0) == 0, "truncate failed");
	TEST_ASSERT(ti->size == 0, "size should be 0 after truncate");
	for (int i = 0; i < TMPFS_NDIRECT; i++)
		TEST_ASSERT(ti->blocks[i] == 0,
			    "direct blocks should be freed");

	// The file is still there and writable after truncate.
	TEST_ASSERT(tmpfs_write_at(file, 0, "ok", 2) == 2, "rewrite failed");
	static char rb[4];
	TEST_ASSERT(tmpfs_read_at(file, 0, rb, 2) == 2 && rb[0] == 'o' &&
			rb[1] == 'k',
		    "rewrite verify failed");

	// Non-zero truncation is rejected (VFS only requests 0).
	TEST_ASSERT(tmpfs_vfs_truncate(file, 100) == -1,
		    "truncate(nonzero) should fail");
	return 0;
}

static int test_tmpfs_comprehensive()
{
	struct vfs_inode *root = tmpfs_root_vfs_inode();
	TEST_ASSERT(root != 0, "fill root failed");

	// 1. Build a two-level directory tree.
	TEST_ASSERT(tmpfs_vfs_mkdir(root, "comp", 0) == 0, "mkdir comp failed");
	struct vfs_inode *comp = tmpfs_vfs_lookup(root, "comp", 0);
	TEST_ASSERT(comp != 0 && comp->type == VFS_DIR, "lookup comp failed");
	TEST_ASSERT(tmpfs_vfs_mkdir(comp, "sub", 0) == 0,
		    "mkdir comp/sub failed");
	struct vfs_inode *sub = tmpfs_vfs_lookup(comp, "sub", 0);
	TEST_ASSERT(sub != 0 && sub->type == VFS_DIR, "lookup sub failed");

	// 2. Create files at both levels and write data.
	static char payload[PGSIZE + 50];
	for (int i = 0; i < (int) sizeof(payload); i++)
		payload[i] = 'x' + (i % 23);
	TEST_ASSERT(tmpfs_vfs_create(comp, "a.txt", VFS_FILE) == 0,
		    "create a.txt failed");
	struct vfs_inode *a = tmpfs_vfs_lookup(comp, "a.txt", 0);
	TEST_ASSERT(a != 0, "lookup a.txt failed");
	TEST_ASSERT(tmpfs_write_at(a, 0, payload, sizeof(payload)) ==
			(int) sizeof(payload),
		    "write a.txt failed");

	TEST_ASSERT(tmpfs_vfs_create(sub, "b.txt", VFS_FILE) == 0,
		    "create b.txt failed");
	struct vfs_inode *b = tmpfs_vfs_lookup(sub, "b.txt", 0);
	TEST_ASSERT(b != 0, "lookup b.txt failed");
	TEST_ASSERT(tmpfs_write_at(b, 0, "hello", 5) == 5,
		    "write b.txt failed");

	// 3. Read both back and verify contents.
	static char rbuf[sizeof(payload)];
	TEST_ASSERT(tmpfs_read_at(a, 0, rbuf, sizeof(rbuf)) ==
			(int) sizeof(rbuf),
		    "read a.txt failed");
	TEST_ASSERT(tmpfs_mem_eq(rbuf, payload, sizeof(payload)),
		    "a.txt data mismatch");
	static char rb2[8];
	TEST_ASSERT(tmpfs_read_at(b, 0, rb2, 5) == 5, "read b.txt failed");
	TEST_ASSERT(tmpfs_mem_eq(rb2, "hello", 5), "b.txt data mismatch");

	// 4. stat reports the right shape.
	struct stat st;
	TEST_ASSERT(tmpfs_vfs_stat(a, &st) == 0 && st.type == VFS_FILE &&
			st.size == sizeof(payload) && st.nlink == 1,
		    "stat a.txt mismatch");
	TEST_ASSERT(tmpfs_vfs_stat(comp, &st) == 0 && st.type == VFS_DIR,
		    "stat comp mismatch");

	// 5. readdir of comp lists both a.txt and sub.
	struct file f = {0};
	f.type = FILE_VFS_NODE;
	f.node = comp;
	f.offset = 0;
	struct vfs_dirent de;
	int seen_a = 0;
	int seen_sub = 0;
	while (tmpfs_vfs_readdir(&f, &de) == 1) {
		if (de.name[0] == 'a' && de.name[1] == '.' && de.name[2] == 't')
			seen_a = 1;
		if (de.name[0] == 's' && de.name[1] == 'u' && de.name[2] == 'b')
			seen_sub = 1;
	}
	TEST_ASSERT(seen_a && seen_sub,
		    "readdir comp should list a.txt and sub");

	// 6. Unlink b.txt from sub and confirm it disappears.
	struct vfs_inode *sub2 = tmpfs_vfs_lookup(comp, "sub", 0);
	TEST_ASSERT(sub2 != 0, "relookup sub failed");
	TEST_ASSERT(tmpfs_vfs_unlink(sub2, "b.txt") == 0,
		    "unlink b.txt failed");
	TEST_ASSERT(tmpfs_vfs_lookup(sub2, "b.txt", 0) == 0,
		    "b.txt should be gone");

	// 7. Truncate a.txt, then reuse the file.
	TEST_ASSERT(tmpfs_vfs_truncate(a, 0) == 0, "truncate a.txt failed");
	struct tmpfs_inode *ta = (struct tmpfs_inode *) a->private_data;
	TEST_ASSERT(ta->size == 0, "a.txt size should be 0 after truncate");
	TEST_ASSERT(tmpfs_write_at(a, 0, "hi", 2) == 2, "rewrite a.txt failed");
	TEST_ASSERT(tmpfs_read_at(a, 0, rb2, 2) == 2 && rb2[0] == 'h' &&
			rb2[1] == 'i',
		    "a.txt rewrite verify failed");

	return 0;
}

void tmpfs_test(void)
{
	RUN_TEST(test_tmpfs_root_init);
	RUN_TEST(test_tmpfs_create_and_lookup);
	RUN_TEST(test_tmpfs_create_file);
	RUN_TEST(test_tmpfs_duplicate_name);
	RUN_TEST(test_tmpfs_lookup_not_found);
	RUN_TEST(test_tmpfs_invalid_args);
	RUN_TEST(test_tmpfs_ino_unique);
	RUN_TEST(test_tmpfs_readdir_empty);
	RUN_TEST(test_tmpfs_readdir_list);
	RUN_TEST(test_tmpfs_readdir_invalid);
	RUN_TEST(test_tmpfs_mkdir);
	RUN_TEST(test_tmpfs_mkdir_duplicate);
	RUN_TEST(test_tmpfs_mkdir_invalid);
	RUN_TEST(test_tmpfs_mkdir_nlinks);
	RUN_TEST(test_tmpfs_mkdir_subfile);
	RUN_TEST(test_tmpfs_write_basic);
	RUN_TEST(test_tmpfs_write_cross_page);
	RUN_TEST(test_tmpfs_write_offset);
	RUN_TEST(test_tmpfs_write_level1);
	RUN_TEST(test_tmpfs_write_level2);
	RUN_TEST(test_tmpfs_write_invalid);
	RUN_TEST(test_tmpfs_write_size_max);
	RUN_TEST(test_tmpfs_write_fill_direct);
	RUN_TEST(test_tmpfs_write_boundary_l1);
	RUN_TEST(test_tmpfs_write_fill_indirect);
	RUN_TEST(test_tmpfs_write_boundary_l2);
	RUN_TEST(test_tmpfs_write_boundary_idx);
	RUN_TEST(test_tmpfs_read_roundtrip);
	RUN_TEST(test_tmpfs_read_eof);
	RUN_TEST(test_tmpfs_read_level1);
	RUN_TEST(test_tmpfs_read_invalid);
	RUN_TEST(test_tmpfs_unlink);
	RUN_TEST(test_tmpfs_unlink_notfound);
	RUN_TEST(test_tmpfs_unlink_dir);
	RUN_TEST(test_tmpfs_unlink_with_data);
	RUN_TEST(test_tmpfs_unlink_open);
	RUN_TEST(test_tmpfs_stat);
	RUN_TEST(test_tmpfs_truncate);
	RUN_TEST(test_tmpfs_comprehensive);
}
