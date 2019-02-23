#include <iostream>
#include <fstream>
#include <cstdio>
#include <catch2/catch.hpp>

#include <bintail/bintail.hpp>

const auto sample_simple = "./samples/simple";

TEST_CASE("Bintail can read and write an executable") {
    const auto outfile = "/tmp/bintail-test-rwsimple";
    remove(outfile);

    Bintail bintail{sample_simple};
    bintail.init_write(outfile, true);
    bintail.write();

    std::ifstream f;
    f.open(outfile);
    REQUIRE(f.good());
}
