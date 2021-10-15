#include "tabulate.h"
using namespace tabulate;

int main()
{
    Table table;
    table.add("Bold", "Italic", "Bold & Italic", "Blinking");
    table.add("Underline", "Crossed", "faint", "Bold, Italic & Underlined");
    table.add("Doubly Underline", "Invisable", "", "");

    table[0][0].format().styles(Style::bold);

    table[0][1].format().styles(Style::italic);

    table[0][2].format().styles(Style::bold, Style::italic);

    table[0][3].format().styles(Style::blink);

    table[1][0].format().styles(Style::underline);

    table[1][1].format().styles(Style::crossed);

    table[1][2].format().styles(Style::faint);

    table[1][3].format().styles(Style::bold, Style::italic, Style::underline);

    table[2][0].format().styles(Style::doubly_underline);
    table[2][1].format().styles(Style::invisible);

    std::cout << table.xterm() << std::endl;
    // std::cout << "Markdown Table:\n" << table.markdown() << std::endl;
}
