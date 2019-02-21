#include <err.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <bintail/bintail.hpp>

bintail::ElfExe::ElfExe(const char* infile)
{
    /* init libelf state */
    if (elf_version(EV_CURRENT) == EV_NONE)
        errx(1, "libelf init failed");
    if ((fd = open(infile, O_RDONLY)) == -1) 
        errx(1, "open %s failed. %s", infile, strerror(errno));
    if ((e = elf_begin(fd, ELF_C_READ, NULL)) == nullptr)
        errx(1, "elf_begin infile failed.");

    /* EHDR */
    gelf_getehdr(e, &ehdr);
}

bintail::ElfExe::~ElfExe()
{
    elf_end(e);
    close(fd);
}

bool
bintail::ElfExe::is_elf()
{
    return elf_kind(e) == ELF_K_ELF;
}

bool
bintail::ElfExe::is_pic()
{
    return ehdr.e_type == ET_DYN;
}

int
bintail::ElfExe::shnum()
{
    return ehdr.e_shnum;
}

int
bintail::ElfExe::shstrndx()
{
    return ehdr.e_shstrndx;
}
