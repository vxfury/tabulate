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
using namespace tabulate;

int main()
{
    Table table;

    table.add("Company", "Contact", "Country");
    table.add("Alfreds Futterkiste", "Maria Anders", "Germany");
    table.add("Centro comercial Moctezuma", "Francisco Chang", "Mexico");
    table.add("Ernst Handel", "Roland Mendel", "Austria");
    table.add("Island Trading", "Helen Bennett", "UK");
    table.add("Laughing Bacchus Winecellars", "Yoshi Tannamuri", "Canada");
    table.add("Magazzini Alimentari Riuniti", "Giovanni Rovelli", "Italy");

    // Set width of cells in each column
    table.column(0).format().width(40);
    table.column(1).format().width(30);
    table.column(2).format().width(30);

    // Iterate over cells in the first row
    for (auto &cell : table[0]) {
        cell.format().styles(Style::underline).align(Align::center);
    }

    // Iterator over cells in the first column
    for (auto &cell : table.column(0)) {
        if (cell.get() != "Company") {
            cell.format().align(Align::right);
        }
    }

    // Iterate over rows in the table
    size_t index = 0;
    for (auto &row : table) {
        // row.format().styles(Style::bold);

        // Set blue background color for alternate rows
        if (index > 0 && index % 2 == 0) {
            for (auto &cell : row) {
                cell.format().background_color(Color::blue);
            }
        }
        index += 1;
    }

    std::cout << table.xterm() << std::endl;
    // std::cout << "Markdown Table:\n" << table.markdown() << std::endl;
}
