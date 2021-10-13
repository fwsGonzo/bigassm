#include "elf128.h"

using Elf_Ehdr = Elf64_Ehdr;
using Elf_Phdr = Elf64_Phdr;
using Elf_Shdr = Elf64_Shdr;
using Elf_Addr = Elf64_Addr;
using Elf_Sym = Elf64_Sym;
#define ELFCLASS_XX  ELFCLASS64

#define Elf_Writer ELFwriter64

#include "elfwriter.cpp"
