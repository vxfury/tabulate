#include "tabulate.h"
using namespace tabulate;

int main()
{
    Table table;

    table.add("English", "I love you");
    table.add("French", "Je t’aime");
    table.add("Spanish", "Te amo");
    table.add("German", "Ich liebe Dich");
    table.add("Mandarin Chinese", "我爱你");
    table.add("Japanese", "愛してる");
    table.add("Korean", "사랑해 (Saranghae)");
    table.add("Greek", "Σ΄αγαπώ (Se agapo)");
    table.add("Italian", "Ti amo");
    table.add("Russian", "Я тебя люблю (Ya tebya liubliu)");
    table.add("Hebrew", "אני אוהב אותך (Ani ohev otakh)");

    // Column 1 is using mult-byte characters
    table.column(1).format().multi_bytes_character(true);
    table.format().corner("♥").styles(Style::bold).corner_color(Color::magenta).border_color(Color::magenta);

    std::cout << table.xterm() << std::endl;
}
