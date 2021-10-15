#include "tabulate.h"
using namespace tabulate;

int main()
{
    tabulate::Table movies;

    movies.add("S/N", "Movie Name", "Director", "Estimated Budget", "Release Date");
    movies.add("tt1979376", "Toy Story 4", "Josh Cooley", 200000000, "21 June 2019");
    movies.add("tt3263904", "Sully", "Clint Eastwood", 60000000, "9 September 2016");
    movies.add("tt1535109", "Captain Phillips", "Paul Greengrass", 55000000, " 11 October 2013");

    // center align 'Director' column
    movies.column(2).format().align(Align::center);

    // right align 'Estimated Budget' column
    movies.column(3).format().align(Align::right);

    // right align 'Release Date' column
    movies.column(4).format().align(Align::right);

    // Color header cells
    for (size_t i = 0; i < movies.column_size(); ++i) {
        movies[0][i].format().color(Color::yellow).styles(Style::bold);
    }

    std::cout << movies.xterm() << std::endl;
    std::cout << "Markdown Table:\n" << movies.markdown() << std::endl;
}
