#include "kernel/fs.h"
#define LOG_MODULE "TMPFS TEST"

#include "kernel/test.h"
#include "tmpfs.h"

static int test_tmpfs_create_and_lookup()
{
	struct tmpfs_dir_entry *root = tmpfs_get_root_dir_entry();
	struct tmpfs_inode root_inode = {
	    .dir = root,
	    .nlinks = 1,
	    .type = VFS_DIR,
	};

	struct vfs_inode *inode =
	    tmpfs_fill_vfs_inode(TMPFS_ROOT_INO, &root_inode, VFS_DIR);

	int n = tmpfs_vfs_create(inode, "test", VFS_DIR);
	TEST_ASSERT(n == 0, "Create test dir failed");

	struct vfs_inode *lp = tmpfs_vfs_lookup(inode, "test", 0);
	TEST_ASSERT(lp != 0, "Lookup test dir failed");
	return 0;
}

void tmpfs_test(void)
{
	RUN_TEST(test_tmpfs_create_and_lookup);
}
