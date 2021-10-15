#include "tabulate.h"
using namespace tabulate;

int main()
{
    Table readme;
    {
        Row &row = readme.add("tabulate for Modern C++");
        row.format().align(Align::center).color(Color::yellow);
    }

    {
        Row &row = readme.add("https://github.com/vxfury/tabulate");
        row.format().color(Color::white).align(Align::center).styles(Style::underline, Style::italic).hide_border_top();
    }

    {
        Row &row = readme.add("Tabulate is a header-only library for printing aligned, formatted, and "
                              "colorized tables in Modern C++");
        row.format().styles(Style::italic).color(Color::magenta);
    }

    {
        Table highlights;
        highlights.add("Header-only Library", "Requires C++11", "Apache License");
        Row &row = readme.add(highlights);
        row.format().align(Align::center).hide_border_top();
    }

    Table empty_row;
    empty_row.format().hide_border();

    {
        Row &row = readme.add(empty_row);
        row.format().hide_border_left().hide_border_right();
    }

    {
        Row &row = readme.add("Easily format and align content within cells");
        row.format().align(Align::center);

        row.format()
            .color(Color::cyan)
            .styles(Style::underline)
            .border_color(Color::green)
            .border_top_padding(0)
            .border_bottom_padding(0);
    }

    {
        Table format;
        {
            Row &row = format.add("Horizontal Alignment", "Left aligned", "Center aligned", "Right aligned");
            row.format().align(Align::center);
            row[0].format().color(Color::green); //.column_separator(":")
        }

        {
            Row &row =
                format.add("Word-Wrapping algorithm taking shamelessly from StackOverflow",
                           "Long sentences automatically word-wrap based on the width of the column",
                           "Word-wrapping also plays nicely with alignment rules. For instance, this cell is center "
                           "aligned.",
                           "Enforce \ncustom word-wrapping \nby embedding '\\n' \ncharacters in your cell\n content.");
            format.column(1).format().width(25).align(Align::left);
            format.column(2).format().width(25).align(Align::center);
            format.column(3).format().width(25).align(Align::right);

            row[0].format().align(Align::center);
            row[2].format().align(Align::center);
            row[3].format().align(Align::right);
        }

        format.column(0).format().width(23);
        format.column(1).format().border_left(":");

        Row &row = readme.add(format);
        row.format().hide_border_top().border_top_padding(0);
    }

    {
        Row &row = readme.add(empty_row);
        row.format().hide_border_left().hide_border_right();
    }

    {
        // clang-format off
        Table embedded_table(
            "You can even\n embed tables...",
            Table(
                "within tables",
                Table(
                    "within tables",
                    Table(
                        "within tables",
                        Table("within tables ...")
                    )
                )
            )
        );
        // clang-format on

        {
            Row &row = readme.add("Nested Representations");
            row.format().align(Align::center);
        }

        Row &row = readme.add(embedded_table);
        row.format().hide_border_top().border_color(Color::white).color(Color::yellow);
    }

    {
        Row &row = readme.add("ᚠ ᚡ ᚢ ᚣ ᚤ ᚥ ᚦ ᚧ ᚨ ᚩ ᚪ ᚫ ᚬ ᚭ ᚮ ᚯ ᚰ ᚱ ᚲ ᚳ ᚴ ᚵ ᚶ ᚷ ᚸ ᚹ ᚺ ᚻ ᚼ ᚽ ᚾ ᚿ ᛀ ᛁ ᛂ ᛃ ᛄ ᛅ ᛆ ᛇ "
                              "ᛈ ᛉ ᛊ ᛋ ᛌ ᛍ ᛎ ᛏ ᛐ ᛑ ᛒ ᛓ");
        row.format().background_color(Color::red).hide_border_top().multi_bytes_character(true);
    }

    // Print the table
    readme.format().border_color(Color::yellow).align(Align::center);
    std::cout << readme.xterm() << std::endl;

    {
        Table chart;

        // row 0 - 9
        for (size_t i = 0; i < 9; ++i) {
            std::vector<std::string> values;
            values.push_back(to_string(90 - i * 10));
            for (size_t j = 0; j <= 50; ++j) { values.push_back(" "); }

            chart.add_multiple(values);
        }

        // row 10
        {
            std::vector<std::string> values;
            for (int i = 0; i <= 12; ++i) {
                if ((i + 1) % 4 == 0) {
                    values.push_back(to_string(i + 1));
                } else {
                    values.push_back(" ");
                }
            }
            chart.add_multiple(values);
        }
        chart.add();
        chart.format().color(Color::white).border_left_padding(0).border_right_padding(0).hide_border();
        chart.column(0).format().border_left_padding(1).border_right_padding(1).border_left(" ");

        // chart.format().width(2).hide_border();
        for (size_t i = 0; i <= 18; ++i) { chart.column(i).format().width(2); }

        chart.column(2).format().border_color(Color::white).border_left("|").border_top("-");
        chart.column(2)[8].format().background_color(Color::red);
        chart.column(2)[7].format().background_color(Color::red);

        chart.column(3)[8].format().background_color(Color::yellow);
        chart.column(3)[7].format().background_color(Color::yellow);
        chart.column(3)[6].format().background_color(Color::yellow);

        chart.column(6)[8].format().background_color(Color::red);
        chart.column(6)[7].format().background_color(Color::red);
        chart.column(6)[6].format().background_color(Color::red);
        chart.column(6)[5].format().background_color(Color::red);

        chart.column(7)[8].format().background_color(Color::yellow);
        chart.column(7)[7].format().background_color(Color::yellow);
        chart.column(7)[6].format().background_color(Color::yellow);
        chart.column(7)[5].format().background_color(Color::yellow);
        chart.column(7)[4].format().background_color(Color::yellow);

        chart.column(10)[8].format().background_color(Color::red);
        chart.column(10)[7].format().background_color(Color::red);
        chart.column(10)[6].format().background_color(Color::red);
        chart.column(10)[5].format().background_color(Color::red);
        chart.column(10)[4].format().background_color(Color::red);
        chart.column(10)[3].format().background_color(Color::red);

        chart.column(11)[8].format().background_color(Color::yellow);
        chart.column(11)[7].format().background_color(Color::yellow);
        chart.column(11)[6].format().background_color(Color::yellow);
        chart.column(11)[5].format().background_color(Color::yellow);
        chart.column(11)[4].format().background_color(Color::yellow);
        chart.column(11)[3].format().background_color(Color::yellow);
        chart.column(11)[2].format().background_color(Color::yellow);
        chart.column(11)[1].format().background_color(Color::yellow);

        chart[2][15].format().background_color(Color::red);
        chart[2][16].set("Batch 1");

        chart.column(16).format().border_left_padding(1).width(20);

        chart[4][15].format().background_color(Color::yellow);
        chart[4][16].set("Batch 2");

        chart.column(17).format().width(50);

        chart[4][17].set("Cells, rows, and columns");
        chart[5][17].set("can be independently formatted.");
        chart[7][17].set("This cell is green and italic");
        chart[7][17].format().color(Color::green).styles(Style::italic);

        chart[8][17].set("This one's yellow and right-aligned");
        chart[8][17].format().color(Color::yellow).align(Align::right);

        chart[9][17].set("This one's on 🔥🔥🔥");

        chart.format().align(Align::center);
        chart.column(0).format().border_left_padding(1).border_right_padding(1).border_left(" ");

        std::cout << chart.xterm() << std::endl;

        {
            Table legend;
            legend.add("Batch 1", "10", "40", "50", "20", "10", "50");
            legend.add("Batch 2", "30", "60", "(70)", "50", "40", "30");

            legend[0].format().align(Align::center);
            legend[1].format().align(Align::center);

            legend.column(0).format().align(Align::right).color(Color::green).background_color(Color::black);

            legend.column(2).format().color(Color::white).background_color(Color::red);

            legend[1][3].format().styles(Style::italic).color(Color::yellow);

            std::cout << legend.xterm() << "\n\n";
        }
    }

    return 0;
}
