#include "tabulate.h"
using namespace tabulate;

int main()
{
    Table class_diagram;

    // Animal class
    {
        Table animal;
        animal.add("Animal");
        animal[0].format().align(Align::center);

        // Animal properties nested table
        {
            Table animal_properties;
            animal_properties.add("+age: Int");
            animal_properties.add("+gender: String");
            animal_properties.format().width(20);
            animal_properties[0].format().hide_border_bottom();

            animal.add(animal_properties);
        }

        // Animal methods nested table
        {
            Table animal_methods;
            animal_methods.add("+isMammal()");
            animal_methods.add("+mate()");
            animal_methods.format().width(20);
            animal_methods[0].format().hide_border_bottom();

            animal.add(animal_methods);
        }
        animal[1].format().hide_border_bottom();

        class_diagram.add(animal);
    }

    // Add rows in the class diagram for the up-facing arrow
    // THanks to center alignment, these will align just fine
    class_diagram.add("▲");
    class_diagram[1][0].format().hide_border_bottom().multi_bytes_character(true);
    class_diagram.add("|");
    class_diagram[2].format().hide_border_bottom();
    class_diagram.add("|");
    class_diagram[3].format().hide_border_bottom();

    // Duck class
    {
        Table duck;
        duck.add("Duck");

        // Duck proeperties nested table
        {
            Table duck_properties;
            duck_properties.add("+beakColor: String = \"yellow\"");
            duck_properties.format().width(40);

            duck.add(duck_properties);
        }

        // Duck methods nested table
        {
            Table duck_methods;
            duck_methods.add("+swim()");
            duck_methods.add("+quack()");
            duck_methods.format().width(40);
            duck_methods[0].format().hide_border_bottom();

            duck.add(duck_methods);
        }

        duck[0].format().align(Align::center);
        duck[1].format().hide_border_bottom();

        class_diagram.add(duck);
    }

    // Global styling
    class_diagram.format().styles(Style::bold).align(Align::center).width(60).hide_border();

    std::cout << class_diagram.xterm() << std::endl;
}
