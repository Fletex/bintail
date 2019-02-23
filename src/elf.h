#ifndef BINTAIL_ELF_H_
#define BINTAIL_ELF_H_

#include <err.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <iostream>

#include <bintail/bintail.hpp>

namespace bintail {

class ElfExe {
    int fd;
    Elf *e;
    GElf_Ehdr ehdr;

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
};

} // namespace bintail
#endif // BINTAIL_ELF_H_
