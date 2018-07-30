#ifndef __MVELEM_H
#define __MVELEM_H

#include <set>
#include <vector>
#include <memory>
#include <cstddef>
#include "bintail.h"

class MVFn;
class MVVar;
class MVPP;

class MVData {
public:
    virtual size_t make_info(std::byte* buf, Section* scn, uint64_t vaddr) = 0;
    virtual ~MVData() {}
};

//-----------------------------------------------------------------------------
class MVText : public MVData {
public:
    MVText(std::byte* buf, size_t size, uint64_t vaddr);
    size_t make_info(std::byte* buf, Section* scn, uint64_t vaddr);
private:
    long orig_vaddr;
    std::vector<std::byte> instr;
};

//-----------------------------------------------------------------------------
struct mv_info_assignment {
    union {
        uint64_t location;
        int info; // Runtime link
    } variable;
    uint32_t lower_bound;
    uint32_t upper_bound;
};

class MVassign : public MVData {
public:
    MVassign(struct mv_info_assignment& _assign);
    size_t make_info(std::byte* buf, Section* scn, uint64_t vaddr);
    uint64_t location();
    bool active();
    void link_var(MVVar* _var);
    void print();
    MVVar* var;
private:
    struct mv_info_assignment assign;
};

//-----------------------------------------------------------------------------
typedef enum {
    MVFN_TYPE_NONE,
    MVFN_TYPE_NOP,
    MVFN_TYPE_CONSTANT,
    MVFN_TYPE_CLI,
    MVFN_TYPE_STI,
} mvfn_type_t;

struct mv_info_mvfn {
    // static
    uint64_t function_body;    // A pointer to the mvfn's function body
    unsigned int n_assignments;  // The mvfn's variable assignments
    uint64_t assignments;      // Array of mv_info_assignment

    // runtime
    int type;                    // This is be interpreted as mv_type_t
                                 // (declared as integer to ensure correct size)
    uint32_t constant;
};

class MVmvfn : public MVData {
public:
    MVmvfn(struct mv_info_mvfn& _mvfn, Section* data, Section* text);
    size_t make_info(std::byte* buf, Section* scn, uint64_t vaddr);
    size_t make_info_ass(std::byte* buf, Section* scn, uint64_t vaddr);
    void set_info_assigns(uint64_t vaddr);
    void check_var(MVVar* var, MVFn* fn);
    void print(bool active);
    bool active();
    bool frozen();
    /**
      @brief decode mvfn function body

      If a multiverse function body does nothing, or only returns a
      constant value, we can further optimize the patched callsites. For a
      dummy architecture implementation, this operation can be implemented
      as a NOP.
    */
    void decode_mvfn_body(struct mv_info_mvfn *info, uint8_t * op);

    uint64_t location() { return mvfn.function_body; }
    struct mv_info_mvfn mvfn;
private:
    std::vector<std::unique_ptr<MVassign>> assigns;
};

//-----------------------------------------------------------------------------
struct mv_info_fn {
    // static
    uint64_t name;             // Functions's symbol name
    uint64_t function_body;    // A pointer to the original (generic) function body
    unsigned int n_mv_functions; // Specialized multiverse variant functions of this function
    uint64_t mv_functions;     // Array of mv_info_mvfn

    // runtime
    struct mv_patchpoint *patchpoints_head;  // Patchpoints as linked list TODO: arch-specific
    struct mv_info_mvfn *active_mvfn; // The currently active mvfn
};

class MVFn : public MVData {
public:
    MVFn(struct mv_info_fn& _fn, Section* data, Section* text);
    size_t make_info(std::byte* buf, Section* scn, uint64_t vaddr);
    void print(Section* rodata, Section* text, Section* mvtext);
    void check_var(MVVar* var);
    void add_pp(MVPP* pp);
    uint64_t location();
    void apply(Section* text, Section* mvtext);
    bool is_fixed();
    size_t make_mvdata(std::byte* buf, Section* mvdata, uint64_t vaddr);
    void set_mvfn_vaddr(uint64_t vaddr);

    struct mv_info_fn fn;
    bool frozen;
    uint64_t active;
    uint64_t mvfn_vaddr;
private:
    std::vector<MVPP*> pps;
    std::vector<std::unique_ptr<MVmvfn>> mvfns;
};

//-----------------------------------------------------------------------------
struct mv_info_fn_ref {
    struct mv_info_fn_ref *next;
    struct mv_info_fn *fn;
};

struct mv_info_var {
    uint64_t name;
    uint64_t variable_location;         // A pointer to the variable
    union {
        unsigned int info;
        struct {
            unsigned int
                variable_width : 4,  // Width of the variable in bytes
                reserved       : 25, // Currently not used
                flag_tracked   : 1,  // Determines if the variable is tracked
                flag_signed    : 1,  // Determines if the variable is signed
                flag_bound     : 1;  // 1 if the variable is bound, 0 if not
                                     // -> this flag is mutable
        };
    };

    // runtime
    struct mv_info_fn_ref *functions_head; // Functions referening this variable
};

class MVVar : public MVData {
public:
    MVVar(struct mv_info_var _var, Section* rodata, Section* data);
    size_t make_info(std::byte* buf, Section* scn, uint64_t vaddr);
    void print(Section* rodata, Section* text, Section* mvtext);
    void check_fns(std::vector<std::unique_ptr<MVFn>>& fns);
    void link_fn(MVFn* fn);
    void set_value(int v, Section* data);
    void apply(Section* text, Section* mvtext);
    uint64_t location();

    std::string& name() { return _name; }
    int64_t value() { return _value; }

    bool frozen;
    struct mv_info_var var;
private:
    std::set<MVFn*> fns;
    std::string _name;
    int64_t _value;
};

//-----------------------------------------------------------------------------
struct mv_info_callsite {
    // static
    uint64_t function_body;
    uint64_t call_label;
};

typedef enum  {
    PP_TYPE_INVALID,
    PP_TYPE_X86_CALL,
    PP_TYPE_X86_CALL_INDIRECT,
    PP_TYPE_X86_JUMP,
} mv_info_patchpoint_type;

struct mv_patchpoint {
    struct mv_patchpoint *next;
    uint64_t* function;
    uint64_t location;                // == callsite call_label
    mv_info_patchpoint_type type;

    // Here we swap in the code, we overwrite
    unsigned char swapspace[6];
};

class MVPP : public MVData {
public:
    MVPP(MVFn* fn);
    MVPP(struct mv_info_callsite& cs, Section* text, Section* mvtext);
    bool invalid();
    void print(Section* text, Section* mvtext);
    void set_fn(MVFn* fn);

    size_t make_info(std::byte* buf, Section* scn, uint64_t vaddr);

    /* ret callee */
    uint64_t decode_callsite(struct mv_info_callsite& cs, Section* text);

    void patchpoint_apply(struct mv_info_mvfn *mvfn, Section* text, Section* mvtext);
    void patchpoint_revert();
    void patchpoint_size(void **from, void** to);

    struct mv_patchpoint pp;
    uint64_t function_body;
    MVFn* _fn;
private:
    bool fptr;
};
#endif