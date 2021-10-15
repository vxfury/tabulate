#include "tabulate.h"
using namespace tabulate;

int main()
{
    Table table;

    table.add("This paragraph contains a veryveryveryveryveryverylong word. The long word will "
              "break and word wrap to the next line.",
              "This paragraph \nhas embedded '\\n' \ncharacters and\n will break\n exactly "
              "where\n you want it\n to\n break.");

    table[0][0].format().width(20);
    table[0][1].format().width(50);

    std::cout << table.xterm() << std::endl;
    // std::cout << "Markdown Table:\n" << table.markdown() << std::endl;
}
