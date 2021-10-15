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
    for (auto &cell : table[0]) { cell.format().styles(Style::underline).align(Align::center); }

    // Iterator over cells in the first column
    for (auto &cell : table.column(0)) {
        if (cell.get() != "Company") { cell.format().align(Align::right); }
    }

    // Iterate over rows in the table
    size_t index = 0;
    for (auto &row : table) {
        // row.format().styles(Style::bold);

        // Set blue background color for alternate rows
        if (index > 0 && index % 2 == 0) {
            for (auto &cell : row) { cell.format().background_color(Color::blue); }
        }
        index += 1;
    }

    std::cout << table.xterm() << std::endl;
    // std::cout << "Markdown Table:\n" << table.markdown() << std::endl;
}
