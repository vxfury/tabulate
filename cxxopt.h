/**
 * Copyright 2022 Kiran Nowak(kiran.nowak@gmail.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <exception>
#include <map>
#include <memory>
#include <unordered_map>
#include <algorithm>

#include <stdio.h>
#include <string.h>
#define TRACE(fmt, ...) // printf("%d: " fmt "\n", __LINE__, ##__VA_ARGS__)

namespace cxxopt
{
namespace conv
{
static inline std::string trim(const std::string &s)
{
    std::string tmp = s;
    tmp.erase(tmp.begin(), std::find_if(tmp.begin(), tmp.end(), [](unsigned char ch) {
                  return !std::isspace(ch);
              }));

    tmp.erase(std::find_if(tmp.rbegin(), tmp.rend(),
                           [](unsigned char ch) {
                               return !std::isspace(ch);
                           })
                  .base(),
              tmp.end());
    return tmp;
}

int derive(const std::string &from, bool &to)
{
    std::string tmp = trim(from);
    const char *p = tmp.c_str();

    char l = *p | ' ';
    if ((l == 't' && strncmp(++p, "rue\0", 4) == 0) || strncmp(p, "1\0", 2) == 0) {
        to = true;
    } else if ((l == 'f' && strncmp(++p, "alse\0", 5) == 0) || strncmp(p, "0\0", 2) == 0) {
        to = false;
    } else {
        return -EINVAL;
    }

    return 0;
}

int derive(const std::string &from, std::string &to)
{
    to = from;
    return 0;
}

template <typename T, typename std::enable_if<std::is_integral<T>::value>::type * = nullptr>
int derive(const std::string &from, T &to)
{
    std::string tmp = trim(from);
    if (tmp.empty()) {
        return -EINVAL;
    }

    bool negative = false;
    if (tmp[0] == '-') {
        negative = true;
    }
    unsigned long long v = std::stoull(negative ? tmp.substr(1) : tmp, nullptr, 0);

    to = static_cast<T>(v) * (negative ? -1 : 1);

    return 0;
}

template <typename T>
struct opStreamExists {
    struct yes {
        char a[1];
    };
    struct no {
        char a[2];
    };

    template <typename C>
    static yes test(__typeof__(&C::operator>>));
    template <typename C>
    static no test(...);

    enum { value = (sizeof(test<T>(0)) == sizeof(yes)) };
};

template <typename T, typename std::enable_if<(std::is_arithmetic<T>::value && !std::is_integral<T>::value)
                                              || opStreamExists<T>::value>::type * = nullptr>
int derive(const std::string &from, T &to)
{
    std::stringstream in(from);
    in >> to;
    if (!in) {
        TRACE("[from-string] Error(%d): derive failed(from = %s)", -EINVAL, from.c_str());
        return -EINVAL;
    }
    return 0;
}

template <typename To>
int derive(const std::string &from, std::vector<To> &to)
{
    std::string tmp = trim(from);
    if (tmp.empty()) {
        to.clear();
        TRACE("[from-string] Warning: Empty");
        return 0;
    }
    if (tmp[0] == '[' || tmp[0] == '{') {
        tmp = tmp.substr(1);
    }
    if (tmp[tmp.size() - 1] == ']' || tmp[tmp.size() - 1] == '}') {
        tmp = tmp.erase(tmp.size() - 1, 1);
    }

    std::stringstream ss(tmp);
    while (ss.good()) {
        std::getline(ss, tmp, ',');

        int err;
        To item;
        if ((err = derive(tmp, item)) != 0) {
            TRACE("[from-string] Error(%d): derive failed(from = %s)", err, tmp.c_str());
            return err;
        }
        to.push_back(item);
    }

    return 0;
}

template <typename Key, typename Value, class Hash = std::hash<Key>>
int derive(const std::string &from, std::unordered_map<Key, Value, Hash> &to)
{
    std::string tmp = trim(from);
    if (tmp.empty()) {
        to.clear();
        return 0;
    }
    std::stringstream ss(tmp);
    while (ss.good()) {
        std::string blk;
        std::getline(ss, blk, ';');
        {
            trim(blk);
            auto it = blk.find(':');
            if (it == std::string::npos) {
                return -EINVAL;
            }
            int err;
            Key key;
            if ((err = derive(blk.substr(0, it), key)) == 0) {
                Value val;
                if ((err = derive(blk.substr(it + 1), val)) != 0) {
                    TRACE("[from-string] Error(%d): derive failed(from = %s)", err, blk.substr(it + 1).c_str());
                    return err;
                }
                to[key] = val;
            }
        }
    }

    return 0;
}

template <typename T>
int derive(const std::string &from, std::vector<std::vector<T>> &to)
{
    std::string tmp = trim(from);
    if (tmp.empty()) {
        to.clear();
        return 0;
    }
    const char *s = tmp.c_str(), *p = s;
    const char *e = tmp.c_str() + tmp.length();

    int depth = 0;
    while (p <= e) {
        if (*p == '[' || *p == '{') {
            if (depth == 0) {
                s = ++p;
            } else {
                s = p++;
            }
            depth++;
            do {
                if (*p == '[' || *p == '{') {
                    depth++;
                } else if (*p == ']' || *p == '}') {
                    depth--;
                }
                p++;
            } while (*p != '\0' && depth != 1);

            int err;
            std::vector<T> vec;
            if ((err = derive(std::string(s, p++ - s), vec)) != 0) {
                TRACE("[from-string] Error(%d): derive failed(from = %s)", err, std::string(s, p - s).c_str());
                return err;
            }
            to.push_back(vec);

            while (*p == ',' || *p == ' ') {
                p++;
            }
        } else {
            p++;
        }
    }

    return 0;
}

int derive(const bool &from, std::string &to)
{
    to = from ? "true" : "false";
    return 0;
}

int derive(const char *from, std::string &to)
{
    to = std::string(from);
    return 0;
}

template <typename From, typename std::enable_if<std::is_arithmetic<From>::value>::type * = nullptr>
int derive(const From &from, std::string &to)
{
    std::stringstream ss;
    ss << from;
    to = ss.str();

    return 0;
}

template <typename T>
int derive(const std::vector<T> &from, std::string &to)
{
    to = "[";
    const std::string delim = ", ";
    for (auto const &e : from) {
        std::string str;
        if (derive(e, str) == 0) {
            to += str + delim;
        } else {
            return -EINVAL;
        }
    }
    if (from.size() != 0) {
        to.erase(to.size() - delim.size(), delim.size());
    }
    to += "]";

    return 0;
}

template <typename K, typename V, class Hash = std::hash<K>>
int derive(const std::unordered_map<K, V, Hash> &from, std::string &to)
{
    int err;
    for (auto const &e : from) {
        std::string key, val;
        if ((err = derive(e.first, key)) != 0) {
            TRACE("[to-string] Error(%d): derive failed", err);
            return err;
        }
        if ((err = derive(e.second, val)) != 0) {
            TRACE("[to-string] Error(%d): derive failed", err);
            return err;
        }
        to += key + ": " + val + "; ";
    }
    if (to.size() >= 2) {
        to.erase(to.size() - 2, 2); // pop last delimeters
    }

    return err;
}
} // namespace conv

class Values : public std::enable_shared_from_this<Values> {
  public:
    std::shared_ptr<Values> clone() const
    {
        return std::make_shared<Values>(*this);
    }

    int last() const
    {
        for (int i = values.size() - 1; i >= 0; i--) {
            if (values[i].has) {
                return i;
            }
        }
        return -1;
    }

    bool has(int level = -1) const
    {
        if (level <= -1) {
            return !values.empty();
        } else if (values.size() > static_cast<unsigned>(level) && values[level].has) {
            return true;
        } else {
            return false;
        }
    }

    template <typename T = std::string>
    T get(int level = -1) const
    {
        if (level <= -1) {
            level = last();
        }
        if (has(level)) {
            TRACE("GET(%d): %s", level, values[level].value.c_str());
            T value;
            if (conv::derive(values[level].value, value) == 0) {
                return value;
            } else {
                TRACE("GET(%d): exception", level);
                throw "Invalid Data: convert failed";
            }
        } else {
            TRACE("GET(%d): exception", level);
            throw "Invalid Arguments: data not exists";
        }
    }

    template <typename T = std::string>
    std::shared_ptr<Values> set(T v, int level = -1)
    {
        if (level <= -1) {
            level = values.size();
        }
        if (static_cast<unsigned>(level + 1) >= values.size()) {
            values.resize(level + 1);
        }
        if (conv::derive(v, values[level].value) == 0) {
            TRACE("SET(%d): %s", level, values[level].value.c_str());
            values[level].has = true;
        } else {
            TRACE("exception");
            throw "Invalid Arguments: data not exists";
        }

        return shared_from_this();
    }

    std::shared_ptr<Values> set_optarg(const char *arg, int level = -1)
    {
        if (level <= -1) {
            level = values.size();
        }
        if (static_cast<unsigned>(level + 1) >= values.size()) {
            values.resize(level + 1);
        }
        values[level].optarg = arg;
        values[level].value = std::string(arg);
        values[level].has = true;

        return shared_from_this();
    }

    const char *get_optarg(int level = -1) const
    {
        if (level <= -1) {
            level = values.size();
        }

        return values.at(level).optarg;
    }

    std::shared_ptr<Values> clear(int level = -1)
    {
        if (level <= -1) {
            level = last();
        }
        if (static_cast<unsigned>(level) < values.size()) {
            values[level].has = false;
        }

        return shared_from_this();
    }

    std::shared_ptr<Values> clear_all()
    {
        for (size_t i = 0; i < values.size(); i++) {
            values[i].has = false;
        }

        return shared_from_this();
    }

    template <typename T>
    std::string format(int level = -1) const
    {
        if (std::is_same<T, std::string>::value) {
            return get(level);
        } else {
            return std::make_shared<Values>()->set<T>(get<T>(level))->get();
        }
    }

    template <typename T = std::string>
    std::string description() const
    {
        int level = last();
        if (level >= 0) {
            std::string desc;
            bool first = true;
            while (level >= 0) {
                if (has(level)) {
                    if (!first) {
                        desc += ", ";
                    }
                    first = false;
                    desc += "\"" + format<T>(level) + "\"";
                    switch (level) {
                        case 0:
                            desc += "[default]";
                            break;
                        case 1:
                            desc += "[implicit]";
                            break;
                        case 2:
                            desc += "[explicit]";
                            break;
                        default:
                            desc += "\"[" + std::to_string(level) + "]";
                            break;
                    }
                }
                level--;
            }
            return desc;
        } else {
            return "<No Values>";
        }
    }

#define DEFINE_BY_LEVEL(level, suffix)                          \
    bool has##suffix() const                                    \
    {                                                           \
        return has(level);                                      \
    }                                                           \
    std::shared_ptr<Values> clear##suffix()                     \
    {                                                           \
        return clear(level);                                    \
    }                                                           \
    template <typename T = std::string>                         \
    std::shared_ptr<Values> set##suffix(T v)                    \
    {                                                           \
        return set<T>(v, level);                                \
    }                                                           \
    template <typename T = std::string>                         \
    T get##suffix() const                                       \
    {                                                           \
        if (!has(level)) {                                      \
            TRACE("exception");                                 \
            throw "Invalid Argument: value not setted";         \
        }                                                       \
        return get<T>(level);                                   \
    }                                                           \
    std::shared_ptr<Values> set_optarg##suffix(const char *arg) \
    {                                                           \
        return set_optarg(arg, level);                          \
    }                                                           \
    const char *get_optarg##suffix()                            \
    {                                                           \
        return get_optarg(level);                               \
    }

    DEFINE_BY_LEVEL(0, _default)
    DEFINE_BY_LEVEL(1, _implicit)
    DEFINE_BY_LEVEL(2, _explicit)

  private:
    struct optional_value {
        bool has = false;
        std::string value;
        const char *optarg;
    };
    std::vector<optional_value> values;
};

template <typename T = void>
inline std::shared_ptr<Values> Value()
{
    return std::make_shared<Values>();
}

enum {
    NOARG = 0,    // no argument
    REQUIRED = 1, // required argument
    OPTIONAL = 2, // optional argument
};

struct option {
    int id;
    int type;
    const char *name;
    const char *brief;
    std::shared_ptr<Values> value;

    option(int id, int type, const char *name, const char *brief, std::shared_ptr<Values> val)
        : id(id), type(type), name(name), brief(brief), value(std::move(val))
    {
    }
};

struct Results {
    std::map<int, std::string> id_to_name;
    std::map<int, std::shared_ptr<Values>> results_by_id;
    std::unordered_map<std::string, std::shared_ptr<Values>> results_by_name;

    Results() = default;
    ~Results() = default;

    bool has(int id)
    {
        return results_by_id.count(id) > 0;
    }

    bool has(const std::string &name)
    {
        return results_by_name.count(name) > 0;
    }

    const Values &operator[](int id) const
    {
        return *results_by_id.at(id);
    }

    const Values &operator[](const std::string &name) const
    {
        return *results_by_name.at(name);
    }

    template <typename T = std::string>
    T operator()(int id)
    {
        return results_by_id[id]->get<T>(-1);
    }

    template <typename T = std::string>
    T operator()(const std::string &name)
    {
        return results_by_name[name]->get<T>(-1);
    }

    std::string description(int indent = 0) const
    {
        std::string info;
        for (auto item : results_by_id) {
            info += std::string(indent, ' ');
            if (isprint(item.first)) {
                info += "-";
                info += static_cast<char>(item.first);
            }
            if (id_to_name.count(item.first)) {
                if (isprint(item.first)) {
                    info += ", ";
                }
                info += "--" + id_to_name.at(item.first);
            }
            info += ": " + item.second->description() + "\n";
        }
        return info;
    }
};

class Options {
  public:
    Options() {}
    Options(std::string program, std::string description) : program(program), description(description) {}

    option &add(const char *opts, const char *brief, std::shared_ptr<Values> val, int type,
                const std::string &group = "")
    {
        int id;
        const char *longopt = nullptr;

        {
            const char *p = opts;
            if (isalnum(*p) && (*(p + 1) == ',' || *(p + 1) == '\0')) {
                id = *p++;
                if (*p != '\0') {
                    p++;
                }
            } else {
                id = automatic_id++;
            }
            while (*p == ' ') {
                p++;
            }

            if (isalnum(*p)) {
                const char *store = p++;
                while (isalnum(*p) || *p == '-' || *p == '_') {
                    p++;
                }
                if (*p == '\0') {
                    longopt = store;
                } else {
                    TRACE("exception");
                    throw "Invalid Arguments: option format error";
                }
            }
        }

        auto &options = options_by_group[group];
        options.emplace_back(id, type, longopt, brief, std::move(val));

        return options[options.size() - 1];
    }

    class group_adder {
      public:
        group_adder(Options &options, std::string group) : options_(options), group_(std::move(group)) {}
        group_adder &operator()(const char *opts, const char *brief)
        {
            options_.add(opts, brief, Value(), NOARG, group_);
            return *this;
        }

        group_adder &operator()(const char *opts, const char *brief, std::shared_ptr<Values> val, int type = OPTIONAL)
        {
            options_.add(opts, brief, val, type, group_);
            return *this;
        }

      private:
        Options &options_;
        std::string group_;
    };
    group_adder add_group(std::string group = "")
    {
        return group_adder(*this, group);
    }

    class Parser {
      public:
        Parser(int argc, char **argv, const std::vector<option> &options)
            : argc(argc), argv(argv), options(options), optarg(nullptr), optind(0), opterr(0), optwhere(0)
        {
        }

        const option *next()
        {
            /* first, deal with silly parameters and easy stuff */
            if (argc == 0 || argv == nullptr || options.size() == 0 || optind >= argc || argv[optind] == nullptr) {
                opterr = EOF;
                return nullptr;
            }
            if (strcmp(argv[optind], "--") == 0) {
                optind++;
                opterr = EOF;
                TRACE("END: %s\n", debuginfo().c_str());
                return nullptr;
            }
            TRACE("START: %s", debuginfo().c_str());

            /* if this is our first time through */
            if (optind == 0) {
                optind = optwhere = 1;
            }
            int nonopts_index = 0, nonopts_count = 0, nextarg_offset = 0;

            /* find our next option, if we're at the beginning of one */
            if (optwhere == 1) {
                nonopts_index = optind;
                nonopts_count = 0;
                while (!is_option(argv[optind])) {
                    TRACE("skip non-option `%s`", argv[optind]);
                    optind++;
                    nonopts_count++;
                }
                if (argv[optind] == nullptr) {
                    /* no more options */
                    TRACE("no more options");
                    optind = nonopts_index;
                    opterr = EOF;
                    TRACE("END: %s\n", debuginfo().c_str());
                    return nullptr;
                } else if (strcmp(argv[optind], "--") == 0) {
                    /* no more options, but have to get `--' out of the way */
                    TRACE("no more options, but have to get `--` out of the way");
                    permute(argv + nonopts_index, nonopts_count, 1);
                    optind = nonopts_index + 1;
                    opterr = EOF;
                    TRACE("END: %s\n", debuginfo().c_str());
                    return nullptr;
                }
                TRACE("next option `%s`", argv[optind]);
            }

        try_again:
            /* End of option list? */
            if (argv[optind] == nullptr) {
                opterr = EOF;
                optind -= nonopts_count;
                TRACE("END: %s\n", debuginfo().c_str());
                return nullptr;
            }
            TRACE("got an option `%s`, where = %d", argv[optind], optwhere);

            /* we've got an option, so parse it */
            int match_index = -1;
            char *possible_arg = 0;
            const option *matched = nullptr;

            /* first, is it a long option? */
            if (memcmp(argv[optind], "--", 2) == 0 && optwhere == 1) {
                TRACE("long option `%s`", argv[optind]);
                optwhere = 2;
                match_index = -1;
                possible_arg = strchr(argv[optind] + optwhere, '=');
                size_t match_chars = 0;
                if (possible_arg == nullptr) {
                    /* no =, so next argv might be arg */
                    TRACE("no =, so next argv might be arg");
                    match_chars = strlen(argv[optind]);
                    possible_arg = argv[optind] + match_chars;
                    match_chars = match_chars - optwhere;
                } else {
                    TRACE("possible_arg `%s`", possible_arg);
                    match_chars = (possible_arg - argv[optind]) - optwhere;
                }
                for (decltype(options.size()) optindex = 0; optindex < options.size(); ++optindex) {
                    if (options[optindex].name != nullptr
                        && memcmp(argv[optind] + optwhere, options[optindex].name, match_chars) == 0) {
                        /* do we have an exact match? */
                        if (match_chars == strlen(options[optindex].name)) {
                            TRACE("have an exact matched option `%s`", argv[optind] + optwhere);
                            match_index = optindex;
                            matched = &options[match_index];
                            break;
                        } else {
                            TRACE("haven't an exact match, option `%s`, given `%s`", argv[optind] + optwhere,
                                  options[optindex].name);
                        }
                    }
                }
                if (match_index < 0) {
                    TRACE("skip options `%s`, optind = %d", argv[optind], optind);
                    optind++;
                    nonopts_count++;
                    optwhere = 1;

                    goto try_again;
                }
            } else if (argv[optind][0] == '-') {
                /* if we didn't find a long option, is it a short option? */
                auto it = std::find_if(options.begin(), options.end(), [&](const option &opt) {
                    return opt.id == argv[optind][optwhere];
                });
                if (it == options.end()) {
                    /* couldn't find option in shortopts */
                    TRACE("couldn't find option in shortopts: `%c`", argv[optind][optwhere]);
                    optwhere++;
                    if (argv[optind][optwhere] == '\0') {
                        TRACE("end of short options `%s`", argv[optind] + 1);
                        optind++;
                        optwhere = 1;
                    }
                    opterr = '?';
                    TRACE("END: %s\n", debuginfo().c_str());
                    return nullptr;
                }
                match_index = std::distance(options.begin(), it);
                possible_arg = argv[optind] + optwhere + 1;
                matched = &options[match_index];
                TRACE("match short option: `%c`, possible_arg = %s", argv[optind][optwhere], possible_arg);
            }

            /* get argument and reset optwhere */
            if (matched != nullptr) {
                switch (matched->type) {
                    case OPTIONAL:
                        if (*possible_arg == '=') {
                            possible_arg++;
                        }
                        optarg = (*possible_arg != '\0') ? possible_arg : 0;
                        optwhere = 1;
                        TRACE("(OPTIONAL)arg = `%s`", optarg ? optarg : "<null>");
                        break;
                    case REQUIRED:
                        if (*possible_arg == '=') {
                            possible_arg++;
                        }
                        if (*possible_arg != '\0') {
                            optarg = possible_arg;
                            optwhere = 1;
                        } else if (optind + 1 >= argc) {
                            optind++;
                            opterr = ':';
                            TRACE("END: %s\n", debuginfo().c_str());
                            return nullptr;
                        } else {
                            optarg = argv[optind + 1];
                            nextarg_offset = 1;
                            optwhere = 1;
                        }
                        TRACE("(REQUIRED)arg = `%s`", optarg);
                        break;
                    default: /* shouldn't happen */
                    case NOARG:
                        TRACE("(NOARG) %c(where = %d)", argv[optind][optwhere], optwhere);
                        optwhere++;
                        if (argv[optind][optwhere] == '\0') {
                            optwhere = 1;
                        }
                        optarg = 0;
                        break;
                }
            } else {
                optwhere = 1;
            }

            /* do we have to permute or otherwise modify optind? */
            TRACE("num-nonopts = %d", nonopts_count);
            if (nonopts_count != 0) {
                TRACE("(PERMUTE)from = %d, length = %d, gap = %d]", nonopts_index, nonopts_count, 1 + nextarg_offset);
                TRACE("before permute: %s", debuginfo().c_str());
                permute(argv + nonopts_index, nonopts_count, 1 + nextarg_offset);
                optind = nonopts_index + 1 + nextarg_offset;
                TRACE("after permute: %s", debuginfo().c_str());
            } else if (optwhere == 1) {
                optind = optind + 1 + nextarg_offset;
            }
            TRACE("END: %s\n", debuginfo().c_str());

            /* finally return */
            opterr = 0;
            return matched;
        }

        int ind() const
        {
            return optind;
        }

        int err() const
        {
            return opterr;
        }

        char *arg() const
        {
            return optarg;
        }

        std::string debuginfo() const
        {
            std::string info;
            for (int i = 1; i < argc; i++) {
                if (i == optind && optwhere == 1) {
                    info += "^";
                    info += argv[i];
                } else {
                    info += argv[i];
                }
                info += " ";
            }
            info.pop_back();

            return info;
        }

      private:
        int argc;
        char **argv;
        const std::vector<option> &options;

        char *optarg;
        int optind, opterr, optwhere;

        static void reverse(char **argv, int num)
        {
            for (int i = 0; i < (num >> 1); i++) {
                char *tmp = argv[i];
                argv[i] = argv[num - i - 1];
                argv[num - i - 1] = tmp;
            }
        }

        static void permute(char *const argv[], int len1, int len2)
        {
            reverse((char **)argv, len1);
            reverse((char **)argv, len1 + len2);
            reverse((char **)argv, len2);
        }

        /* is this argv-element an option or the end of the option list? */
        int is_option(char *arg)
        {
            return (arg == nullptr) || (arg[0] == '-');
        }
    };

    Results parse(int &argc, char **&argv)
    {
        std::vector<option> all_options;
        for (auto const &group : options_by_group) {
            all_options.insert(all_options.end(), group.second.begin(), group.second.end());
        }

        Results ret;
        const option *opt;
        Parser parser(argc, argv, all_options);
        while (parser.err() != EOF) {
            if ((opt = parser.next()) == nullptr) {
                TRACE("cann't get an option");
                continue;
            }
            if (!ret.has(opt->id)) {
                auto val = opt->value->clone();
                ret.results_by_id[opt->id] = val;
                if (opt->name != nullptr) {
                    ret.results_by_name[opt->name] = val;
                }
            }
            if (opt->type != NOARG) {
                if (parser.arg() != nullptr) {
                    TRACE("arg = %s", parser.arg());
                    ret.results_by_id[opt->id]->set_optarg_explicit(parser.arg());
                    if (opt->name != nullptr) {
                        ret.results_by_name[opt->name]->set_optarg_explicit(parser.arg());
                    }
                }
            }
        }

        for (auto &group : options_by_group) {
            for (auto &opt : group.second) {
                if (opt.value->has_default() && !ret.has(opt.id)) {
                    auto val = opt.value->clone()->clear_implicit();
                    ret.results_by_id[opt.id] = val;
                    if (opt.name != nullptr) {
                        ret.results_by_name[opt.name] = val;
                    }
                }
                if (opt.name != nullptr && opt.value->has()) {
                    ret.id_to_name[opt.id] = opt.name;
                }
            }
        }
        argc -= parser.ind();
        argv += parser.ind();

        return ret;
    }

    std::string usage() const
    {
        std::string str;
        str += program + " - " + description + "\n\n";
        str += "Mandatory arguments to long options are madatory for short options too.\n\n";

        auto headlen = head_length();
        if (options_by_group.count("")) {
            str += group_usage(options_by_group.at(""), headlen) + "\n";
        }

        for (auto const &group : options_by_group) {
            if (group.first == "") {
                continue;
            }
            str += " " + group.first + " options\n";
            str += group_usage(group.second, headlen) + "\n";
        }

        return str;
    }

  private:
    int automatic_id = 10000;
    std::string program, description;
    std::map<std::string, std::vector<option>> options_by_group;

    size_t head_length() const
    {
        size_t headlen = 0;
        for (auto const &group : options_by_group) {
            for (auto const &opt : group.second) {
                if (opt.name != nullptr) {
                    headlen = std::max(headlen, strlen(opt.name));
                }
            }
        }
        headlen += 10; /* -x, --xxxx [+:.] */

        return headlen;
    }

    std::string group_usage(const std::vector<option> &options, size_t headlen) const
    {
        std::string str;
        for (auto const &opt : options) {
            size_t len = headlen;
            str += std::string(2, ' ');
            if (std::isprint(opt.id)) {
                str += '-';
                str += static_cast<char>(opt.id);
                len -= 2;

                if (opt.name != nullptr) {
                    str += ", ";
                    len -= 2;
                }
            } else {
                str += "    ";
                len -= 4;
            }

            if (opt.name != nullptr) {
                str += "--";
                str += opt.name;
                len -= 2 + strlen(opt.name);
            }

            str += std::string(len - 4, ' ');

            str += ' ';
            str += '[';
            str += ".*:"[opt.type];
            str += ']';
            len -= 4;

            if (opt.brief != nullptr) {
                str += "   ";
                {
                    std::string line;
                    std::stringstream ss(opt.brief);
                    while (std::getline(ss, line, '\n')) {
                        if (str[str.length() - 1] == '\n') {
                            str += std::string(2 + headlen + 3, ' ');
                        }
                        str += line + "\n";
                    }
                }
            } else {
                str += '\n';
            }
        }
        return str;
    }
};

class Dispatcher {
  public:
    Dispatcher(std::string program) : program(std::move(program)) {}
    ~Dispatcher()
    {
        for (auto handler : handlers) {
            delete handler.second;
        }
    }

    int add(std::string name, int (*main)(int argc, char **argv))
    {
        if (handlers.count(name) == 0) {
            handlers[name] = new BasicHandler(main);
            return 0;
        } else {
            return -EEXIST;
        }
    }

    template <typename T, int (T::*cli_main)(int argc, char **argv)>
    int add(std::string name, T *obj)
    {
        if (handlers.count(name) == 0) {
            handlers[name] = new MemberHandler<T, T::cli_main>(obj);
            return 0;
        } else {
            return -EEXIST;
        }
    }

    int operator()(int argc, char **argv)
    {
        if (argc < 2 || *argv[1] == '-') {
            if (handlers.count("") != 0 && handlers.at("")->valid()) {
                return handlers.at("")->operator()(argc, argv);
            } else {
                std::string str = "`" + program + " ";
                if (handlers.size() > 0) {
                    for (auto &pair : handlers) {
                        str += pair.first + "|";
                    }
                    str.erase(str.size() - 1, 1);
                    str += " --help` for details";
                } else {
                    str += "No sub-command added";
                }
                std::cout << str << std::endl;

                return 0;
            }
        } else {
            if (handlers.count(argv[1]) > 0) {
                auto handler = handlers.at(argv[1]);
                return handler->valid() ? handler->operator()(argc - 1, argv + 1) : -EINVAL;
            }
            return -ENOTSUP;
        }
    }

  private:
    class Handler {
      public:
        virtual ~Handler() {}

        virtual bool valid() = 0;
        virtual int operator()(int argc, char **argv) = 0;
    };

    class BasicHandler : public Handler {
      public:
        BasicHandler(int (*handler)(int argc, char **argv)) : handler(handler) {}

        virtual bool valid() override
        {
            return handler != nullptr;
        }

        virtual int operator()(int argc, char **argv) override
        {
            return handler(argc, argv);
        }

      private:
        int (*handler)(int argc, char **argv);
    };

    template <typename T, int (T::*cli_main)(int argc, char **argv)>
    class MemberHandler : public Handler {
      public:
        MemberHandler(T *obj) : obj_(obj) {}

        virtual bool valid() override
        {
            return obj_ != nullptr;
        }

        virtual int operator()(int argc, char **argv) override
        {
            return (obj_->*cli_main)(argc, argv);
        }

      private:
        T *obj_;
    };
    std::string program;
    std::map<std::string, Handler *> handlers;
};
} // namespace cxxopt
