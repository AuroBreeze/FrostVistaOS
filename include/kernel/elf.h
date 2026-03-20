#ifndef __KERNEL_ELF_H__
#define __KERNEL_ELF_H__

#include "kernel/types.h"
// elf.h
#define ELF_MAGIC 0x464C457F // "\x7FELF" lower case
#define ELF_PROG_LOAD 1

#define ELF_PROG_FLAG_EXEC 1
#define ELF_PROG_FLAG_WRITE 2
#define ELF_PROG_FLAG_READ 4

// ELF Header
struct elfhdr {
  uint32 magic; // must be ELF_MAGIC
  uint8 elf[12];
  uint16 type;    // file type
  uint16 machine; // 0xF3 for RISC-V
  uint32 version;
  uint64 entry; // gives the virtual address to which the system first transfers
                // control, thus starting the process
  uint64 phoff; // holds the program header table's file offset in bytes
  uint64 shoff; // Offset from the beginning of the file to the section header
  uint32 flags; // holds processor-specific flags associated with the file
  uint16 ehsize;    // holds the ELF header's size in bytes.
  uint16 phentsize; // holds the size in bytes of one entry in the file's
                    // program header table
  uint16 phnum;     // holds the number of entries in the program header table
  uint16 shentsize; // The byte size of each entry
  uint16 shnum;     // Display the number of entries in the section table
  uint16 shstrndx;
};

// Program Header
struct proghdr {
  uint32 type;
  uint32 flags;
  uint64 off;    // gives the offset from the beginning of the file at which the
                 // first byte of the segment resides
  uint64 vaddr;  // gives the virtual address at which the first byte of the
                 // segment resides in memory
  uint64 paddr;  // On systems for which physical addressing is relevant, this
                 // member is reserved for the segment's physical address
  uint64 filesz; // gives the number of bytes in the file image of the segment
  uint64 memsz;  // gives the number of bytes in the memory image of the segment
  uint64 align;
};

#endif
