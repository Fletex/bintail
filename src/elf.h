#ifndef BINTAIL_ELF_H_
#define BINTAIL_ELF_H_

#include <err.h>
#include <fcntl.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <bintail/bintail.hpp>

namespace bintail {

class ElfExe {
public:
  explicit ElfExe(const char *infile);
  ~ElfExe();

  void write(const char *outfile);

  bool is_elf();
  bool is_pic();

  int shnum();
  int shstrndx();

  uint64_t get_phdr_offset();
  uint64_t get_shdr_offset();

private:
  int fd_;
  Elf *e_;
  GElf_Ehdr ehdr_;
};

} // namespace bintail
#endif // BINTAIL_ELF_H_
