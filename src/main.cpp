#include <cstdint>
#include <cstdio>
#include <queue>
#include <string>
#include <utility>
#include <vector>

// ------------------------- DATATYPES ------------------------------

enum class DEDUCTION_TYPE;
struct attrset_t;
struct fd_t;
struct fd_set;
struct reason_t;
struct deduction_set;

// ---------------------- JAVASCRIPT API ----------------------------
extern "C" {

void reset(int qtype, int n_attrs);
void add_given(uint32_t lhs, uint32_t rhs);
int add_deduction(uint32_t lhs, uint32_t rhs, int type, int ref1, int ref2);
void erase_deductions_after(int n);
int deduction_hint(uint32_t target_lhs, uint32_t target_rhs, int hint_type);

int set_closure(uint32_t val, int ref);
int closure_hint(int hint_type);

}
// --------------------- PUBLIC OPERATIONS --------------------------

attrset_t closure(attrset_t base_attrs, const fd_set& fds);
deduction_set consequence(const fd_set& fds, const fd_t& target);
std::vector<attrset_t> candidate_keys_exhaustive(attrset_t superkey, const fd_set& fds);
std::vector<attrset_t> candidate_keys_heuristic(attrset_t superkey, const fd_set& fds);

//int add_deduction_consequence(uint32_t lhs, uint32_t rhs, int type, int ref1, int ref2);
//int add_deduction_closure(uint32_t lhs, int ref);
//int add_deduction_exhaustive(uint32_t lhs, int ref);

// --------------------- INTERNAL HELPERS ---------------------------

static std::string pretty_print(const attrset_t attr);
static std::string pretty_print(const fd_t& fd);
static std::string pretty_print(const reason_t& fd);
static std::string pretty_print(const deduction_set& s);

static constexpr bool try_transitive       (const fd_t& f, const fd_t& s, fd_t& o_f, reason_t& o_r);
static constexpr bool try_pseudotransitive (const fd_t& f, const fd_t& s, fd_t& o_f, reason_t& o_r);
static constexpr bool try_union            (const fd_t& f, const fd_t& s, fd_t& o_f, reason_t& o_r);
static constexpr bool try_decompose        (const fd_t& f, attrset_t rhs, fd_t& o_f, reason_t& o_r);
static constexpr bool try_extend           (const fd_t& f, attrset_t lhs, fd_t& o_f, reason_t& o_r);
static constexpr bool try_augment          (const fd_t& f, attrset_t lhs, fd_t& o_f, reason_t& o_r);

// ------------------------- TYPE IMPL ------------------------------

enum class DEDUCTION_TYPE {
    GIVEN,
    REFLEXIVITY,
    TRANSITIVITY,
    AUGMENTATION,
    UNION,
    DECOMPOSITION,
    PSEUDOTRANSITIVITY,
};
const char* ui_names_deductions[] = {
    "Given",
    "Reflexivity",
    "Transitivity",
    "Augmentation",
    "Union",
    "Decomposition",
    "Pseudotransitivity",
};

struct attrset_t {
    uint32_t val;
    constexpr attrset_t() : val(0) { }
    constexpr attrset_t(uint32_t v) : val(v) { }
    constexpr operator uint32_t() const {
        return val;
    }
    constexpr static attrset_t n_bits(int n) {
        attrset_t attrs;
        for (int i = 0; i < n; i++) attrs.val |= 1 << n;
        return attrs;
    }
    constexpr int popcnt() const {
        return __builtin_popcount(val);
    }
    constexpr bool operator==(const attrset_t& other) const {
        return val == other.val;
    }
    constexpr bool operator!=(const attrset_t& other) const {
        return val != other.val;
    }
    constexpr bool contains(const attrset_t& other) const {
        return !(~val & other.val);
    }
    constexpr bool try_cover(const fd_t& other);
    std::vector<attrset_t> all_subsets(int n_attrs) const {
        if (popcnt() == 0)
            return std::vector<attrset_t>{};
        else if(popcnt() == 1)
            return std::vector<attrset_t>{*this};
        int bit = n_attrs;
        attrset_t val2 = 0;
        for (; bit > 0; bit--) {
            val2 = (val << (32 - bit)) >> (32 - bit);
            if (val2.val != val) break;
        }
        std::vector<attrset_t> vec = val2.all_subsets(n_attrs - 1);
        int sz = vec.size();
        vec.emplace_back(1 << bit);
        for (int i = 0; i < sz; i++)
            vec.emplace_back(vec[i] | (1 << bit));
        return vec;
    }
};

struct fd_t {
    attrset_t lhs = {0};
    attrset_t rhs = {0};
    constexpr bool operator==(const fd_t& other) const {
        return lhs == other.lhs && rhs == other.rhs;
    }
    constexpr bool operator!=(const fd_t& other) const {
        return lhs != other.lhs || rhs != other.rhs;
    }
};

struct fd_set {
    int n_attrs;
    std::vector<fd_t> v;
    fd_set() = default;
    fd_set(int n_attrs) : n_attrs(n_attrs) { }
    constexpr fd_t operator[](int idx) const {
        return v[idx];
    }
    constexpr bool covers(const fd_t& fd) {
        return closure(fd.lhs, *this).contains(fd.rhs);
    }
    constexpr bool covers(const fd_set& other) {
        for (int i = 0; i < other.v.size(); i++)
            if (!covers(other[i]))
                return false;
        return true;
    }
    constexpr int find(const fd_t& fd) {
        for (int i = 0; i < v.size(); i++)
            if (v[i] == fd)
                return i;
        return -1;
    }
    constexpr bool contains(const fd_t& fd) {
        return find(fd) >= 0;
    }
    constexpr int create_or_find(const fd_t& fd) {
        int idx = find(fd);
        if (idx == -1) {
            idx = v.size();
            v.emplace_back(fd);
        }
        return idx;
    }
};

struct reason_t {
    DEDUCTION_TYPE type;
    int references[2] = {-1, -1};
};

struct deduction_set {
    fd_set f;
    std::vector<std::vector<reason_t>> r;
    int create_or_find(const fd_t& fd) {
        int idx = f.find(fd);
        if (idx == -1) {
            idx = f.v.size();
            f.v.emplace_back(fd);
            r.emplace_back();
        }
        return idx;
    }
    deduction_set prune() const {
        deduction_set s = *this;
        for (int i = 0; i < s.r.size(); i++) {
            for (int j = 0; j < s.r[i].size(); j++) {
                reason_t& r = s.r[i][j];
                if (r.references[0] > i || 
                (r.type != DEDUCTION_TYPE::AUGMENTATION && r.type != DEDUCTION_TYPE::DECOMPOSITION && r.references[1] > i))
                    s.r[i].erase(s.r[i].begin() + j--);
            }
        }
        return s;
    }
    deduction_set reduce(const fd_t& target) const {
        deduction_set s = *this;
        attrset_t target_overlap = target.rhs & target.lhs;
        fd_t target_reduced {target.lhs, target.rhs ^ target_overlap};
        int start = s.create_or_find(target_reduced) + 1;
        //printf("erasing %i .. %i\n", start + 1, (int)s.r.size() + 1);
        if (s.r.size() - start) {
            s.f.v.erase(s.f.v.begin() + start, s.f.v.begin() + s.r.size());
            s.r.erase(s.r.begin() + start, s.r.begin() + s.r.size());
        }
        while (start > 1) {
            int minmax = 0;
            for (int i = start - 1; i < s.r.size(); i++) {
                int minmax2 = start - 1;
                for (int j = 0; j < s.r[i].size(); j++) {
                    if (s.r[i][j].type == DEDUCTION_TYPE::GIVEN || s.r[i][j].type == DEDUCTION_TYPE::REFLEXIVITY)
                        continue;
                    int minmax3 = 0;
                    if (s.r[i][j].references[0] < start - 1)
                        minmax3 = s.r[i][j].references[0];
                    if (s.r[i][j].type != DEDUCTION_TYPE::AUGMENTATION && s.r[i][j].type != DEDUCTION_TYPE::DECOMPOSITION
                    && s.r[i][j].references[1] < start - 1 && s.r[i][j].references[1] >= minmax3)
                        minmax3 = s.r[i][j].references[1];
                    if (minmax3 < minmax2)
                        minmax2 = minmax3;
                }
                if (minmax2 > minmax)
                    minmax = minmax2;
            }
            //printf("minmax in %i .. %i is %i\n", start, (int)s.r.size(), minmax + 1);
            if (minmax == start - 1) {
                start = minmax;
                continue;
            }

            for (int i = start - 1; i < s.r.size(); i++) {
                for (int j = 0; j < s.r[i].size(); j++) {
                    bool bad = false;
                    if (s.r[i][j].type == DEDUCTION_TYPE::GIVEN || s.r[i][j].type == DEDUCTION_TYPE::REFLEXIVITY)
                        continue;
                    bad |= (s.r[i][j].references[0] > minmax && s.r[i][j].references[0] < start - 1);
                    if (s.r[i][j].type != DEDUCTION_TYPE::AUGMENTATION && s.r[i][j].type != DEDUCTION_TYPE::DECOMPOSITION)
                        bad |= (s.r[i][j].references[1] > minmax && s.r[i][j].references[1] < start - 1);
                    if (bad) {
                        //printf("deleting %s\n", pretty_print(s.r[i][j]).c_str());
                        s.r[i].erase(s.r[i].begin() + j--);
                    }
                    //else
                    //    printf("looked at %s\n", pretty_print(s.r[i][j]).c_str());
                }
            }
            if (start == 2) break;
            for (int i = start; i < s.r.size(); i++) {
                for (int j = 0; j < s.r[i].size(); j++) {
                    printf("inspecting %s\n", pretty_print(s.r[i][j]).c_str());
                    if (s.r[i][j].type == DEDUCTION_TYPE::GIVEN || s.r[i][j].type == DEDUCTION_TYPE::REFLEXIVITY)
                        continue;
                    if (s.r[i][j].references[0] > minmax)
                        s.r[i][j].references[0] -= start - minmax - 2;
                    if (s.r[i][j].type != DEDUCTION_TYPE::AUGMENTATION && s.r[i][j].type != DEDUCTION_TYPE::DECOMPOSITION)
                        if (s.r[i][j].references[1] > minmax)
                            s.r[i][j].references[1] -= start - minmax - 2;
                    //printf("reduced %s\n", pretty_print(s.r[i][j]).c_str());
                }
            }
            int end = start - 1;
            start = minmax + 1;
            //printf("erasing %i .. %i\n", start + 1, end);
            if (end - start - 1 > 0) {
                s.r.erase(s.r.begin() + start, s.r.begin() + end);
                s.f.v.erase(s.f.v.begin() + start, s.f.v.begin() + end);
            }
            if (end < start) break;
        }
        if (target_overlap) {
            s.f.v.emplace_back(target);
            s.r.emplace_back(std::vector<reason_t>({reason_t{DEDUCTION_TYPE::AUGMENTATION, {(int)s.r.size() - 1, target_overlap}}}));
        }
        return s;
    }
};

// ------------------------- FUNC IMPL ------------------------------

static std::string pretty_print(attrset_t attr) {
    std::string s{};
    char c = 'A';
    while (attr) {
        if (attr.val & 1) s.append(1, c);
        c++;
        attr.val >>= 1;
    }
    return s;
}
static std::string pretty_print(const fd_t& fd) {
    return pretty_print(fd.lhs) + "->" + pretty_print(fd.rhs);
}
static std::string pretty_print(const reason_t& r) {
    std::string ret{ui_names_deductions[(int)r.type]};
    if (r.references[1] >= 0) {
        if (r.type == DEDUCTION_TYPE::DECOMPOSITION || r.type == DEDUCTION_TYPE::AUGMENTATION) 
            ret += " " + pretty_print(attrset_t(r.references[1]));
        else
            ret += " " + std::to_string(r.references[1] + 1);
    }
    if (r.references[0] >= 0)
        ret += " " + std::to_string(r.references[0] + 1);
    return ret;
}
static std::string pretty_print(const deduction_set& s) {
    std::string output;
    for (int i = 0; i < s.f.v.size(); i++) {
        output += std::to_string(i + 1) + ". " + pretty_print(s.f[i]) + "\n";
        for (int j = 0; j < s.r[i].size(); j++)
            output += "    " + pretty_print(s.r[i][j]) + "\n";
    }
    return output;
}

constexpr bool attrset_t::try_cover(const fd_t& other) {
    if (!contains(other.lhs))
        return false;
    //if (contains(other.rhs))
    //    return false;
    val |= other.rhs.val;
    return true;
}

attrset_t closure(attrset_t attrs, const fd_set& base_fds) {
    fd_set fds = base_fds;
    attrset_t prev_set = attrs;
    do {
        prev_set = attrs;
        for (int i = 0; i < fds.v.size(); i++) {
            if (attrs.try_cover(fds[i]))
                fds.v.erase(fds.v.begin() + i--);
        }
    } while (attrs.popcnt() < fds.n_attrs && attrs != prev_set);
    return attrs;
}

static constexpr bool try_transitive(const fd_t& f, const fd_t& s, fd_t& o_f, reason_t& o_r) {
    if (f.rhs != s.lhs) 
        return false;
    o_f = {f.lhs, s.rhs};
    o_r.type = DEDUCTION_TYPE::TRANSITIVITY;
    return true;
}
static constexpr bool try_union(const fd_t& f, const fd_t& s, fd_t& o_f, reason_t& o_r) {
    if (f.lhs != s.lhs || f.rhs.contains(s.rhs) || s.rhs.contains(f.rhs))
        return false;
    o_f = {f.lhs, f.rhs.val | s.rhs.val};
    o_r.type = DEDUCTION_TYPE::UNION;
    return true;
}
static constexpr bool try_pseudotransitive(const fd_t& f, const fd_t& s, fd_t& o_f, reason_t& o_r) {
    if (f.rhs == s.lhs || !s.lhs.contains(f.rhs) || s.lhs.contains(f.lhs & ~f.rhs))
        return false;
    o_f = {s.lhs & ~f.rhs | f.lhs, s.rhs};
    o_r.type = DEDUCTION_TYPE::PSEUDOTRANSITIVITY;
    return true;
}
static constexpr bool try_decompose(const fd_t& f, attrset_t rhs, fd_t& o_f, reason_t& o_r) {
    o_f = {f.lhs, rhs};
    o_r.type = DEDUCTION_TYPE::DECOMPOSITION;
    return true;
}
static constexpr bool try_extend(const fd_t& f, attrset_t lhs, fd_t& o_f, reason_t& o_r) {
    o_f = {f.lhs | lhs, f.rhs};
    o_r.type = DEDUCTION_TYPE::AUGMENTATION;
    return true;
}
static constexpr bool try_augment(const fd_t& f, attrset_t lhs, fd_t& o_f, reason_t& o_r) {
    o_f = {f.lhs | lhs, f.rhs | lhs};
    o_r.type = DEDUCTION_TYPE::AUGMENTATION;
    return true;
}

deduction_set consequence(const fd_set& fds, const fd_t& target) {
    deduction_set r{fds};
    for (int i = 0; i < fds.v.size(); i++)
        r.r.emplace_back(std::vector<reason_t>({reason_t{ DEDUCTION_TYPE::GIVEN }}));
    //for (int i = 0; i < fds.n_attrs; i++) {
    //    r.f.v.emplace_back(fd_t{1 << i, 1 << i});
    //    r.r.emplace_back(std::vector<reason_t>({reason_t{ DEDUCTION_TYPE::REFLEXIVITY }}));
    //}

    for (int i = 1; i < r.f.v.size(); i++) {
        fd_t out_fd;
        reason_t out_reason = {DEDUCTION_TYPE::GIVEN, {i, 0}};
        bool exit_now = false;
        auto check_axiom = [&r, &out_fd, &out_reason, &exit_now, &target](bool b){
            if (b) {
                if (out_fd == target)
                    exit_now = true;
                int idx = r.create_or_find(out_fd);
                if (idx > out_reason.references[0])
                    r.r[idx].emplace_back(out_reason);
            }
            return b;
        };

        for (int j = 0; j < i; j++) {
            out_reason.references[1] = j;
            if (!check_axiom(try_transitive(r.f[i], r.f[j], out_fd, out_reason)))
                check_axiom(try_transitive(r.f[j], r.f[i], out_fd, out_reason));
            check_axiom(try_union(r.f[i], r.f[j], out_fd, out_reason));
            if (!check_axiom(try_pseudotransitive(r.f[i], r.f[j], out_fd, out_reason)))
                check_axiom(try_pseudotransitive(r.f[j], r.f[i], out_fd, out_reason));
            if (exit_now) return r;
        }
        std::vector<attrset_t> rhs_subset = r.f[i].rhs.all_subsets(r.f.n_attrs);
        for (int j = 0; j < rhs_subset.size() - 1; j++) {
            out_reason.references[1] = rhs_subset[j];
            check_axiom(try_decompose(r.f[i], rhs_subset[j], out_fd, out_reason));
            if (exit_now) return r;
        }
        //std::vector<attrset_t> lhsdiff_subset = attrset_t(r.f[i].rhs & ~r.f[i].lhs).all_subsets(r.f.n_attrs);
        //for (int j = 0; j < lhsdiff_subset.size() - 1; j++) {
        //    out_reason.references[1] = lhsdiff_subset[j];
        //    check_axiom(try_extend(r.f[i], lhsdiff_subset[j], out_fd, out_reason));
        //    if (exit_now) return r;
        //}
        if (r.r.size() > 1000) break;
    }
    return r;
}

int question_type;
int num_givens;
fd_set working_set;
attrset_t closure_set;
deduction_set cached_path;
bool cache_dirty;

extern "C" void reset(int qtype, int n_attrs) {
    question_type = qtype;
    num_givens = 0;
    working_set = {n_attrs};
    cache_dirty = true;
}
extern "C" void add_given(uint32_t lhs, uint32_t rhs) {
    working_set.v.emplace_back(fd_t{lhs, rhs});
    num_givens++;
    cache_dirty = true;
}

extern "C" int add_deduction(uint32_t lhs, uint32_t rhs, int type, int ref1, int ref2) {
    cache_dirty = true;
    fd_t result = fd_t{lhs, rhs};
    if (type != (int)DEDUCTION_TYPE::GIVEN)
        working_set.v.emplace_back(result);
    if (ref1 >= working_set.v.size() || ref2 >= working_set.v.size())
        return -2;

    reason_t out_reason;
    fd_t out_fd = result;
    bool prerequisites_failed = false;
    switch((DEDUCTION_TYPE)type) {
    case DEDUCTION_TYPE::GIVEN:
        prerequisites_failed = working_set.find(result) >= num_givens || working_set.find(result) || (ref1 && working_set[ref1] != result);
        out_fd = working_set[ref1];
        break;
    case DEDUCTION_TYPE::REFLEXIVITY:
        prerequisites_failed = lhs != rhs;
        break;
    case DEDUCTION_TYPE::TRANSITIVITY:
        prerequisites_failed = !try_transitive(working_set[ref1], working_set[ref2], out_fd, out_reason)
                            && !try_transitive(working_set[ref2], working_set[ref1], out_fd, out_reason);
        break;
    case DEDUCTION_TYPE::AUGMENTATION:
        prerequisites_failed = !attrset_t(lhs).contains(~working_set[ref1].rhs & rhs);
        break;
    case DEDUCTION_TYPE::UNION:
        prerequisites_failed = !try_union(working_set[ref1], working_set[ref2], out_fd, out_reason);
        break;
    case DEDUCTION_TYPE::DECOMPOSITION:
        prerequisites_failed = !working_set[ref1].rhs.contains(rhs) || lhs != working_set[ref1].lhs;
        out_fd = {working_set[ref1].lhs, rhs};
        break;
    case DEDUCTION_TYPE::PSEUDOTRANSITIVITY:
        prerequisites_failed = !try_pseudotransitive(working_set[ref1], working_set[ref2], out_fd, out_reason)
                            && !try_pseudotransitive(working_set[ref2], working_set[ref1], out_fd, out_reason);
        break;
    }
    if (prerequisites_failed)
        return -1;
    if (result != out_fd)
        return -3;
    return 0;
}
extern "C" void erase_deductions_after(int n) {
    working_set.v.erase(working_set.v.begin() + n, working_set.v.begin() + working_set.v.size());
    cache_dirty = true;
}
extern "C" int deduction_hint(uint32_t target_lhs, uint32_t target_rhs, int hint_type) {
    if (working_set[working_set.v.size() - 1] == fd_t{target_lhs, target_rhs})
        return -1;

    if (cache_dirty) {
        deduction_set full_set = consequence(working_set, fd_t{target_lhs, target_rhs}).prune();
        cached_path = full_set.reduce(fd_t{target_lhs, target_rhs});
        printf("set: %s\n", pretty_print(full_set).c_str());
        printf("set (pruned): %s\n", pretty_print(cached_path).c_str());
        printf("target: %s\n", pretty_print(cached_path.f[working_set.v.size()]).c_str());
        cache_dirty = false;
    }
    fd_t target_fd = cached_path.f[working_set.v.size()];
    reason_t target_reason = cached_path.r[working_set.v.size()].size() ? cached_path.r[working_set.v.size()][0] : reason_t{};
    switch(hint_type) {
        case 0: return (int)target_reason.type;
        case 1: return target_reason.references[0];
        case 2:
            if (target_reason.type == DEDUCTION_TYPE::AUGMENTATION || target_reason.type == DEDUCTION_TYPE::REFLEXIVITY)
                return -1;
            return target_reason.references[1];
        case 3: return target_fd.lhs;
        case 4: return target_fd.rhs;
        default: return -1;
    }
}

extern "C" int set_closure(uint32_t val, int ref) {
    if (ref == -1) {
        closure_set = val;
        return 0;
    }
    else {
        if (closure_set.try_cover(working_set[ref])) {
            if (closure_set == attrset_t(val))
                return 0;
        }
        return -3;
    }
}
extern "C" int closure_hint(int hint_type) {
    attrset_t attrs = closure_set;
    int ref = 0;
    for (int i = 0; i < working_set.v.size(); i++) {
        if (attrs.try_cover(working_set[i])) {
            ref = i;
            break;
        }
    }
    if (hint_type == 0) return (int)attrs;
    else return ref;
}

extern "C" int test_fn() {
    fd_set fds(5);
    fds.v.emplace_back(fd_t{1, 2});
    fds.v.emplace_back(fd_t{2, 4});
    fds.v.emplace_back(fd_t{6, 8});
    fds.v.emplace_back(fd_t{14, 16});
    fd_t all{1, 31};
    deduction_set set = consequence(fds, all).prune();
    printf("%s\n", pretty_print(set).c_str());
    deduction_set set2 = set.reduce(all);
    printf("%s\n", pretty_print(set2).c_str());

    return 0;
}