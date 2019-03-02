#include "elf.h"

namespace bintail {

Section::Section(Elf_Scn *scn, Elf *elf, size_t shstrndx) {
  assert(scn != nullptr);

  /* Load section header and name */
  gelf_getshdr(scn, &shdr_);
  name_ = std::string(elf_strptr(elf, shstrndx, shdr_.sh_name));

  /* Load Data */
  auto scn_data = elf_getdata(scn, nullptr);
  if (scn_data != nullptr && scn_data->d_buf != nullptr) {
    auto data_size = scn_data->d_size;
    assert(scn_data->d_size == shdr_.sh_size);
    assert(scn_data->d_buf != nullptr);
    assert(scn_data->d_align == shdr_.sh_addralign);

    auto buf = static_cast<uint8_t *>(scn_data->d_buf);
    buf_.assign(buf, buf + data_size);
  }
}

Section::~Section() {}

/**
 * Creates a single data object per scn, the buf of the Section object is used.
 * The elf file object becomes invalid if this Section is destructed.
 */
size_t Section::write_new_scn(Elf *elf) const {
  assert(elf != nullptr);
  GElf_Shdr tmp_shdr;
  Elf_Data *data_in = nullptr, *data_out = nullptr;

  auto scn_out = elf_newscn(elf);
  if (scn_out == nullptr) errx(1, "elf_newscn failed.");

  /* SHDR */
  gelf_getshdr(scn_out, &tmp_shdr);
  tmp_shdr = shdr_;
  assert(shdr_.sh_size == buf_.size() || shdr_.sh_type == SHT_NOBITS);
  tmp_shdr.sh_size = buf_.size();
  gelf_update_shdr(scn_out, &tmp_shdr);

  /* Data */
  if ((data_out = elf_newdata(scn_out)) == nullptr)
    errx(1, "elf_newdata failed.");
  data_out->d_align = shdr_.sh_addralign;
  data_out->d_off = 0;
  data_out->d_size = buf_.size();
  data_out->d_type =
      ELF_T_BYTE;  // ToDo(felix): use type info on read and write

  /* ToDo(felix): This is disgusting and should be revisited */
  data_out->d_buf =
      const_cast<void *>(reinterpret_cast<const void *>(buf_.data()));

  return buf_.size();
}

uint64_t Section::get_vaddr() const { return shdr_.sh_addr; }
uint64_t Section::get_offset() const { return shdr_.sh_offset; }

const std::vector<uint8_t> Section::get_data() const { return buf_; }

ElfExe::ElfExe(const char *infile) {
  /* init libelf state */
  if (elf_version(EV_CURRENT) == EV_NONE)
    errx(1, "libelf init failed");
  if ((fd_ = open(infile, O_RDONLY)) == -1)
    errx(1, "open %s failed. %s", infile, strerror(errno));
  if ((e_ = elf_begin(fd_, ELF_C_READ, NULL)) == nullptr)
    errx(1, "elf_begin infile failed.");

  /* EHDR */
  gelf_getehdr(e_, &ehdr_);

  /* Read sections */
  Elf_Scn *scn = nullptr;
  size_t shstrndx;
  elf_getshdrstrndx(e_, &shstrndx);
  while ((scn = elf_nextscn(e_, scn)) != nullptr) {
    auto sec = std::make_unique<Section>(scn, e_, shstrndx);
    secs_.push_back(std::move(sec));
  }
}

ElfExe::~ElfExe() {
  elf_end(e_);
  close(fd_);
}

void ElfExe::write(const char *outfile) {
  auto outfd = open(outfile, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IXUSR);
  if (outfd == -1)
    errx(1, "open %s failed. %s", outfile, strerror(errno));

  auto e_out = elf_begin(outfd, ELF_C_WRITE, NULL);
  if (e_out == nullptr)
    errx(1, "elf_begin outfile failed.");

  // Manual layout: Sections in segments have to be relocated manualy
  elf_flagelf(e_out, ELF_C_SET, ELF_F_LAYOUT);

  GElf_Ehdr ehdr_out;
  gelf_newehdr(e_out, ELFCLASS64);
  gelf_getehdr(e_out, &ehdr_out);
  ehdr_out = ehdr_;
  gelf_update_ehdr(e_out, &ehdr_out);

  /* Write Sections */
  for (const auto &s : secs_) {
    s->write_new_scn(e_out);
  }

  /* Finish elf */
  elf_fill(0xcccccccc); // asm(int 0x3) - fail fast on failure
  if (elf_update(e_out, ELF_C_WRITE) < 0) {
    std::cerr << elf_errmsg(elf_errno()) << "\n";
    errx(1, "elf_update(write) failed.");
  }
}

Section *ElfExe::get_section(const char *section_name) {
  auto it = std::find_if(secs_.cbegin(), secs_.cend(), [&](auto &s) {
    return s->get_name() == section_name;
  });
  return it != secs_.cend() ? it->get() : nullptr;
}

uint64_t ElfExe::get_phdr_offset() { return ehdr_.e_phoff; }

uint64_t ElfExe::get_shdr_offset() { return ehdr_.e_shoff; }

bool ElfExe::is_elf() { return elf_kind(e_) == ELF_K_ELF; }

bool ElfExe::is_pic() { return ehdr_.e_type == ET_DYN; }

int ElfExe::shnum() { return ehdr_.e_shnum; }

int ElfExe::shstrndx() { return ehdr_.e_shstrndx; }

} // namespace bintail
