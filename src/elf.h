#ifndef BINTAIL_ELF_H_
#define BINTAIL_ELF_H_

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <algorithm>
#include <cassert>
#include <iostream>

#include <bintail/bintail.hpp>

namespace bintail {

class Section {
 public:
  /**
   * Load all data from the file on initialization
   * libelf stucts are not saved
   **/
  explicit Section(Elf_Scn *scn, Elf *elf, size_t shstrndx);
  ~Section();

  /**
   * Add a new section to the elf object and write the shdr and date into it.
   *
   * \return data size
   **/
  size_t write_new_scn(Elf *elf) const;

  uint64_t get_vaddr() const;
  uint64_t get_offset() const;

  /**
   * Data changes are complex, relocations have to be taken into account
   * and different section types have differing constraints.
   **/
  const std::vector<uint8_t> get_data() const;
  const std::string get_name() { return name_; }

 private:
  std::vector<uint8_t> buf_;
  GElf_Shdr shdr_;
  std::string name_;
};

class ElfExe {
public:
  explicit ElfExe(const char *infile);
  ~ElfExe();

  void write(const char *outfile);

  Section *get_section(const char *section_name);

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

  std::vector<std::unique_ptr<Section>> secs_;
};

} // namespace bintail
#endif // BINTAIL_ELF_H_
