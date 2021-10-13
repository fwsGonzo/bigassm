#include "elf128.h"

using Elf_Ehdr = Elf128_Ehdr;
using Elf_Phdr = Elf128_Phdr;
using Elf_Shdr = Elf128_Shdr;
using Elf_Addr = Elf128_Addr;
using Elf_Sym = Elf128_Sym;
#define ELFCLASS_XX  ELFCLASS128

#define Elf_Writer ELFwriter128

#include "elfwriter.cpp"
