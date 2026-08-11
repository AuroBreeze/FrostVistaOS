#include "kernel/defs.h"
#define LOG_MODULE "EXT4 MIX TEST"

#include "kernel/fs.h"
#include "kernel/test.h"
#include "ext4.h"
#include "mix.h"
#include "../tmpfs/tmpfs.h"

/* Byte compare: the kernel has no memcmp. */
static int test_mem_eq(const void *a, const void *b, uint64 n)
{
	const uint8 *pa = (const uint8 *) a;
	const uint8 *pb = (const uint8 *) b;
	for (uint64 i = 0; i < n; i++) {
		if (pa[i] != pb[i])
			return 0;
	}
	return 1;
}

static struct vfs_inode *mix_test_namei(const char *path)
{
	extern struct vfs_inode *vfs_root;
	return vfs_lookup_at(vfs_root, (char *) path);
}

static int test_overlay_create_lookup()
{
	struct vfs_inode *musl = mix_test_namei("/musl");
	TEST_ASSERT(musl != 0, "lookup /musl failed");
	TEST_ASSERT(musl->dev != TMPFS_DEV, "/musl should be an ext4 dir");

	TEST_ASSERT(mix_vfs_create(musl, "ovf", VFS_FILE) == 0,
		    "create /musl/ovf failed");
	struct vfs_inode *lp = mix_vfs_lookup(musl, "ovf", 0);
	TEST_ASSERT(lp != 0, "lookup ovf failed");
	TEST_ASSERT(lp->dev == TMPFS_DEV, "ovf should be a tmpfs node");
	TEST_ASSERT(mix_vfs_create(musl, "ovf", VFS_FILE) == -1,
		    "duplicate create should fail");

	put_inode(lp, 0);
	put_inode(musl, 0);
	return 0;
}

static int test_overlay_write_read()
{
	struct vfs_inode *musl = mix_test_namei("/musl");
	TEST_ASSERT(musl != 0, "lookup /musl failed");

	TEST_ASSERT(mix_vfs_create(musl, "ovw", VFS_FILE) == 0,
		    "create ovw failed");
	struct vfs_inode *lp = mix_vfs_lookup(musl, "ovw", 0);
	TEST_ASSERT(lp != 0, "lookup ovw failed");

	struct file f = {0};
	f.type = FILE_VFS_NODE;
	f.node = lp;
	f.offset = 0;
	TEST_ASSERT(mix_vfs_write(&f, (uint8 *) "hello", 5) == 5,
		    "write ovw failed");
	TEST_ASSERT(f.node->dev == TMPFS_DEV, "ovw node should be tmpfs");

	f.offset = 0;
	char buf[8] = {0};
	TEST_ASSERT(mix_vfs_read(&f, (uint8 *) buf, 5) == 5 && buf[0] == 'h' &&
			buf[4] == 'o',
		    "read ovw failed");

	put_inode(lp, 0);
	put_inode(musl, 0);
	return 0;
}

static int test_overlay_readdir()
{
	struct vfs_inode *musl = mix_test_namei("/musl");
	TEST_ASSERT(musl != 0, "lookup /musl failed");
	TEST_ASSERT(mix_vfs_create(musl, "ovd", VFS_FILE) == 0,
		    "create ovd failed");

	struct file f = {0};
	f.type = FILE_VFS_NODE;
	f.node = musl;
	f.offset = 0;
	struct vfs_dirent de;
	int seen = 0;
	while (mix_vfs_readdir(&f, &de) == 1) {
		if (de.name[0] == 'o' && de.name[1] == 'v' && de.name[2] == 'd')
			seen++;
	}
	TEST_ASSERT(seen == 1, "ovd should appear exactly once in /musl");

	put_inode(musl, 0);
	return 0;
}

static int test_overlay_unlink()
{
	struct vfs_inode *musl = mix_test_namei("/musl");
	TEST_ASSERT(musl != 0, "lookup /musl failed");

	TEST_ASSERT(mix_vfs_create(musl, "ovu", VFS_FILE) == 0,
		    "create ovu failed");
	TEST_ASSERT(mix_vfs_unlink(musl, "ovu") == 0, "unlink ovu failed");
	TEST_ASSERT(mix_vfs_lookup(musl, "ovu", 0) == 0,
		    "ovu should be gone after unlink");

	put_inode(musl, 0);
	return 0;
}

static int test_overlay_copyup()
{
	struct vfs_inode *basic = mix_test_namei("/musl/basic");
	TEST_ASSERT(basic != 0, "lookup /musl/basic failed");

	struct vfs_inode *lower = mix_vfs_lookup(basic, "test.txt", 0);
	if (lower == 0 || lower->dev == TMPFS_DEV) {
		/* no lower ext4 test.txt to copy up; skip */
		if (lower != 0)
			put_inode(lower, 0);
		put_inode(basic, 0);
		LOG_WARN("mix_test: skip copy-up (no lower test.txt)");
		return 0;
	}

	/* read the original content, then write it back unchanged: this
	 * triggers copy-up without altering the visible content */
	struct file f = {0};
	f.type = FILE_VFS_NODE;
	f.node = lower;
	f.offset = 0;
	char orig[128];
	int n = mix_vfs_read(&f, (uint8 *) orig, sizeof(orig));
	TEST_ASSERT(n > 0, "read lower test.txt failed");

	f.offset = 0;
	TEST_ASSERT(mix_vfs_write(&f, (uint8 *) orig, n) == n,
		    "copy-up write failed");
	TEST_ASSERT(f.node->dev == TMPFS_DEV,
		    "node should switch to tmpfs after copy-up");

	f.offset = 0;
	char back[128];
	TEST_ASSERT(mix_vfs_read(&f, (uint8 *) back, n) == n &&
			test_mem_eq(orig, back, n),
		    "copy-up read-back mismatch");

	put_inode(lower, 0);
	put_inode(basic, 0);
	return 0;
}

static int test_overlay_unlink_dir()
{
	struct vfs_inode *musl = mix_test_namei("/musl");
	TEST_ASSERT(musl != 0, "lookup /musl failed");

	TEST_ASSERT(mix_vfs_mkdir(musl, "ovdir", 0) == 0, "mkdir ovdir failed");
	TEST_ASSERT(mix_vfs_unlink(musl, "ovdir") == -1,
		    "unlink of an overlay directory should fail");
	struct vfs_inode *lp = mix_vfs_lookup(musl, "ovdir", 0);
	TEST_ASSERT(lp != 0 && lp->type == VFS_DIR,
		    "ovdir should remain after failed unlink");

	put_inode(lp, 0);
	put_inode(musl, 0);
	return 0;
}

static int test_overlay_recreate()
{
	struct vfs_inode *musl = mix_test_namei("/musl");
	TEST_ASSERT(musl != 0, "lookup /musl failed");

	/* create -> unlink -> recreate: the overlay slot must be reusable */
	for (int i = 0; i < 5; i++) {
		TEST_ASSERT(mix_vfs_create(musl, "ovrc", VFS_FILE) == 0,
			    "recreate ovrc failed");
		TEST_ASSERT(mix_vfs_unlink(musl, "ovrc") == 0,
			    "unlink ovrc failed");
		TEST_ASSERT(mix_vfs_lookup(musl, "ovrc", 0) == 0,
			    "ovrc should be gone");
	}
	TEST_ASSERT(mix_vfs_create(musl, "ovrc", VFS_FILE) == 0,
		    "final recreate failed");
	struct vfs_inode *lp = mix_vfs_lookup(musl, "ovrc", 0);
	TEST_ASSERT(lp != 0, "lookup after recreate failed");
	TEST_ASSERT(mix_vfs_unlink(musl, "ovrc") == 0, "final unlink failed");

	put_inode(lp, 0);
	put_inode(musl, 0);
	return 0;
}

static int test_overlay_dot()
{
	struct vfs_inode *musl = mix_test_namei("/musl");
	TEST_ASSERT(musl != 0, "lookup /musl failed");

	struct vfs_inode *dot = mix_vfs_lookup(musl, ".", 0);
	TEST_ASSERT(dot != 0, "lookup of . failed");
	TEST_ASSERT(dot->dev == musl->dev && dot->ino == musl->ino,
		    ". should resolve to the directory itself");

	put_inode(dot, 0);
	put_inode(musl, 0);
	return 0;
}

static int test_overlay_unlink_missing()
{
	struct vfs_inode *musl = mix_test_namei("/musl");
	TEST_ASSERT(musl != 0, "lookup /musl failed");

	TEST_ASSERT(mix_vfs_unlink(musl, "ov_nonexistent") == -1,
		    "unlink of a missing name should fail");
	TEST_ASSERT(mix_vfs_unlink(musl, "") == -1,
		    "unlink of an empty name should fail");

	put_inode(musl, 0);
	return 0;
}

static int test_overlay_read_lower()
{
	struct vfs_inode *basic = mix_test_namei("/musl/basic");
	TEST_ASSERT(basic != 0, "lookup /musl/basic failed");

	struct vfs_inode *lower = mix_vfs_lookup(basic, "test.txt", 0);
	if (lower == 0 || lower->dev == TMPFS_DEV) {
		/* no un-copied-up lower test.txt; skip */
		if (lower != 0)
			put_inode(lower, 0);
		put_inode(basic, 0);
		LOG_WARN("mix_test: skip read-lower (no lower test.txt)");
		return 0;
	}

	/* direct read of the read-only lower file (no copy-up) */
	struct file f = {0};
	f.type = FILE_VFS_NODE;
	f.node = lower;
	f.offset = 0;
	char buf[64];
	int n = mix_vfs_read(&f, (uint8 *) buf, sizeof(buf));
	TEST_ASSERT(n > 0, "read lower test.txt should return data");
	TEST_ASSERT(f.node->dev != TMPFS_DEV,
		    "lower node must stay ext4 (no copy-up on read)");

	put_inode(lower, 0);
	put_inode(basic, 0);
	return 0;
}

void mix_test(void)
{
	RUN_TEST(test_overlay_create_lookup);
	RUN_TEST(test_overlay_write_read);
	RUN_TEST(test_overlay_readdir);
	RUN_TEST(test_overlay_unlink);
	RUN_TEST(test_overlay_copyup);
	RUN_TEST(test_overlay_unlink_dir);
	RUN_TEST(test_overlay_recreate);
	RUN_TEST(test_overlay_dot);
	RUN_TEST(test_overlay_unlink_missing);
	RUN_TEST(test_overlay_read_lower);
}
