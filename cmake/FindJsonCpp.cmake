# Locate jsoncpp
#
# This module defines
#   JSONCPP_FOUND, if false, do not try to link to jsoncpp
#   JSONCPP_LIBRARY, where to find jsoncpp
#   JSONCPP_INCLUDE_DIR, where to find yaml.h
# There is also one IMPORTED library target,
#   jsoncpp::jsoncpp
#
# By default, the dynamic libraries of jsoncpp will be found. To find the static ones instead,
# you must set the JSONCPP_STATIC_LIBRARY variable to TRUE before calling find_package(YamlCpp ...).
#
# If jsoncpp is not installed in a standard path, you can use the JSONCPP_DIR CMake variable
# to tell CMake where jsoncpp is.

IF (TARGET jsoncpp::jsoncpp)
  RETURN()
ENDIF ()

# attempt to find static library first if this is set
IF (JSONCPP_STATIC_LIBRARY)
  SET(JSONCPP_STATIC libjsoncpp.a)
ENDIF ()

# find the jsoncpp include directory
FIND_PATH(
  JSONCPP_INCLUDE_DIR jsoncpp/json/json.h json/json.h
  PATH_SUFFIXES include
  PATHS ~/Library/Frameworks/jsoncpp/include/
        /Library/Frameworks/jsoncpp/include/
        /usr/local/include/
        /usr/include/
        /sw/jsoncpp/ # Fink
        /opt/local/jsoncpp/ # DarwinPorts
        /opt/csw/jsoncpp/ # Blastwave
        /opt/jsoncpp/
        ${JSONCPP_DIR}/include/
)

# find the jsoncpp library
FIND_LIBRARY(
  JSONCPP_LIBRARY
  NAMES ${JSONCPP_STATIC} jsoncpp
  PATH_SUFFIXES lib64 lib
  PATHS ~/Library/Frameworks
        /Library/Frameworks
        /usr/local
        /usr
        /sw
        /opt/local
        /opt/csw
        /opt
        ${JSONCPP_DIR}/lib
)

# handle the QUIETLY and REQUIRED arguments and set JSONCPP_FOUND to TRUE if all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
  JSONCPP DEFAULT_MSG JSONCPP_INCLUDE_DIR JSONCPP_LIBRARY
)
MARK_AS_ADVANCED(JSONCPP_INCLUDE_DIR JSONCPP_LIBRARY)

# Add an imported target
IF (JSONCPP_LIBRARY)
  ADD_LIBRARY(jsoncpp::jsoncpp UNKNOWN IMPORTED)
  SET_PROPERTY(
    TARGET jsoncpp::jsoncpp PROPERTY IMPORTED_LOCATION ${JSONCPP_LIBRARY}
  )
  IF (JSONCPP_INCLUDE_DIR)
    SET_PROPERTY(
      TARGET jsoncpp::jsoncpp PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                                       ${JSONCPP_INCLUDE_DIR}
    )
  ENDIF ()
ENDIF ()
