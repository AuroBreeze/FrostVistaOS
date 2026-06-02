#include "user.h"
#include "libtest.h"

#define PIPE_BUF_SIZE 512

static void fill_pattern(char *buf, int len)
{
	for (int i = 0; i < len; i++)
		buf[i] = 'A' + (i % 26);
}

static int pattern_matches(const char *buf, int offset, int len)
{
	for (int i = 0; i < len; i++) {
		char expected = 'A' + ((offset + i) % 26);
		if (buf[i] != expected)
			return 0;
	}
	return 1;
}

static int bytes_equal(const char *a, const char *b, int len)
{
	for (int i = 0; i < len; i++) {
		if (a[i] != b[i])
			return 0;
	}
	return 1;
}

static void test_parent_write_child_read(void)
{
	int fds[2];
	char buf[32] = {0};
	long ret = pipe2(fds, 0);
	printf("fork parent->child pipe2 -> %d, read=%d write=%d\n", (int) ret,
	       fds[0], fds[1]);
	TEST_ASSERT(ret == 0, "sys_pipe",
		    "fork parent->child pipe2 should work");

	int pid = fork();
	if (pid < 0) {
		TEST_FAIL("sys_pipe");
	} else if (pid == 0) {
		ret = close(fds[1]);
		printf("[child parent->child] close write fd -> %d\n",
		       (int) ret);
		TEST_ASSERT(ret == 0, "sys_pipe",
			    "child should close unused write end");
		ret = read(fds[0], buf, sizeof(buf));
		printf("[child parent->child] read -> %d, buf=%s\n", (int) ret,
		       buf);
		TEST_ASSERT(ret == 12, "sys_pipe",
			    "child should read parent message");
		TEST_ASSERT(bytes_equal(buf, "from-parent\n", 12), "sys_pipe",
			    "child should receive parent data");
		ret = read(fds[0], buf, sizeof(buf));
		printf("[child parent->child] read EOF -> %d\n", (int) ret);
		TEST_ASSERT(
		    ret == 0, "sys_pipe",
		    "child should see EOF after parent closes write end");
		ret = close(fds[0]);
		printf("[child parent->child] close read fd -> %d\n",
		       (int) ret);
		TEST_ASSERT(ret == 0, "sys_pipe",
			    "child should close read end");
		exit(0);
	}

	ret = close(fds[0]);
	printf("[parent parent->child] close read fd -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "sys_pipe",
		    "parent should close unused read end");
	ret = write(fds[1], "from-parent\n", 12);
	printf("[parent parent->child] write -> %d\n", (int) ret);
	TEST_ASSERT(ret == 12, "sys_pipe", "parent should write child message");
	ret = close(fds[1]);
	printf("[parent parent->child] close write fd -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "sys_pipe", "parent should close write end");
	int reaped = wait();
	printf("[parent parent->child] wait -> %d\n", reaped);
	TEST_ASSERT(reaped == pid, "sys_pipe",
		    "parent should reap reader child");
}

static void test_child_write_parent_read(void)
{
	int fds[2];
	char buf[32] = {0};
	long ret = pipe2(fds, 0);
	printf("fork child->parent pipe2 -> %d, read=%d write=%d\n", (int) ret,
	       fds[0], fds[1]);
	TEST_ASSERT(ret == 0, "sys_pipe",
		    "fork child->parent pipe2 should work");

	int pid = fork();
	if (pid < 0) {
		TEST_FAIL("sys_pipe");
	} else if (pid == 0) {
		ret = close(fds[0]);
		printf("[child child->parent] close read fd -> %d\n",
		       (int) ret);
		TEST_ASSERT(ret == 0, "sys_pipe",
			    "child should close unused read end");
		ret = write(fds[1], "from-child\n", 11);
		printf("[child child->parent] write -> %d\n", (int) ret);
		TEST_ASSERT(ret == 11, "sys_pipe",
			    "child should write parent message");
		ret = close(fds[1]);
		printf("[child child->parent] close write fd -> %d\n",
		       (int) ret);
		TEST_ASSERT(ret == 0, "sys_pipe",
			    "child should close write end");
		exit(0);
	}

	ret = close(fds[1]);
	printf("[parent child->parent] close write fd -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "sys_pipe",
		    "parent should close unused write end");
	ret = read(fds[0], buf, sizeof(buf));
	printf("[parent child->parent] read -> %d, buf=%s\n", (int) ret, buf);
	TEST_ASSERT(ret == 11, "sys_pipe", "parent should read child message");
	TEST_ASSERT(bytes_equal(buf, "from-child\n", 11), "sys_pipe",
		    "parent should receive child data");
	ret = read(fds[0], buf, sizeof(buf));
	printf("[parent child->parent] read EOF -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "sys_pipe",
		    "parent should see EOF after child closes write end");
	ret = close(fds[0]);
	printf("[parent child->parent] close read fd -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "sys_pipe", "parent should close read end");
	int reaped = wait();
	printf("[parent child->parent] wait -> %d\n", reaped);
	TEST_ASSERT(reaped == pid, "sys_pipe",
		    "parent should reap writer child");
}

void _start()
{
	TEST_START("sys_pipe");
	long ret;
	int fds[2] = {-1, -1};

	ret = pipe2(fds, 1);
	printf("pipe2(fds, 1) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "sys_pipe", "pipe2 unsupported flags should fail");
	TEST_ASSERT(fds[0] == -1 && fds[1] == -1, "sys_pipe",
		    "pipe2 failed flags should not update fds");

	ret = pipe2(0, 0);
	printf("pipe2(NULL, 0) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "sys_pipe", "pipe2 null fd buffer should fail");

	ret = pipe2(fds, 0);
	printf("pipe2(fds, 0) -> %d, read=%d write=%d\n", (int) ret, fds[0],
	       fds[1]);
	TEST_ASSERT(ret == 0, "sys_pipe", "pipe2 should succeed");
	TEST_ASSERT(fds[0] >= 0 && fds[1] >= 0, "sys_pipe",
		    "pipe2 should return valid fds");
	TEST_ASSERT(fds[0] != fds[1], "sys_pipe",
		    "pipe read/write fds should differ");

	char small[32] = {0};
	ret = read(fds[1], small, 1);
	printf("read(write fd, small, 1) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "sys_pipe", "write end should not be readable");
	ret = write(fds[0], "x", 1);
	printf("write(read fd, x, 1) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "sys_pipe", "read end should not be writable");

	ret = write(fds[1], "", 0);
	printf("write(write fd, empty, 0) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "sys_pipe",
		    "zero-length pipe write should succeed");

	ret = write(fds[1], "hello pipe", 10);
	printf("write(write fd, hello pipe, 10) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 10, "sys_pipe", "small pipe write should complete");
	ret = read(fds[0], small, 0);
	printf("read(read fd, small, 0) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "sys_pipe",
		    "zero-length pipe read should succeed");
	ret = read(fds[0], small, 5);
	printf("read(read fd, small, 5) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 5, "sys_pipe", "partial pipe read should return 5");
	TEST_ASSERT(bytes_equal(small, "hello", 5), "sys_pipe",
		    "partial pipe read should preserve order");
	memset(small, 0, sizeof(small));
	ret = read(fds[0], small, sizeof(small));
	printf("read(read fd, rest, 32) -> %d, buf=%s\n", (int) ret, small);
	TEST_ASSERT(ret == 5, "sys_pipe",
		    "read larger than available should drain");
	TEST_ASSERT(bytes_equal(small, " pipe", 5), "sys_pipe",
		    "remaining pipe data should match");

	char big[PIPE_BUF_SIZE];
	char out[PIPE_BUF_SIZE];
	fill_pattern(big, sizeof(big));
	memset(out, 0, sizeof(out));
	ret = write(fds[1], big, sizeof(big));
	printf("write(write fd, 512-byte pattern) -> %d\n", (int) ret);
	TEST_ASSERT(ret == PIPE_BUF_SIZE, "sys_pipe",
		    "pipe should accept one full buffer");
	ret = read(fds[0], out, 200);
	printf("read(read fd, out, 200) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 200, "sys_pipe",
		    "first bulk read should return 200");
	TEST_ASSERT(pattern_matches(out, 0, 200), "sys_pipe",
		    "first bulk chunk should match pattern");
	ret = read(fds[0], out, 200);
	printf("read(read fd, out, 200) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 200, "sys_pipe",
		    "second bulk read should return 200");
	TEST_ASSERT(pattern_matches(out, 200, 200), "sys_pipe",
		    "second bulk chunk should match pattern");
	ret = read(fds[0], out, sizeof(out));
	printf("read(read fd, out, 512) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 112, "sys_pipe",
		    "final bulk read should return 112");
	TEST_ASSERT(pattern_matches(out, 400, 112), "sys_pipe",
		    "final bulk chunk should match pattern");

	ret = close(fds[1]);
	printf("close(write fd) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "sys_pipe", "close write end should succeed");
	ret = read(fds[0], small, sizeof(small));
	printf("read(read fd after write close) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "sys_pipe",
		    "closed write end should produce EOF");
	ret = close(fds[0]);
	printf("close(read fd) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "sys_pipe", "close read end should succeed");

	ret = pipe2(fds, 0);
	printf("pipe2(second fds, 0) -> %d, read=%d write=%d\n", (int) ret,
	       fds[0], fds[1]);
	TEST_ASSERT(ret == 0, "sys_pipe", "second pipe2 should succeed");
	ret = close(fds[0]);
	printf("close(second read fd) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "sys_pipe",
		    "close second read end should succeed");
	ret = write(fds[1], "z", 1);
	printf("write(write fd after read close) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "sys_pipe",
		    "closed read end should reject writes");
	ret = close(fds[1]);
	printf("close(second write fd) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "sys_pipe",
		    "close second write end should succeed");

	test_parent_write_child_read();
	test_child_write_parent_read();

	TEST_PASS("sys_pipe");
	shutdown();
}
