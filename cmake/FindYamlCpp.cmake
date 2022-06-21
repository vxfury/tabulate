# Locate yaml-cpp
#
# This module defines
#   YAMLCPP_FOUND, if false, do not try to link to yaml-cpp
#   YAMLCPP_LIBRARY, where to find yaml-cpp
#   YAMLCPP_INCLUDE_DIR, where to find yaml.h
# There is also one IMPORTED library target,
#   yamlcpp::yamlcpp
#
# By default, the dynamic libraries of yaml-cpp will be found. To find the static ones instead,
# you must set the YAMLCPP_STATIC_LIBRARY variable to TRUE before calling find_package(YamlCpp ...).
#
# If yaml-cpp is not installed in a standard path, you can use the YAMLCPP_DIR CMake variable
# to tell CMake where yaml-cpp is.

IF (TARGET yamlcpp::yamlcpp)
  RETURN()
ENDIF ()

# attempt to find static library first if this is set
IF (YAMLCPP_STATIC_LIBRARY)
  SET(YAMLCPP_STATIC libyaml-cpp.a)
ENDIF ()

# find the yaml-cpp include directory
FIND_PATH(
  YAMLCPP_INCLUDE_DIR yaml-cpp/yaml.h
  PATH_SUFFIXES include
  PATHS ~/Library/Frameworks/yaml-cpp/include/
        /Library/Frameworks/yaml-cpp/include/
        /usr/local/include/
        /usr/include/
        /sw/yaml-cpp/ # Fink
        /opt/local/yaml-cpp/ # DarwinPorts
        /opt/csw/yaml-cpp/ # Blastwave
        /opt/yaml-cpp/
        ${YAMLCPP_DIR}/include/
)

# find the yaml-cpp library
FIND_LIBRARY(
  YAMLCPP_LIBRARY
  NAMES ${YAMLCPP_STATIC} yaml-cpp
  PATH_SUFFIXES lib64 lib
  PATHS ~/Library/Frameworks
        /Library/Frameworks
        /usr/local
        /usr
        /sw
        /opt/local
        /opt/csw
        /opt
        ${YAMLCPP_DIR}/lib
)

# handle the QUIETLY and REQUIRED arguments and set YAMLCPP_FOUND to TRUE if all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
  YAMLCPP DEFAULT_MSG YAMLCPP_INCLUDE_DIR YAMLCPP_LIBRARY
)
MARK_AS_ADVANCED(YAMLCPP_INCLUDE_DIR YAMLCPP_LIBRARY)

# Add an imported target
IF (YAMLCPP_LIBRARY)
  ADD_LIBRARY(yamlcpp::yamlcpp UNKNOWN IMPORTED)
  SET_PROPERTY(
    TARGET yamlcpp::yamlcpp PROPERTY IMPORTED_LOCATION ${YAMLCPP_LIBRARY}
  )
  IF (YAMLCPP_INCLUDE_DIR)
    SET_PROPERTY(
      TARGET yamlcpp::yamlcpp PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                                       ${YAMLCPP_INCLUDE_DIR}
    )
  ENDIF ()
ENDIF ()
