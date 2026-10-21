# 用户态测试框架入口

`test.c` 是新测试框架的启动入口。目前执行一百万次 `getpid()`，
输出总耗时和每次调用的平均耗时，最后关闭本次运行。

在仓库根目录、具备交叉工具链与 QEMU 的环境中运行：

```bash
make qemu ARCH=riscv ROOTFS=easyfs FS_LIST="devtmpfs tmpfs" TEST=framework BUILD=release
```

`TEST=framework` 选择 `test/framework/test.c`；其他 `TEST=<name>` 仍选择
`test/standalone/test_<name>.c`。RISC-V 和 LoongArch 的构建规则均支持此入口，
实际可运行的用例取决于对应架构的内核功能。

后续用例可以先写成函数，由 `_start()` 依次调用。需要时再拆出注册、断言、
进程隔离和结果汇总模块。

当前计时使用现有 `clock_gettime(0, ...)`，底层时间被量化到微秒。
平均纳秒值由整批耗时换算得到，包含循环开销，不代表单次采样具有纳秒分辨率。
`PASS` 表示测量流程完成，不代表性能达到某个阈值。
