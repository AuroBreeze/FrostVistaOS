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

void tmpfs_test(void)
{
	RUN_TEST(test_tmpfs_root_init);
	RUN_TEST(test_tmpfs_create_and_lookup);
	RUN_TEST(test_tmpfs_create_file);
	RUN_TEST(test_tmpfs_duplicate_name);
	RUN_TEST(test_tmpfs_lookup_not_found);
	RUN_TEST(test_tmpfs_invalid_args);
	RUN_TEST(test_tmpfs_ino_unique);
}
