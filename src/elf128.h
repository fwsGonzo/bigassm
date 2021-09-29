#ifndef ELF128_H
#define ELF128_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "elf.h"

typedef uint16_t    Elf128_Half;
typedef uint32_t    Elf128_Word;
typedef uint64_t    Elf128_Xword;
typedef uint64_t    Elf128_Off;

#ifdef OUTPUT_64BIT_ELF
typedef uint64_t    Elf128_Addr;
#define ELFCLASS128  2
#else
typedef __uint128_t Elf128_Addr;
#define ELFCLASS128  3
#endif

typedef struct {
  unsigned char	e_ident[EI_NIDENT];
  Elf128_Half	e_type;
  Elf128_Half	e_machine;
  Elf128_Word	e_version;
  Elf128_Addr	e_entry;
  Elf128_Off	e_phoff;
  Elf128_Off	e_shoff;
  Elf128_Word	e_flags;
  Elf128_Half	e_ehsize;
  Elf128_Half	e_phentsize;
  Elf128_Half	e_phnum;
  Elf128_Half	e_shentsize;
  Elf128_Half	e_shnum;
  Elf128_Half	e_shstrndx;
} Elf128_Ehdr;

typedef struct {
  Elf128_Word	sh_name;
  Elf128_Word	sh_type;
  Elf128_Xword	sh_flags;
  Elf128_Addr	sh_addr;
  Elf128_Off	sh_offset;
  Elf128_Xword	sh_size;
  Elf128_Word	sh_link;
  Elf128_Word	sh_info;
  Elf128_Xword	sh_addralign;
  Elf128_Xword	sh_entsize;
} Elf128_Shdr;

typedef struct {
  Elf128_Word	p_type;
  Elf128_Word	p_flags;
  Elf128_Off	p_offset;
  Elf128_Addr	p_vaddr;
  Elf128_Addr	p_paddr;
  Elf128_Xword	p_filesz;
  Elf128_Xword	p_memsz;
  Elf128_Xword	p_align;
} Elf128_Phdr;

typedef struct {
  Elf128_Word	st_name;
  unsigned char	st_info;
  unsigned char st_other;
  Elf128_Half	st_shndx;
  Elf128_Addr	st_value;
  Elf128_Xword	st_size;
} Elf128_Sym;

#ifdef __cplusplus
#include <vector>
#include <map>

struct ElfStringSection {
	Elf128_Shdr& shdr;
	std::vector<uint8_t>& bin;
	std::map<std::string, size_t> namelist;
	const int shindex;
	size_t offset = 0;

	ElfStringSection(Elf128_Shdr& sh, int idx, std::vector<uint8_t>& b)
		: shdr{sh}, bin{b}, shindex{idx}
	{
		shdr.sh_name = 0;
		shdr.sh_type = SHT_STRTAB;
		shdr.sh_flags = 0;
		shdr.sh_entsize = 1;
		shdr.sh_offset = bin.size();
		shdr.sh_size = 0;
		shdr.sh_addralign = 1;
		shdr.sh_info = 0;
		shdr.sh_link = 0;
		this->append_null();
	}

	size_t add(const std::string& str) {
		bin.insert(bin.end(), str.begin(), str.end()+1);
		shdr.sh_size += str.size()+1;
		const size_t offset = this->offset;
		namelist[str] = offset;
		this->offset += str.size()+1;
		return offset;
	}
	size_t lookup(const std::string& str) const {
		return namelist.at(str);
	}
	void append_null() {
		bin.push_back(0);
		shdr.sh_size += 1;
		this->offset += 1;
	}
};

struct ElfSymSection {
	Elf128_Shdr& shdr;
	std::vector<uint8_t>& bin;

	ElfSymSection(Elf128_Shdr& sh, int strindex, std::vector<uint8_t>& b)
		: shdr{sh}, bin{b}
	{
		shdr.sh_type = SHT_SYMTAB;
		shdr.sh_flags = 0;
		shdr.sh_entsize = sizeof(Elf128_Sym);
		shdr.sh_offset = bin.size();
		shdr.sh_size = 0;
		shdr.sh_info = 0;
		shdr.sh_addralign = alignof(Elf128_Addr);
		shdr.sh_link = strindex;
		Elf128_Sym nosym {};
		this->add_sym(nosym);
	}

	void add(const std::string& name, Elf128_Addr addr, int info, size_t size, ElfStringSection& str) {
		Elf128_Sym sym {};
		sym.st_name = str.lookup(name);
		sym.st_shndx = SHN_UNDEF;
		sym.st_info  = info;
		sym.st_other = STV_DEFAULT;
		sym.st_value = addr;
		sym.st_size  = size;
		this->add_sym(sym);
	}
	void add_sym(const Elf128_Sym& sym) {
		const uint8_t* s = (const uint8_t *)&sym;
		bin.insert(bin.end(), s, s + sizeof(sym));
		shdr.sh_size += sizeof(sym);
		shdr.sh_info ++;
	}

	size_t count() const noexcept {
		return shdr.sh_info;
	}
};

}
#endif

#endif /* ELF128_H */
