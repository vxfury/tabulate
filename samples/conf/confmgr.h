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

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <set>
#include <variant>
#include <atomic>
#include <type_traits>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <algorithm>
#include <stdio.h>
#include <google/protobuf/message.h>

#define TRACE(...) printf("%s:%d ", __FILE__, __LINE__), printf(__VA_ARGS__), printf("\n")

namespace conf
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

template <typename T,
          typename std::enable_if<std::is_base_of<google::protobuf::Message, T>::value>::type * = nullptr>
int derive(const std::string &from, T &to)
{
    return to.ParseFromString(from);
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

        To item;
        if (int err = derive(tmp, item); err != 0) {
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
            Key key;
            if (int err = derive(blk.substr(0, it), key); err == 0) {
                Value val;
                if (int err = derive(blk.substr(it + 1), val); err != 0) {
                    TRACE("[from-string] Error(%d): derive failed(from = %s)", err,
                          blk.substr(it + 1).c_str());
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

            std::vector<T> vec;
            if (int err = derive(std::string(s, p++ - s), vec); err != 0) {
                TRACE("[from-string] Error(%d): derive failed(from = %s)", err,
                      std::string(s, p - s).c_str());
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

template <typename From, typename std::enable_if<std::is_arithmetic<From>::value>::type * = nullptr>
int derive(const From &from, std::string &to)
{
    std::stringstream ss;
    ss << from;
    to = ss.str();

    return 0;
}

template <typename T,
          typename std::enable_if<std::is_base_of<google::protobuf::Message, T>::value>::type * = nullptr>
int derive(const T &from, std::string &to)
{
    return from.SerializeToString(&to) ? 0 : -EINVAL;
}

template <typename T>
int derive(const std::vector<T> &from, std::string &to)
{
    to = "[";
    const std::string delim = ", ";
    for (auto const &e : from) {
        if (std::string str; derive(e, str) == 0) {
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
    for (auto const &e : from) {
        std::string key, val;
        if (int err = derive(e.first, key); err != 0) {
            TRACE("[to-string] Error(%d): derive failed", err);
            return err;
        }
        if (int err = derive(e.second, val); err != 0) {
            TRACE("[to-string] Error(%d): derive failed", err);
            return err;
        }
        to += key + ": " + val + "; ";
    }
    if (to.size() >= 2) {
        to.erase(to.size() - 2, 2); // pop last delimeters
    }

    return 0;
}

template <typename To>
int derive(const std::string &from, To &to)
{
    TRACE("[from-string] Error(%d): Not Implemented", -ENOSYS);
    return -ENOSYS;
}
} // namespace conv

template <typename Key, typename... Values>
class manager {
  public:
    enum level { DEFAULT = 0, WORKSPACE, PULL_STAGE, PUSH_STAGE, NUMBER };
    using map_type = std::unordered_map<Key, std::variant<Values...>>;

#define WR_LOCKGUARD(level) std::lock_guard<std::shared_mutex> guard_##level(mutexs[level])
#define RD_LOCKGUARD(level) std::shared_lock<std::shared_mutex> guard_##level(mutexs[level])

    template <typename T = void>
    bool has(const Key &key)
    {
        RD_LOCKGUARD(WORKSPACE);
        return has<T>(WORKSPACE, key);
    }

    template <typename Value, typename Store = Value>
    int get(const Key &key, Value &value, bool try_pull = true)
    {
        if (try_pull) {
            std::set<Key> keys = {key};
            if (int err = pull(relevant_keys(key, keys)); err != 0) {
                TRACE("Error(%d): pull failed", err);
                return err;
            }
        }

        {
            RD_LOCKGUARD(PUSH_STAGE);
            if (get<Value, Store>(PUSH_STAGE, key, value) == 0) {
                return 0;
            }
        }

        {
            RD_LOCKGUARD(WORKSPACE);
            return get<Value, Store>(WORKSPACE, key, value);
        }
    }

    template <typename Value, typename Store = Value>
    Value get(const Key &key, const Value &defval, bool try_pull = true)
    {
        Value val;
        if (get<Value, Store>(key, val, try_pull) != 0) {
            val = defval;
        }
        return val;
    }

    template <typename Store = void, typename Value>
    int set(const Key &key, const Value &value, bool try_push = true)
    {
        {
            WR_LOCKGUARD(PUSH_STAGE);
            if constexpr (std::is_same_v<Store, void>) {
                if (int err = set<Value, Value>(PUSH_STAGE, key, value); err != 0) {
                    TRACE("Error(%d): set failed", err);
                    return err;
                }
            } else {
                if (int err = set<Store, Value>(PUSH_STAGE, key, value); err != 0) {
                    TRACE("Error(%d): set failed", err);
                    return err;
                }
            }
        }

        if (try_push) {
            std::set<Key> keys = {key};
            return push(relevant_keys(key, keys));
        }

        return 0;
    }

    virtual int push(const std::set<Key> &keys)
    {
        WR_LOCKGUARD(PUSH_STAGE);

        /* push to storage */
        {
            if (int err = sync_push(keys, maps[PUSH_STAGE]); err != 0) {
                TRACE("Error(%d): sync failed", err);
                return err;
            }
        }

        /* swap with stage */
        {
            WR_LOCKGUARD(WORKSPACE);
            for (auto &key : keys) {
                if (maps[PUSH_STAGE].count(key)) {
                    maps[WORKSPACE][key] = maps[PUSH_STAGE][key];
                    maps[PUSH_STAGE].erase(key);
                }
            }
        }

        return 0;
    }

    virtual int pull(const std::set<Key> &keys)
    {
        WR_LOCKGUARD(PULL_STAGE);

        /* pull from storage */
        {
            if (int err = sync_pull(keys, maps[PULL_STAGE]); err != 0) {
                TRACE("Error(%d): sync failed", err);
                return err;
            }
        }

        /* swap with stage */
        {
            WR_LOCKGUARD(WORKSPACE);
            for (auto &key : keys) {
                if (maps[PULL_STAGE].count(key)) {
                    maps[WORKSPACE][key] = maps[PULL_STAGE][key];
                    maps[PULL_STAGE].erase(key);
                }
            }
        }

        return 0;
    }

  protected:
    template <typename From, typename To>
    int derive(const From &from, To &to)
    {
        if constexpr (std::is_same_v<From, std::string> || std::is_same_v<To, std::string>) {
            return conv::derive(from, to);
        }

        return -ENOSYS;
    }

    virtual std::set<Key> &relevant_keys(const Key &, std::set<Key> &keys)
    {
        return keys;
    }

    virtual int sync_pull(const std::set<Key> &, map_type &)
    {
        return 0;
    }

    virtual int sync_push(const std::set<Key> &, const map_type &)
    {
        return 0;
    }

    template <typename Value, typename Store = Value>
    int get(int level, const Key &key, Value &value)
    {
        const auto &map = maps[level];
        if (map.count(key) > 0) {
            if (const Store *p = std::get_if<Store>(&map.at(key)); p != nullptr) {
                if constexpr (std::is_same_v<Value, Store>) {
                    value = *p;
                    return 0;
                } else {
                    return derive(*p, value);
                }
            } else {
                return -EINVAL;
            }
        }
        return -ENOENT;
    }

    template <typename Store, typename Value = Store>
    int set(int level, const Key &key, const Value &value)
    {
        auto &map = maps[level];
        if constexpr (std::is_same_v<Store, Value>) {
            map[key] = value;
        } else {
            Store to;
            if (int err = derive(value, to); err != 0) {
                TRACE("Error(%d): derive failed", err);
                return err;
            }
            map[key] = to;
        }

        return 0;
    }

    template <typename Value = void, typename Store = Value>
    bool has(int level, const Key &key)
    {
        const auto &map = maps[level];
        if constexpr (std::is_same_v<Value, void>) {
            return map.count(key) != 0;
        } else {
            if (map.count(key)) {
                if (const Store *p = std::get_if<Store>(map.at(key)); p != nullptr) {
                    if constexpr (std::is_same_v<Value, Store>) {
                        return true;
                    } else {
                        Value to;
                        return derive(*p, to) == 0;
                    }
                }
            }
            return false;
        }
    }

    map_type maps[level::NUMBER];
    std::shared_mutex mutexs[level::NUMBER];
};

template <typename Key = std::string>
class DistributedManager : public manager<Key, bool, int, uint32_t, uint64_t, std::string> {
  public:
    using map_type = std::unordered_map<Key, std::variant<bool, int, uint32_t, uint64_t, std::string>>;
    DistributedManager(std::string repo, std::string branch)
        : repo_(std::move(repo)), branch_(std::move(branch))
    {
    }

  protected:
    virtual int sync_pull(const std::set<Key> &, map_type &) override
    {
        return 0;
    }

    virtual int sync_push(const std::set<Key> &, const map_type &) override
    {
        return 0;
    }

  private:
    std::string repo_;
    std::string branch_;
};

template <typename Key = std::string>
class FileManager : public manager<Key, bool, int, uint32_t, uint64_t, std::string> {
  public:
    using map_type = std::unordered_map<Key, std::variant<bool, int, uint32_t, uint64_t, std::string>>;
    FileManager(std::string path) : path_(std::move(path)) {}

  protected:
    virtual int sync_pull(const std::set<Key> &, map_type &) override
    {
        return 0;
    }

    virtual int sync_push(const std::set<Key> &, const map_type &) override
    {
        return 0;
    }

  private:
    std::string path_;
};
} // namespace conf
