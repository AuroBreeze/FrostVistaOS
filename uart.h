// uart.h
#ifndef __UART_H__

#define __UART_H__

#define UART_ADDR 0x10000000 // UART 在模拟器中的起始地址

#define UART_RX_ADR 0 // 接收消息时的偏移地址
#define UART_TX_ADR 0 // 发送消息写入的偏移地址

// UART 各个寄存器的偏移地址
#define UART_IER_ADR 1 // 中断使能寄存器
#define UART_IIR_ADR 2 // 中断识别寄存器
#define UART_FCR_ADR 2 // FIFO 控制寄存器
#define UART_LCR_ADR 3 // 线路控制寄存器
#define UART_MCR_ADR 4 // 调制解调器控制寄存器
#define UART_LSR_ADR 5 // 线路状态寄存器
#define UART_MSR_ADR 6 // 调制解调器状态寄存器

// 需要注意的是: UART的除数锁存器低位被写入时，内部就开始工作了，所以要先写高位再写低位
// 驱动手册中说的也很明确，开启访问除数锁存器后，会占用偏移为0和1的两个寄存器，所以在设置完除数锁存器的时候要及时关闭
#define UART_DLL_ADR 0 // 除数锁存器低位
#define UART_DLH_ADR 1 // 除数锁存器高位

// IER 寄存器的位定义
#define UART_IER_RX_ENABLE (1 << 0)
#define UART_IER_TX_ENABLE (1 << 1)
#define UART_IER_LINE_STATUS_ENABLE (1 << 2)

// FIFO 寄存器的位定义
// 置1则启用FIFO
#define UART_FCR_FIFO_ENABLE (1<<0)
#define UART_FCR_CLEAR_RX (1 << 1)
#define UART_FCR_CLEAR_TX (1 << 2)
// FIFO 中断触发字节，通过设置第6位和第7位来设置多少字节可以触发
// 在这里设置为 11，即14字节触发中断
#define UART_FCR_FIFO_LENGTH_ENABLE (3UL << 6)

// LCR 寄存器的位定义
// 设置前两位为11，即8位一个字符
#define UART_LCR_WORD_LENGTH (3 << 0)
// 设置允许访问和设置除数锁存器，即设置波特率
#define UART_LCR_DIVISOR_LATCH_ENABLE (1UL << 7)

// LSR 寄存器的位定义
// 1则表示接收数据准备好了
#define UART_LSR_DATA_READY (1 << 0)
// 1则表示发送数据已经空闲了
#define UART_LSR_TX_EMPTY (1 << 5)

#define Reg(reg) ((volatile char *)(UART_ADDR + reg))
#define ReadReg(reg) (*Reg(reg))
#define WriteReg(reg, value) ((*Reg(reg)) = value)

#endif