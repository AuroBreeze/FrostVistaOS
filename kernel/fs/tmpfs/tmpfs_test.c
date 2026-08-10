#include "kernel/defs.h"
#include "kernel/fs.h"
#define LOG_MODULE "TMPFS TEST"

#include "kernel/test.h"
#include "tmpfs.h"

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
}
