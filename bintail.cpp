#include <iostream>
#include <string>
#include <set>
#include <iomanip>
#include <memory>
#include <err.h>
#include <stdio.h>
#include <cstdlib>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <gelf.h>
#include <regex>

#include "bintail.h"
#include "mvelem.h"

using namespace std;

static uint64_t sym_value(vector<struct symbol> &syms, const char* name) {
    return find_if(syms.cbegin(), syms.cend(), [name](auto& s) {
            return s.name == name;
            }).base()->sym.st_value;
}

static Elf_Scn * get_scn(vector<struct sec> &secs, const char* name) {
    return find_if(secs.cbegin(), secs.cend(), [name](auto& s) {
            return s.name == name;
            }).base()->scn;
}

Bintail::~Bintail() {
    elf_end(e);
    close(fd);
}

Bintail::Bintail(string filename) {
    /* init libelf state */ 
    if (elf_version(EV_CURRENT) == EV_NONE)
        errx(1, "libelf init failed");
    if ((fd = open(filename.c_str(), O_RDWR)) == -1) 
        errx(1, "open %s failed. %s", filename.c_str(), strerror(errno));
    if ((e = elf_begin(fd, ELF_C_RDWR, NULL)) == nullptr)
        errx(1, "elf_begin RDWR failed.");
    gelf_getehdr(e, &ehdr);
    elf_getshdrstrndx(e, &shstrndx);

    /*-------------------------------------------------------------------------
     * Manual ELF file layout, remove for smaller file.
     * ToDo(felix): Adjust phdr section after auto re-layout
     * see man pages: manual -> set offsets manualy (ehdr,shdr,phdr)
     *-----------------------------------------------------------------------*/
    elf_flagelf(e, ELF_C_SET, ELF_F_LAYOUT);

    Elf_Scn *scn = nullptr;
    GElf_Shdr shdr;
    while((scn = elf_nextscn(e, scn)) != nullptr) {
        struct sec s;
        gelf_getshdr(scn, &shdr);
        s.scn = scn;
        s.shdr = shdr;
        s.name = elf_strptr(e, shstrndx, shdr.sh_name);
        secs.push_back(s);
    }

    for (const auto& sec : secs)
        if (sec.shdr.sh_type == SHT_RELA && sec.shdr.sh_info == 0)
            reloc_scn = sec.scn;

    symtab_scn = get_scn(secs, ".symtab");

    mvvar.load  (e, get_scn(secs, "__multiverse_var_"));
    mvfn.load   (e, get_scn(secs, "__multiverse_fn_"));
    mvcs.load   (e, get_scn(secs, "__multiverse_callsite_"));
    mvdata.load (e, get_scn(secs, "__multiverse_data_"));
    mvtext.load (e, get_scn(secs, "__multiverse_text_"));
    rodata.load (e, get_scn(secs, ".rodata"));
    data.load   (e, get_scn(secs, ".data"));
    text.load   (e, get_scn(secs, ".text"));
    dynamic.load(e, get_scn(secs, ".dynamic"));
    bss.load    (e, get_scn(secs, ".bss"));

    /* Get LOAD Section ending in bss */ 
    size_t phdr_num;
    elf_getphdrnum(e, &phdr_num);
    GElf_Phdr phdr;
    for (auto i=0u; i<phdr_num; i++) {
        gelf_getphdr(e, i, &phdr);
        if (phdr.p_type != PT_LOAD &&
            bss.get_offset() != phdr.p_offset + phdr.p_filesz)
            continue;
        if ( mvvar.in_segment(phdr) && mvdata.in_segment(phdr) && 
             mvfn.in_segment(phdr) && mvcs.in_segment(phdr))
            break;
    }
    /* Get start addr */
    auto area_end = phdr.p_offset + phdr.p_filesz;
    auto area_start = area_end;
    area_start = min(area_start, mvvar.get_offset());
    area_start = min(area_start, mvdata.get_offset());
    area_start = min(area_start, mvfn.get_offset());
    area_start = min(area_start, mvcs.get_offset());
    assert(area_start + mvvar.max_sz() + mvdata.max_sz() +
           mvcs.max_sz() + mvfn.max_sz() == area_end);

    /* read info sections */
    auto mvvar_infos = mvvar.read();
    auto mvcs_infos = mvcs.read();
    auto mvfn_infos = mvfn.read();
    for (auto e : *mvvar_infos)
        vars.push_back(make_unique<MVVar>(e, &rodata, &data));
    for (auto e : *mvcs_infos)
        pps.push_back(make_unique<MVPP>(e, &text, &mvtext));
    for (auto e : *mvfn_infos) {
        auto f = make_unique<MVFn>(e, &mvdata, &mvtext, &rodata);
        auto pp = make_unique<MVPP>(f.get());
        f->add_pp(pp.get());
        fns.push_back(move(f));
        pps.push_back(move(pp));
    }

    /* multiverse_init equivalent */
    // find var & save ptr to it
    //    add fn to var.functions_head
    for (auto& fn : fns)
        for (auto& var: vars)
            fn->probe_var(var.get());

    // 1. Find function
    // 2. Create patchpoint
    // 3. Append pp to fn ll
    for (auto& pp : pps)
        for (auto& fn : fns) {
            if (fn->location() != pp->function_body )
                continue;
            fn->add_pp(pp.get());
            pp->set_fn(fn.get());
        }

    /* Keep symbols the same (refs to index) */
    GElf_Sym sym;
    Elf_Data * d2 = elf_getdata(symtab_scn, nullptr);
    gelf_getshdr(symtab_scn, &shdr);
    for (size_t i=0; i < d2->d_size / shdr.sh_entsize; i++) {
        gelf_getsym(d2, i, &sym);
        struct symbol s;
        s.sym = sym;
        s.name = elf_strptr(e, shdr.sh_link, sym.st_name);
        syms.push_back(s);
    }
    mvvar.start_ptr = sym_value(syms, "__start___multiverse_var_ptr");
    mvvar.stop_ptr  = sym_value(syms, "__stop___multiverse_var_ptr");
    mvfn.start_ptr  = sym_value(syms, "__start___multiverse_fn_ptr");
    mvfn.stop_ptr   = sym_value(syms, "__stop___multiverse_fn_ptr");
    mvcs.start_ptr  = sym_value(syms, "__start___multiverse_callsite_ptr");
    mvcs.stop_ptr   = sym_value(syms, "__stop___multiverse_callsite_ptr");

    for (auto& sym : syms)
        for (auto& fn : fns)
            fn->probe_sym(sym);

    // Remove claimed relocs (Regenerate on trim)
    GElf_Rela rela;
    gelf_getshdr(reloc_scn, &shdr);
    auto d = elf_getdata(reloc_scn, nullptr);
    for (size_t i=0; i < d->d_size / shdr.sh_entsize; i++) {
        gelf_getrela(d, i, &rela);
        auto claims = 0u;
        claims += mvvar.probe_rela(&rela);
        claims += mvfn.probe_rela(&rela);
        claims += mvcs.probe_rela(&rela);
        claims += mvdata.probe_rela(&rela);
        if (claims == 0)
            rela_other.push_back(rela);
    }
}

/**
 * Regenerate rela & sym table & update .dynamic info
 */
void Bintail::update_relocs_sym() {
    vector<GElf_Rela>* rvv[] = { 
        &data.relocs,
        &mvvar.relocs,
        &mvdata.relocs,
        &mvfn.relocs,
        &mvcs.relocs, 
        &mvtext.relocs,
        &rela_other
    };

    GElf_Shdr shdr, sym_shdr;
    gelf_getshdr(reloc_scn, &shdr);
    gelf_getshdr(symtab_scn, &sym_shdr);
    auto d = elf_getdata(reloc_scn, nullptr);
    auto d2 = elf_getdata(symtab_scn, nullptr);

    // RELOCS
    int i = 0;
    int cnt = 0;
    for (auto v : rvv) 
        for (auto r : *v) {
            if (r.r_info == R_X86_64_RELATIVE)
                cnt++;
            if (!gelf_update_rela (d, i++, &r))
                cout << "Error: gelf_update_rela() "
                    << elf_errmsg(elf_errno()) << endl;
        }

    assert(sizeof(GElf_Rela) == shdr.sh_entsize);
    shdr.sh_size = i * sizeof(GElf_Rela);
    d->d_size = shdr.sh_size;

    auto dyn_relacount = dynamic.get_dyn(DT_RELACOUNT);
    auto dyn_relasz = dynamic.get_dyn(DT_RELASZ);
    dyn_relacount->d_un.d_val = cnt;
    dyn_relasz->d_un.d_val = shdr.sh_size;

    // SYMS
    i = 0;
    for (auto s : syms) {
        if (!gelf_update_sym(d2, i++, &s.sym))
            cout << "Error: gelf_update_sym() "
                << elf_errmsg(elf_errno()) << endl;
    }

    assert(sizeof(GElf_Sym) == sym_shdr.sh_entsize);
    sym_shdr.sh_size = i * sizeof(GElf_Sym);
    d2->d_size = sym_shdr.sh_size;

    elf_flagdata(d, ELF_C_SET, ELF_F_DIRTY);
    elf_flagdata(d2, ELF_C_SET, ELF_F_DIRTY);
    gelf_update_shdr(reloc_scn, &shdr);
    gelf_update_shdr(symtab_scn, &sym_shdr);
    elf_flagshdr(reloc_scn, ELF_C_SET, ELF_F_DIRTY);
    elf_flagshdr(symtab_scn, ELF_C_SET, ELF_F_DIRTY);
}

void Bintail::change(string change_str) {
    smatch m;
    regex_search(change_str, m, regex(R"((\w+)=(\d+))"));
    auto var_name = m.str(1);
    auto value = stoi(m.str(2));
    for (auto& e : vars)
        if (var_name == e->name())
            e->set_value(value, &data);
}

/**
 * Remove variance
 *  guard - replace mvtext entry of unused with 0xc3
 */
void Bintail::apply(string change_str, bool guard) {
    smatch m;
    regex_search(change_str, m, regex(R"((\w+))"));
    auto var_name = m.str(1);
    for (auto& e : vars)
        if (var_name == e->name())
            e->apply(&text, &mvtext, guard);
}

void Bintail::trim() {
    // Remove relocs from multiverse sections
    mvvar.relocs.clear();
    mvfn.relocs.clear();
    mvcs.relocs.clear();
    mvdata.relocs.clear();

    size_t mvvar_sz = 0;
    size_t mvfn_sz = 0;
    size_t mvdata_sz = 0;
    size_t mvcs_sz = 0;

    GElf_Shdr shdr;
    byte* buf;
    byte* dbuf;
    uint64_t vaddr;
    uint64_t dvaddr;

    /* multiverse vars */
    buf = mvvar.dirty_buf();
    gelf_getshdr(mvvar.scn, &shdr);
    vaddr = shdr.sh_addr;
    for (auto& e:vars) {
        if (e->frozen)
            continue;
        mvvar_sz += e->make_info(buf+mvvar_sz, &mvvar, vaddr+mvvar_sz);
    }

    /* multiverse fns & mvdata */
    buf = mvfn.dirty_buf();
    dbuf = mvdata.dirty_buf();
    gelf_getshdr(mvfn.scn, &shdr);
    vaddr = shdr.sh_addr;
    gelf_getshdr(mvdata.scn, &shdr);
    dvaddr = shdr.sh_addr;
    for (auto& e:fns) {
        if (e->is_fixed())
            continue;
        e->set_mvfn_vaddr(dvaddr + mvdata_sz);
        mvdata_sz += e->make_mvdata(dbuf+mvdata_sz, &mvdata, dvaddr+mvdata_sz);
        mvfn_sz += e->make_info(buf+mvfn_sz, &mvfn, vaddr+mvfn_sz);
    }

    /* multiverse callsites */
    buf = mvcs.dirty_buf();
    gelf_getshdr(mvcs.scn, &shdr);
    vaddr = shdr.sh_addr;
    for (auto& e:pps) {
        if ( e->_fn->is_fixed() || e->pp.type == PP_TYPE_X86_JUMP)
            continue;
        mvcs_sz += e->make_info(buf+mvcs_sz, &mvcs, vaddr+mvcs_sz);
    }

    mvvar.mark_boundry(&data, mvvar_sz);
    mvfn.mark_boundry(&data, mvfn_sz);
    mvcs.mark_boundry(&data, mvcs_sz);
    mvdata.set_size(mvdata_sz);

    update_relocs_sym();
    dynamic.write();
}

// See: libelf by example
void Bintail::write() {
    elf_fill(0xcccccccc);
    if (elf_update(e, ELF_C_NULL) < 0) {
        cout << elf_errmsg(elf_errno());
        errx(1, "elf_update(null) failed.");
    }
    if (elf_update(e, ELF_C_WRITE) < 0)
        errx(1, "elf_update(write) failed.");
}

/*
 * PRINTING
 */
void Bintail::print_sym() {
    cout << ANSI_COLOR_YELLOW "\nSyms:\n" ANSI_COLOR_RESET; 
    for (auto& sym : syms) {
        cout << "\t" << setw(34) << sym.name << hex
             << " type=" << GELF_ST_TYPE(sym.sym.st_info)
             << " bind=" << GELF_ST_BIND(sym.sym.st_info) << " "
             << sym.sym.st_other << "\t" /* Symbol visibility */
             << sym.sym.st_value << "\t" /* Symbol value */
             << sym.sym.st_size   << "\t" /* Symbol size */
             << endl;
    }
    cout << ANSI_COLOR_YELLOW "\nboundry ptr:" ANSI_COLOR_RESET << hex
         << "\n\tmvvar.start_ptr=0x" << mvvar.start_ptr
         << "\n\tmvvar.stop_ptr=0x" << mvvar.stop_ptr
         << "\n\tmvfn.start_ptr=0x" << mvfn.start_ptr
         << "\n\tmvfn.stop_ptr=0x" << mvfn.stop_ptr
         << "\n\tmvcs.start_ptr=0x" << mvcs.start_ptr
         << "\n\tmvcs.stop_ptr=0x" << mvcs.stop_ptr << "\n";
}

void Bintail::print_reloc() {
#define PRINT_RELOC(S, T) \
    cout << ANSI_COLOR_YELLOW #S ":\n" ANSI_COLOR_RESET; \
    S.print(sizeof(T));

    PRINT_RELOC(mvcs, mv_info_callsite);
    PRINT_RELOC(mvfn, mv_info_fn);
    PRINT_RELOC(mvvar, mv_info_var);
    PRINT_RELOC(mvtext, mv_info_callsite);
    PRINT_RELOC(mvdata, mv_info_callsite);

    cout << ANSI_COLOR_RED "\nRela other:\n" ANSI_COLOR_RESET; 
    for (auto rela : rela_other) {
        cout << hex << " offset=0x" << rela.r_offset
             << " addend=0x" << rela.r_addend;
        for (auto s : secs)
            if (rela.r_offset < s.shdr.sh_addr + s.shdr.sh_size 
                    && rela.r_offset >= s.shdr.sh_addr)
                cout << " - " << s.name;
        cout << endl;
    }
}

void Bintail::print_dyn() {
    cout << ANSI_COLOR_YELLOW ".dynamic: \n" ANSI_COLOR_RESET;
    dynamic.print();
}

void Bintail::print() {
    for (auto& var : vars)
        var->print();
}
