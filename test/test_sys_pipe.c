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

static void test_pipe_creation_errors(void)
{
	TEST_START("test_pipe_creation_errors");
	int fds[2] = {-1, -1};
	long ret = pipe2(fds, 1);
	printf("pipe2(fds, 1) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "test_pipe_creation_errors",
		    "pipe2 unsupported flags should fail");
	TEST_ASSERT(fds[0] == -1 && fds[1] == -1, "test_pipe_creation_errors",
		    "pipe2 failed flags should not update fds");

	ret = pipe2(0, 0);
	printf("pipe2(NULL, 0) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "test_pipe_creation_errors",
		    "pipe2 null fd buffer should fail");
	TEST_PASS("test_pipe_creation_errors");
}

static void test_pipe_basic_rw(void)
{
	TEST_START("test_pipe_basic_rw");
	int fds[2] = {-1, -1};
	long ret = pipe2(fds, 0);
	printf("pipe2(fds, 0) -> %d, read=%d write=%d\n", (int) ret, fds[0],
	       fds[1]);
	TEST_ASSERT(ret == 0, "test_pipe_basic_rw", "pipe2 should succeed");
	TEST_ASSERT(fds[0] >= 0 && fds[1] >= 0, "test_pipe_basic_rw",
		    "pipe2 should return valid fds");
	TEST_ASSERT(fds[0] != fds[1], "test_pipe_basic_rw",
		    "pipe read/write fds should differ");

	char small[32] = {0};
	ret = read(fds[1], small, 1);
	printf("read(write fd, small, 1) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "test_pipe_basic_rw",
		    "write end should not be readable");
	ret = write(fds[0], "x", 1);
	printf("write(read fd, x, 1) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "test_pipe_basic_rw",
		    "read end should not be writable");

	ret = write(fds[1], "", 0);
	printf("write(write fd, empty, 0) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "test_pipe_basic_rw",
		    "zero-length pipe write should succeed");

	ret = write(fds[1], "hello pipe", 10);
	printf("write(write fd, hello pipe, 10) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 10, "test_pipe_basic_rw",
		    "small pipe write should complete");
	ret = read(fds[0], small, 0);
	printf("read(read fd, small, 0) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "test_pipe_basic_rw",
		    "zero-length pipe read should succeed");
	ret = read(fds[0], small, 5);
	printf("read(read fd, small, 5) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 5, "test_pipe_basic_rw",
		    "partial pipe read should return 5");
	TEST_ASSERT(bytes_equal(small, "hello", 5), "test_pipe_basic_rw",
		    "partial pipe read should preserve order");
	memset(small, 0, sizeof(small));
	ret = read(fds[0], small, sizeof(small));
	printf("read(read fd, rest, 32) -> %d, buf=%s\n", (int) ret, small);
	TEST_ASSERT(ret == 5, "test_pipe_basic_rw",
		    "read larger than available should drain");
	TEST_ASSERT(bytes_equal(small, " pipe", 5), "test_pipe_basic_rw",
		    "remaining pipe data should match");

	close(fds[0]);
	close(fds[1]);
	TEST_PASS("test_pipe_basic_rw");
}

static void test_pipe_bulk_pattern(void)
{
	TEST_START("test_pipe_bulk_pattern");
	int fds[2];
	long ret = pipe2(fds, 0);
	TEST_ASSERT(ret == 0, "test_pipe_bulk_pattern",
		    "bulk pattern pipe2 should work");

	char big[PIPE_BUF_SIZE];
	char out[PIPE_BUF_SIZE];
	fill_pattern(big, sizeof(big));
	memset(out, 0, sizeof(out));
	ret = write(fds[1], big, sizeof(big));
	printf("write(write fd, 512-byte pattern) -> %d\n", (int) ret);
	TEST_ASSERT(ret == PIPE_BUF_SIZE, "test_pipe_bulk_pattern",
		    "pipe should accept one full buffer");
	ret = read(fds[0], out, 200);
	printf("read(read fd, out, 200) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 200, "test_pipe_bulk_pattern",
		    "first bulk read should return 200");
	TEST_ASSERT(pattern_matches(out, 0, 200), "test_pipe_bulk_pattern",
		    "first bulk chunk should match pattern");
	ret = read(fds[0], out, 200);
	printf("read(read fd, out, 200) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 200, "test_pipe_bulk_pattern",
		    "second bulk read should return 200");
	TEST_ASSERT(pattern_matches(out, 200, 200), "test_pipe_bulk_pattern",
		    "second bulk chunk should match pattern");
	ret = read(fds[0], out, sizeof(out));
	printf("read(read fd, out, 512) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 112, "test_pipe_bulk_pattern",
		    "final bulk read should return 112");
	TEST_ASSERT(pattern_matches(out, 400, 112), "test_pipe_bulk_pattern",
		    "final bulk chunk should match pattern");

	close(fds[0]);
	close(fds[1]);
	TEST_PASS("test_pipe_bulk_pattern");
}

static void test_pipe_close_semantics(void)
{
	TEST_START("test_pipe_close_semantics");
	int fds[2];
	long ret = pipe2(fds, 0);
	TEST_ASSERT(ret == 0, "test_pipe_close_semantics",
		    "close-semantics pipe2 should work");

	char small[32] = {0};
	close(fds[1]);
	ret = read(fds[0], small, sizeof(small));
	printf("read(read fd after write close) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "test_pipe_close_semantics",
		    "closed write end should produce EOF");
	close(fds[0]);

	ret = pipe2(fds, 0);
	TEST_ASSERT(ret == 0, "test_pipe_close_semantics",
		    "second pipe2 should succeed");
	close(fds[0]);
	ret = write(fds[1], "z", 1);
	printf("write(write fd after read close) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "test_pipe_close_semantics",
		    "closed read end should reject writes");
	close(fds[1]);
	TEST_PASS("test_pipe_close_semantics");
}

static void test_pipe_user_buffer_faults(void)
{
	TEST_START("test_pipe_user_buffer_faults");
	int fds[2];
	char buf[8] = {0};
	long ret = pipe2(fds, 0);
	printf("fault pipe2 -> %d, read=%d write=%d\n", (int) ret, fds[0],
	       fds[1]);
	TEST_ASSERT(ret == 0, "test_pipe_user_buffer_faults",
		    "fault pipe2 should work");

	ret = write(fds[1], 0, 1);
	printf("write(pipe write fd, NULL, 1) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "test_pipe_user_buffer_faults",
		    "pipe write null buffer should fail");

	ret = write(fds[1], "q", 1);
	printf("write(pipe write fd, q, 1) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 1, "test_pipe_user_buffer_faults",
		    "pipe write after copyin fault works");
	ret = read(fds[0], 0, 1);
	printf("read(pipe read fd, NULL, 1) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "test_pipe_user_buffer_faults",
		    "pipe read null buffer should fail");
	ret = read(fds[0], buf, 1);
	printf("read(pipe read fd, buf, 1) -> %d, byte=%d\n", (int) ret,
	       buf[0]);
	TEST_ASSERT(ret == 1 && buf[0] == 'q', "test_pipe_user_buffer_faults",
		    "failed pipe copyout should not consume data");

	close(fds[0]);
	close(fds[1]);
	TEST_PASS("test_pipe_user_buffer_faults");
}

static void test_dup_lifetime(void)
{
	TEST_START("test_dup_lifetime");
	int fds[2];
	char buf[16] = {0};
	long ret = pipe2(fds, 0);
	printf("dup lifetime pipe2 -> %d, read=%d write=%d\n", (int) ret,
	       fds[0], fds[1]);
	TEST_ASSERT(ret == 0, "test_dup_lifetime",
		    "dup lifetime pipe2 should work");

	int dup_read = dup3(fds[0], 20, 0);
	printf("dup3(read fd, 20, 0) -> %d\n", dup_read);
	TEST_ASSERT(dup_read == 20, "test_dup_lifetime",
		    "dup read fd should succeed");
	int dup_write = dup3(fds[1], 21, 0);
	printf("dup3(write fd, 21, 0) -> %d\n", dup_write);
	TEST_ASSERT(dup_write == 21, "test_dup_lifetime",
		    "dup write fd should succeed");

	close(fds[0]);
	close(fds[1]);

	ret = write(dup_write, "dup-live", 8);
	printf("write(dup write fd, dup-live, 8) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 8, "test_dup_lifetime",
		    "dup write fd should stay live");
	close(dup_write);

	ret = read(dup_read, buf, sizeof(buf));
	printf("read(dup read fd, buf, 16) -> %d, buf=%s\n", (int) ret, buf);
	TEST_ASSERT(ret == 8, "test_dup_lifetime",
		    "dup read fd should read data");
	TEST_ASSERT(bytes_equal(buf, "dup-live", 8), "test_dup_lifetime",
		    "dup pipe data should match");
	ret = read(dup_read, buf, sizeof(buf));
	printf("read(dup read fd EOF) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "test_dup_lifetime",
		    "dup read fd should see EOF after final writer closes");
	close(dup_read);
	TEST_PASS("test_dup_lifetime");
}

static void test_exit_closes_writer(void)
{
	TEST_START("test_exit_closes_writer");
	int fds[2];
	char buf[8] = {0};
	long ret = pipe2(fds, 0);
	printf("exit writer pipe2 -> %d, read=%d write=%d\n", (int) ret, fds[0],
	       fds[1]);
	TEST_ASSERT(ret == 0, "test_exit_closes_writer",
		    "exit writer pipe2 should work");

	int pid = fork();
	if (pid < 0) {
		TEST_FAIL("test_exit_closes_writer");
	} else if (pid == 0) {
		printf("[child exit writer] exiting with inherited write fd\n");
		exit(0);
	}

	close(fds[1]);
	printf("[parent exit writer] close own write fd -> %d\n", (int) ret);
	int reaped = wait();
	printf("[parent exit writer] wait -> %d\n", reaped);
	TEST_ASSERT(reaped == pid, "test_exit_closes_writer",
		    "parent reaps writer child");
	ret = read(fds[0], buf, sizeof(buf));
	printf("[parent exit writer] read EOF -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "test_exit_closes_writer",
		    "child exit should close inherited write fd");
	close(fds[0]);
	TEST_PASS("test_exit_closes_writer");
}

static void test_exit_closes_reader(void)
{
	TEST_START("test_exit_closes_reader");
	int fds[2];
	long ret = pipe2(fds, 0);
	printf("exit reader pipe2 -> %d, read=%d write=%d\n", (int) ret, fds[0],
	       fds[1]);
	TEST_ASSERT(ret == 0, "test_exit_closes_reader",
		    "exit reader pipe2 should work");

	int pid = fork();
	if (pid < 0) {
		TEST_FAIL("test_exit_closes_reader");
	} else if (pid == 0) {
		printf("[child exit reader] exiting with inherited read fd\n");
		exit(0);
	}

	close(fds[0]);
	printf("[parent exit reader] close own read fd -> %d\n", (int) ret);
	int reaped = wait();
	printf("[parent exit reader] wait -> %d\n", reaped);
	TEST_ASSERT(reaped == pid, "test_exit_closes_reader",
		    "parent reaps reader child");
	ret = write(fds[1], "x", 1);
	printf("[parent exit reader] write after reader exit -> %d\n",
	       (int) ret);
	TEST_ASSERT(ret < 0, "test_exit_closes_reader",
		    "child exit should close inherited read fd");
	close(fds[1]);
	TEST_PASS("test_exit_closes_reader");
}

static void test_parent_write_child_read(void)
{
	TEST_START("test_parent_write_child_read");
	int fds[2];
	char buf[32] = {0};
	long ret = pipe2(fds, 0);
	printf("fork parent->child pipe2 -> %d, read=%d write=%d\n", (int) ret,
	       fds[0], fds[1]);
	TEST_ASSERT(ret == 0, "test_parent_write_child_read",
		    "fork parent->child pipe2 should work");

	int pid = fork();
	if (pid < 0) {
		TEST_FAIL("test_parent_write_child_read");
	} else if (pid == 0) {
		close(fds[1]);
		printf("[child parent->child] close write fd -> %d\n",
		       (int) ret);
		ret = read(fds[0], buf, sizeof(buf));
		printf("[child parent->child] read -> %d, buf=%s\n", (int) ret,
		       buf);
		TEST_ASSERT(ret == 12, "test_parent_write_child_read",
			    "child should read parent message");
		TEST_ASSERT(bytes_equal(buf, "from-parent\n", 12),
			    "test_parent_write_child_read",
			    "child should receive parent data");
		ret = read(fds[0], buf, sizeof(buf));
		printf("[child parent->child] read EOF -> %d\n", (int) ret);
		TEST_ASSERT(
		    ret == 0, "test_parent_write_child_read",
		    "child should see EOF after parent closes write end");
		close(fds[0]);
		exit(0);
	}

	close(fds[0]);
	printf("[parent parent->child] close read fd -> %d\n", (int) ret);
	ret = write(fds[1], "from-parent\n", 12);
	printf("[parent parent->child] write -> %d\n", (int) ret);
	TEST_ASSERT(ret == 12, "test_parent_write_child_read",
		    "parent should write child message");
	close(fds[1]);
	printf("[parent parent->child] close write fd -> %d\n", (int) ret);
	int reaped = wait();
	printf("[parent parent->child] wait -> %d\n", reaped);
	TEST_ASSERT(reaped == pid, "test_parent_write_child_read",
		    "parent should reap reader child");
	TEST_PASS("test_parent_write_child_read");
}

static void test_child_write_parent_read(void)
{
	TEST_START("test_child_write_parent_read");
	int fds[2];
	char buf[32] = {0};
	long ret = pipe2(fds, 0);
	printf("fork child->parent pipe2 -> %d, read=%d write=%d\n", (int) ret,
	       fds[0], fds[1]);
	TEST_ASSERT(ret == 0, "test_child_write_parent_read",
		    "fork child->parent pipe2 should work");

	int pid = fork();
	if (pid < 0) {
		TEST_FAIL("test_child_write_parent_read");
	} else if (pid == 0) {
		close(fds[0]);
		printf("[child child->parent] close read fd -> %d\n",
		       (int) ret);
		ret = write(fds[1], "from-child\n", 11);
		printf("[child child->parent] write -> %d\n", (int) ret);
		TEST_ASSERT(ret == 11, "test_child_write_parent_read",
			    "child should write parent message");
		close(fds[1]);
		printf("[child child->parent] close write fd -> %d\n",
		       (int) ret);
		exit(0);
	}

	close(fds[1]);
	printf("[parent child->parent] close write fd -> %d\n", (int) ret);
	ret = read(fds[0], buf, sizeof(buf));
	printf("[parent child->parent] read -> %d, buf=%s\n", (int) ret, buf);
	TEST_ASSERT(ret == 11, "test_child_write_parent_read",
		    "parent should read child message");
	TEST_ASSERT(bytes_equal(buf, "from-child\n", 11),
		    "test_child_write_parent_read",
		    "parent should receive child data");
	ret = read(fds[0], buf, sizeof(buf));
	printf("[parent child->parent] read EOF -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "test_child_write_parent_read",
		    "parent should see EOF after child closes write end");
	close(fds[0]);
	printf("[parent child->parent] close read fd -> %d\n", (int) ret);
	int reaped = wait();
	printf("[parent child->parent] wait -> %d\n", reaped);
	TEST_ASSERT(reaped == pid, "test_child_write_parent_read",
		    "parent should reap writer child");
	TEST_PASS("test_child_write_parent_read");
}

static void test_blocking_read_wakeup(void)
{
	TEST_START("test_blocking_read_wakeup");
	int fds[2];
	char buf[16] = {0};
	long ret = pipe2(fds, 0);
	printf("blocking read pipe2 -> %d, read=%d write=%d\n", (int) ret,
	       fds[0], fds[1]);
	TEST_ASSERT(ret == 0, "test_blocking_read_wakeup",
		    "blocking read pipe2 should work");

	int pid = fork();
	if (pid < 0) {
		TEST_FAIL("test_blocking_read_wakeup");
	} else if (pid == 0) {
		close(fds[1]);
		printf("[child blocking read] close write fd -> %d\n",
		       (int) ret);
		ret = read(fds[0], buf, sizeof(buf));
		printf("[child blocking read] read -> %d, buf=%s\n", (int) ret,
		       buf);
		TEST_ASSERT(ret == 6, "test_blocking_read_wakeup",
			    "blocked reader should receive parent data");
		TEST_ASSERT(bytes_equal(buf, "wake!\n", 6),
			    "test_blocking_read_wakeup",
			    "blocked reader data should match");
		close(fds[0]);
		printf("[child blocking read] close read fd -> %d\n",
		       (int) ret);
		exit(0);
	}

	close(fds[0]);
	printf("[parent blocking read] close read fd -> %d\n", (int) ret);
	sched_yield();
	ret = write(fds[1], "wake!\n", 6);
	printf("[parent blocking read] write -> %d\n", (int) ret);
	TEST_ASSERT(ret == 6, "test_blocking_read_wakeup",
		    "parent wakes blocked reader");
	close(fds[1]);
	printf("[parent blocking read] close write fd -> %d\n", (int) ret);
	int reaped = wait();
	printf("[parent blocking read] wait -> %d\n", reaped);
	TEST_ASSERT(reaped == pid, "test_blocking_read_wakeup",
		    "parent reaps blocked reader");
	TEST_PASS("test_blocking_read_wakeup");
}

static void test_blocking_write_wakeup(void)
{
	TEST_START("test_blocking_write_wakeup");
	int fds[2];
	char fill[PIPE_BUF_SIZE];
	char out[PIPE_BUF_SIZE];
	long ret = pipe2(fds, 0);
	printf("blocking write pipe2 -> %d, read=%d write=%d\n", (int) ret,
	       fds[0], fds[1]);
	TEST_ASSERT(ret == 0, "test_blocking_write_wakeup",
		    "blocking write pipe2 should work");

	fill_pattern(fill, sizeof(fill));
	ret = write(fds[1], fill, sizeof(fill));
	printf("[parent blocking write] fill pipe -> %d\n", (int) ret);
	TEST_ASSERT(ret == PIPE_BUF_SIZE, "test_blocking_write_wakeup",
		    "pipe fill should succeed");

	int pid = fork();
	if (pid < 0) {
		TEST_FAIL("test_blocking_write_wakeup");
	} else if (pid == 0) {
		close(fds[1]);
		printf("[child blocking write] close write fd -> %d\n",
		       (int) ret);
		ret = read(fds[0], out, sizeof(out));
		printf("[child blocking write] drain read -> %d\n", (int) ret);
		TEST_ASSERT(ret == PIPE_BUF_SIZE, "test_blocking_write_wakeup",
			    "child should drain full pipe");
		TEST_ASSERT(pattern_matches(out, 0, PIPE_BUF_SIZE),
			    "test_blocking_write_wakeup",
			    "drained full pipe data should match");
		ret = read(fds[0], out, 1);
		printf("[child blocking write] final read -> %d, byte=%d\n",
		       (int) ret, out[0]);
		TEST_ASSERT(ret == 1 && out[0] == '!',
			    "test_blocking_write_wakeup",
			    "child should read writer data after wakeup");
		close(fds[0]);
		printf("[child blocking write] close read fd -> %d\n",
		       (int) ret);
		exit(0);
	}

	close(fds[0]);
	printf("[parent blocking write] close read fd -> %d\n", (int) ret);
	ret = write(fds[1], "!", 1);
	printf("[parent blocking write] write after full -> %d\n", (int) ret);
	TEST_ASSERT(ret == 1, "test_blocking_write_wakeup",
		    "full pipe writer should resume");
	close(fds[1]);
	printf("[parent blocking write] close write fd -> %d\n", (int) ret);
	int reaped = wait();
	printf("[parent blocking write] wait -> %d\n", reaped);
	TEST_ASSERT(reaped == pid, "test_blocking_write_wakeup",
		    "parent reaps blocking writer child");
	TEST_PASS("test_blocking_write_wakeup");
}

void _start()
{
	TEST_START("sys_pipe");
	test_pipe_creation_errors();
	test_pipe_basic_rw();
	test_pipe_bulk_pattern();
	test_pipe_close_semantics();
	test_pipe_user_buffer_faults();
	test_dup_lifetime();
	test_exit_closes_writer();
	test_exit_closes_reader();
	test_parent_write_child_read();
	test_child_write_parent_read();
	test_blocking_read_wakeup();
	test_blocking_write_wakeup();
	TEST_PASS("sys_pipe");
	shutdown();
}
