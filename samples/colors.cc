#include "tabulate.h"
using namespace tabulate;

int main()
{
    Table colors;

    colors.add("Font Color is Red", "Font Color is Blue", "Font Color is Green");
    colors.add("Everything is Red", "Everything is Blue", "Everything is Green");
    colors.add("Font Background is Red", "Font Background is Blue", "Font Background is Green");

    colors[0][0].format().color(Color::red).styles(Style::bold);
    colors[0][1].format().color(Color::blue).styles(Style::bold);
    colors[0][2].format().color(Color::green).styles(Style::bold);

    colors[1][0]
        .format()
        .border_left_color(Color::red)
        .border_left_background_color(Color::red)
        .background_color(Color::red)
        .color(Color::red)
        .border_right_color(Color::blue)
        .border_right_background_color(Color::blue);

    colors[1][1]
        .format()
        .background_color(Color::blue)
        .color(Color::blue)
        .border_right_color(Color::green)
        .border_right_background_color(Color::green);

    colors[1][2]
        .format()
        .background_color(Color::green)
        .color(Color::green)
        .border_right_color(Color::green)
        .border_right_background_color(Color::green);

    colors[2][0].format().background_color(Color::red).styles(Style::bold);
    colors[2][1].format().background_color(Color::blue).styles(Style::bold);
    colors[2][2].format().background_color(Color::green).styles(Style::bold);

    std::cout << colors.xterm() << std::endl;
    // std::cout << "Markdown Table:\n" << colors.markdown() << std::endl;
}
