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

#include "cxxopt.h"
#include <iostream>

int main(int argc, char **argv)
{
    {
        cxxopt::Options options("test", "test demo");

        // clang-format off
        options.add_group()
        ("h,help", "display help and exit")
        ("s", "short option")
        ("v,verbose", "set verbose", cxxopt::Value<int>()->set_implicit(3))
        ("F", "force set\nanother line\nanother line", cxxopt::Value<bool>()->set_default(false)->set_implicit(true), cxxopt::NOARG)
        ("input", "input file", cxxopt::Value<std::string>()->set_default("  /path/to/file "), cxxopt::REQUIRED)
        ("array", "set array", cxxopt::Value<std::vector<int>>()->set_default("[ 1,2 , 3 ]"), cxxopt::REQUIRED)
        ("matrix", "set matrix", cxxopt::Value<std::vector<std::vector<int>>>()->set_default("[[ 1,2 , 3 ], [4,5,6]]"), cxxopt::REQUIRED)
        ("map", "set map", cxxopt::Value<std::unordered_map<std::string, std::vector<std::string>>>()->set_default("key1  : [val1 ,val2 ] ; key2:[val3, val4];key3:val5 :sss, val6, \"  val7 \""), cxxopt::REQUIRED);
        // clang-format on

        auto results = options.parse(argc, argv);
        if (results.has('h')) {
            std::cout << "LINE(" << __LINE__ << "): " << options.usage() << std::endl;
            exit(0);
        }
        std::cout << "LINE(" << __LINE__ << "):\n" << results.description(5) << std::endl;

        {
            std::cout << "array: " << results["array"].format<std::vector<int>>() << std::endl
                      << std::string(strlen("array: "), ' ');
            for (auto item : results["array"].get<std::vector<int>>()) {
                std::cout << item << ", ";
            }
            std::cout << std::endl;
        }

        for (int i = 0; i < argc; i++) {
            std::cout << "LINE(" << __LINE__ << "): "
                      << "Unkown: " << argv[i] << std::endl;
        }
    }

    // test values
    {
        {
            auto v = cxxopt::Value<int>()
                         ->set("10", 0)
                         ->set("100", 1)
                         ->set("1000", 2)
                         ->set(10000, 1)
                         ->set(100000, 10)
                         ->set(10000000, 7)
                         ->clear();
            v->clear(3)->clear(1);
            std::cout << "LINE(" << __LINE__ << "): " << v->description() << std::endl;
            std::cout << "LINE(" << __LINE__ << "): " << v->clear_all()->description<int>() << std::endl;
        }

        {
            auto v = cxxopt::Value<std::vector<int>>()->set("10,100,1000", 0)->set("100, 1000, 10000,100000", 1);
            std::cout << "LINE(" << __LINE__ << "): " << v->description<std::vector<int>>() << std::endl;
        }

        {
            auto v = cxxopt::Value<std::vector<std::vector<int>>>()
                         ->set("[[1,10,100], [10, 100, 1000], [105, 1005, 10005]]", 0)
                         ->set("[[100, 1000, 10000,100000]]", 1);
            std::cout << "LINE(" << __LINE__ << "): " << v->description<std::vector<std::vector<int>>>() << std::endl;
        }

        {
            auto v = cxxopt::Value<std::vector<std::vector<std::vector<int>>>>()
                         ->set("[[[1,10,100], [10, 100, 1000]], [[105, 1005, 10005]]]", 0)
                         ->set("[[[100, 1000, 10000,100000]]]", 1);
            std::cout << "LINE(" << __LINE__ << "): " << v->description<std::vector<std::vector<std::vector<int>>>>()
                      << std::endl;
        }

        {
            auto v = cxxopt::Value<std::unordered_map<std::string, std::vector<std::string>>>()
                         ->set_default("key1:val1,val2;key2:val3;key3:val4,val5")
                         ->set_implicit("key1:[val1,val3];key2:val4,val5");

            std::cout << "LINE(" << __LINE__
                      << "): " << v->description<std::unordered_map<std::string, std::vector<std::string>>>()
                      << std::endl;
        }
    }

    return 0;
}
