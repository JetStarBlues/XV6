// Format of an ELF executable file

#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian

// File header
struct elfhdr
{
	uint   magic;       // must equal ELF_MAGIC

	uchar  elf [ 12 ];  // 1 - 32bit, 2 - 64bit
	                    // 1 - little endian, 2 - big endian
	                    // ...
	                    // target OS e.g. System V, Linux
	                    // ...

	ushort type;        // object file type e.g. EXEC, DYN
	ushort machine;     // ISA e.g. x86, MIPS, ARM, RISC-V
	uint   version;     // ...

	uint   entry;       // entry point where process starts executing
	uint   phoff;       // points to start of program header table
	uint   shoff;       // points to start of section header table

	uint   flags;       // ...

	ushort ehsize;      // size of this elf header

	ushort phentsize;   // size of program header entry
	ushort phnum;       // number of entries in program header table

	ushort shentsize;   // size of section header entry
	ushort shnum;       // number of entries in section header table
	ushort shstrndx;    // index of the section	header table entry that contains the section names
};

// Program section header
struct proghdr
{
	uint type;    // type of segment e.g. LOAD, DYNAMIC
	uint off;     // offset of the segment in the file image
	uint vaddr;   // virtual address of the segment in memory
	uint paddr;   // physical address of the segment in memory 
	uint filesz;  // size in bytes of the segment in the file image
	uint memsz;   // size in bytes of the segment in memory
	uint flags;   // ...
	uint align;   // 0 or 1 no alignment
};

// Values for proghdr type
#define ELF_PROG_LOAD       1

// Flag bits for proghdr flags
#define ELF_PROG_FLAG_EXEC  1
#define ELF_PROG_FLAG_WRITE 2
#define ELF_PROG_FLAG_READ  4
