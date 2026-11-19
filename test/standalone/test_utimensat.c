#include "user/user.h"
#include "asm/user.h"
#include "user/libtest.h"

#define UTIME_NOW   1073741823L
#define UTIME_OMIT  1073741822L

struct linux_stat_user {
    uint64 st_dev;
    uint64 st_ino;
    uint32 st_mode;
    uint32 st_nlink;
    uint32 st_uid;
    uint32 st_gid;
    uint64 st_rdev;
    uint64 __pad1;
    int64  st_size;
    int    st_blksize;
    int    __pad2;
    int64  st_blocks;
    int64  st_atime_sec;
    int64  st_atime_nsec;
    int64  st_mtime_sec;
    int64  st_mtime_nsec;
    int64  st_ctime_sec;
    int64  st_ctime_nsec;
    uint32 __unused[2];
};

static long fstat_syscall(int fd, struct linux_stat_user *st)
{
    return arch_user_syscall3(SYS_fstat, fd, (long) st, 0);
}

int main(int argc, char **argv)
{
    TEST_START("utimensat");

    int fd = open("/ut_a", O_WRONLY | O_CREAT | O_TRUNC);
    TEST_ASSERT(fd >= 0, "utimensat", "should create test file");
    close(fd);

    /* ---------------- T1: 显式时间 ---------------- */
    struct linux_timespec t[2];
    t[0].tv_sec = 1111;  t[0].tv_nsec = 0;
    t[1].tv_sec = 2222;  t[1].tv_nsec = 0;

    int r = utimensat(AT_FDCWD, "/ut_a", t, 0);
    TEST_ASSERT(r == 0, "utimensat", "T1 explicit times should succeed");

    fd = open("/ut_a", O_RDONLY);
    TEST_ASSERT(fd >= 0, "utimensat", "T1 should reopen test file");

    struct linux_stat_user st = {0};
    TEST_ASSERT(fstat_syscall(fd, &st) == 0, "utimensat", "T1 fstat should succeed");
    printf("T1 read back: atime=%d mtime=%d\n",
           (int) st.st_atime_sec, (int) st.st_mtime_sec);
    TEST_ASSERT(st.st_atime_sec == 1111, "utimensat", "T1 atime should be 1111");
    TEST_ASSERT(st.st_mtime_sec == 2222, "utimensat", "T1 mtime should be 2222");

    /* ---------------- T2: times == NULL ---------------- */
    struct linux_timespec nowts;
    clock_gettime(0, &nowts);
    int64 now_sec = (int64) nowts.tv_sec;

    r = utimensat(AT_FDCWD, "/ut_a", 0, 0);
    TEST_ASSERT(r == 0, "utimensat", "T2 NULL times should succeed");

    struct linux_stat_user st2 = {0};
    TEST_ASSERT(fstat_syscall(fd, &st2) == 0, "utimensat", "T2 fstat should succeed");
    printf("T2 NULL: atime=%d mtime=%d now=%d\n",
           (int) st2.st_atime_sec, (int) st2.st_mtime_sec, (int) now_sec);

    TEST_ASSERT(st2.st_atime_sec >= now_sec - 1 &&
                st2.st_atime_sec <= now_sec + 1,
                "utimensat", "T2 NULL should set atime to now");
    TEST_ASSERT(st2.st_mtime_sec >= now_sec - 1 &&
                st2.st_mtime_sec <= now_sec + 1,
                "utimensat", "T2 NULL should set mtime to now");

    /* ---------------- T3: UTIME_OMIT ---------------- */
    t[0].tv_sec = 3333;  t[0].tv_nsec = 0;
    t[1].tv_sec = 4444;  t[1].tv_nsec = 0;
    TEST_ASSERT(utimensat(AT_FDCWD, "/ut_a", t, 0) == 0,
                "utimensat", "T3 seed should succeed");

    t[0].tv_sec = 0;     t[0].tv_nsec = UTIME_OMIT;
    t[1].tv_sec = 5555;  t[1].tv_nsec = 0;
    TEST_ASSERT(utimensat(AT_FDCWD, "/ut_a", t, 0) == 0,
                "utimensat", "T3 OMIT call should succeed");

    struct linux_stat_user st3 = {0};
    TEST_ASSERT(fstat_syscall(fd, &st3) == 0, "utimensat", "T3 fstat should succeed");
    printf("T3 OMIT: atime=%d mtime=%d\n",
           (int) st3.st_atime_sec, (int) st3.st_mtime_sec);
    TEST_ASSERT(st3.st_atime_sec == 3333, "utimensat", "T3 UTIME_OMIT must keep atime");
    TEST_ASSERT(st3.st_mtime_sec == 5555, "utimensat", "T3 mtime should be 5555");

    /* ---------------- T4: dirfd 相对路径 ---------------- */
    mkdir("/ut_dir", 0);                    /* 已存在则忽略 */

    int fdx = open("/ut_x", O_WRONLY | O_CREAT | O_TRUNC);       /* cwd 侧的同名文件 */
    TEST_ASSERT(fdx >= 0, "utimensat", "T4 should create /ut_x");
    close(fdx);

    fdx = open("/ut_dir/ut_x", O_WRONLY | O_CREAT | O_TRUNC);    /* dirfd 侧的同名文件 */
    TEST_ASSERT(fdx >= 0, "utimensat", "T4 should create /ut_dir/ut_x");
    close(fdx);

    int dirfd = open("/ut_dir", O_RDONLY);
    TEST_ASSERT(dirfd >= 0, "utimensat", "T4 should open /ut_dir as dirfd");

    t[0].tv_sec = 6666;  t[0].tv_nsec = 0;
    t[1].tv_sec = 7777;  t[1].tv_nsec = 0;
    r = utimensat(dirfd, "ut_x", t, 0);                          /* 相对 dirfd！ */
    TEST_ASSERT(r == 0, "utimensat", "T4 utimensat with dirfd should succeed");

    fdx = open("/ut_dir/ut_x", O_RDONLY);
    TEST_ASSERT(fdx >= 0, "utimensat", "T4 should reopen /ut_dir/ut_x");
    struct linux_stat_user sta = {0};
    TEST_ASSERT(fstat_syscall(fdx, &sta) == 0, "utimensat", "T4 fstat /ut_dir/ut_x");
    printf("T4 dir entry: atime=%d mtime=%d\n", (int) sta.st_atime_sec, (int) sta.st_mtime_sec);
    TEST_ASSERT(sta.st_atime_sec == 6666, "utimensat", "T4 /ut_dir/ut_x atime should be 6666");
    TEST_ASSERT(sta.st_mtime_sec == 7777, "utimensat", "T4 /ut_dir/ut_x mtime should be 7777");
    close(fdx);

    fdx = open("/ut_x", O_RDONLY);
    TEST_ASSERT(fdx >= 0, "utimensat", "T4 should reopen /ut_x");
    struct linux_stat_user stb = {0};
    TEST_ASSERT(fstat_syscall(fdx, &stb) == 0, "utimensat", "T4 fstat /ut_x");
    printf("T4 cwd entry: atime=%d mtime=%d\n", (int) stb.st_atime_sec, (int) stb.st_mtime_sec);
    TEST_ASSERT(stb.st_atime_sec != 6666, "utimensat", "T4 /ut_x must NOT be touched");
    TEST_ASSERT(stb.st_mtime_sec != 7777, "utimensat", "T4 /ut_x mtime must NOT be touched");
    close(fdx);
    close(dirfd);

    close(fd);
    TEST_PASS("utimensat");
    shutdown();
    return 0;
}
