#include "tabulate.h"
using namespace tabulate;

int main()
{
    Table universal_constants;

    universal_constants.add("Quantity", "Value");
    universal_constants.add("Characteristic impedance of vacuum", "376.730 313 461... Ω");
    universal_constants.add("Electric constant (permittivity of free space)", "8.854 187 817... × 10⁻¹²F·m⁻¹");
    universal_constants.add("Magnetic constant (permeability of free space)",
                            "4π × 10⁻⁷ N·A⁻² = 1.2566 370 614... × 10⁻⁶ N·A⁻²");
    universal_constants.add("Gravitational constant (Newtonian constant of gravitation)",
                            "6.6742(10) × 10⁻¹¹m³·kg⁻¹·s⁻²");
    universal_constants.add("Planck's constant", "6.626 0693(11) × 10⁻³⁴ J·s");
    universal_constants.add("Dirac's constant", "1.054 571 68(18) × 10⁻³⁴ J·s");
    universal_constants.add("Speed of light in vacuum", "299 792 458 m·s⁻¹");

    universal_constants.format()
        .styles(Style::bold)
        .border_top(" ")
        .border_bottom(" ")
        .border_left(" ")
        .border_right(" ")
        .corner(" ");

    universal_constants[0]
        .format()
        .border_top_padding(1)
        .border_bottom_padding(1)
        .align(Align::center)
        .styles(Style::underline)
        .background_color(Color::red);

    universal_constants.column(1).format().color(Color::yellow);

    universal_constants[0][1].format().background_color(Color::blue).color(Color::white);

    std::cout << universal_constants.xterm() << std::endl;
    // std::cout << "Markdown Table:\n" << universal_constants.markdown() << std::endl;
}
