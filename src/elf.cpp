#include "elf.h"

namespace bintail {

ElfExe::ElfExe(const char* infile)
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

ElfExe::~ElfExe()
{
    elf_end(e);
    close(fd);
}

void
ElfExe::write(const char *outfile)
{
    auto outfd = open(outfile, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IXUSR);
    if (outfd  == -1) 
        errx(1, "open %s failed. %s", outfile, strerror(errno));

    auto e_out = elf_begin(outfd, ELF_C_WRITE, NULL);
    if (e_out  == nullptr)
        errx(1, "elf_begin outfile failed.");

     // Manual layout: Sections in segments have to be relocated manualy
    elf_flagelf(e_out, ELF_C_SET, ELF_F_LAYOUT);

    GElf_Ehdr ehdr_out;
    gelf_newehdr(e_out, ELFCLASS64);
    gelf_getehdr(e_out, &ehdr_out);
    ehdr_out = ehdr;
    gelf_update_ehdr(e_out, &ehdr_out);

    elf_fill(0xcccccccc); // asm(int 0x3) - fail fast on failure
    if (elf_update(e_out, ELF_C_WRITE) < 0) {
        std::cerr << elf_errmsg(elf_errno()) << "\n";
        errx(1, "elf_update(write) failed.");
    }
}

uint64_t
ElfExe::get_phdr_offset()
{
    return ehdr.e_phoff;
}

uint64_t
ElfExe::get_shdr_offset()
{
    return ehdr.e_shoff;
}

bool
ElfExe::is_elf() 
{
    return elf_kind(e) == ELF_K_ELF;
}

bool
ElfExe::is_pic()
{
    return ehdr.e_type == ET_DYN;
}

int
ElfExe::shnum()
{
    return ehdr.e_shnum;
}

int
ElfExe::shstrndx()
{
    return ehdr.e_shstrndx;
}

} // namespace bintail
