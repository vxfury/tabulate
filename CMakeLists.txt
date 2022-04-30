CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
PROJECT("Table Maker for Modern C++")

INCLUDE(cmake/target_configure.cmake)

ENABLE_TESTING()
FILE(GLOB files samples/tabulate/*.cc)
FOREACH (file ${files})
  GET_FILENAME_COMPONENT(sample ${file} NAME_WE)
  ADD_EXECUTABLE(tabulate_${sample} ${file})
  TARGET_INCLUDE_DIRECTORIES(
    tabulate_${sample} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
  )
  ADD_TEST(NAME tabulate_${sample} COMMAND tabulate_${sample}
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )
  TARGET_CONFIGURE(tabulate_${sample})
ENDFOREACH ()
