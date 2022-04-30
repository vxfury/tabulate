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

#include "tabulate.h"

int main()
{
    tabulate::Table movies;

    movies.add("S/N", "Movie Name", "Director", "Estimated Budget", "Release Date");
    movies.add("tt1979376", "Toy Story 4", "Josh Cooley", 200000000, "21 June 2019");
    movies.add("tt3263904", "Sully", "Clint Eastwood", 60000000, "9 September 2016");
    movies.add("tt1535109", "Captain Phillips", "Paul Greengrass", 55000000, " 11 October 2013");

    // center align 'Director' column
    movies.column(2).format().align(tabulate::Align::center);

    // right align 'Estimated Budget' column
    movies.column(3).format().align(tabulate::Align::right);

    // right align 'Release Date' column
    movies.column(4).format().align(tabulate::Align::right);

    // Color header cells
    for (size_t i = 0; i < movies.column_size(); ++i) {
        movies[0][i].format().color(tabulate::Color::yellow).styles(tabulate::Style::bold);
    }

    std::cout << movies.xterm() << std::endl;
    std::cout << "Markdown Table:\n" << movies.markdown() << std::endl;
}
