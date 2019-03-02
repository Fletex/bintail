#include "elf.h"

#include <catch2/catch.hpp>

const auto sample_simple = "./samples/simple";

TEST_CASE("Executables can be parsed and changed") {
  bintail::ElfExe exe{sample_simple};

  const auto outfile = "/tmp/bintail-test-rwsimple";
  remove(outfile);
  exe.write(outfile);

  SECTION("EHDR is the same in and out") {
    bintail::ElfExe exe2{outfile};

    REQUIRE(exe2.is_elf() == exe.is_elf());
    REQUIRE(exe2.is_pic() == exe.is_pic());
    REQUIRE(exe2.get_phdr_offset() == exe.get_phdr_offset());
    REQUIRE(exe2.get_shdr_offset() == exe.get_shdr_offset());
    // REQUIRE( exe2.shnum() == exe.shnum() );
    REQUIRE(exe2.shstrndx() == exe.shstrndx());
  }

  SECTION("text section is the same in and out") {
    bintail::ElfExe exe2{outfile};

    auto in_scn = exe.get_section(".text");
    auto out_scn = exe2.get_section(".text");

    REQUIRE(in_scn != nullptr);
    REQUIRE(out_scn != nullptr);
    REQUIRE(out_scn->get_vaddr() == in_scn->get_vaddr());
    REQUIRE(out_scn->get_offset() == in_scn->get_offset());
    REQUIRE(out_scn->get_data()[10] == in_scn->get_data()[10]);
  }
}
