FIND_PROGRAM(DOXYGEN_EXECUTABLE
  NAMES doxygen
  PATHS
    /usr/local
    /usr
  PATH_SUFFIXES bin
)

SET(DOXYGEN_FOUND "NO")
IF(DOXYGEN_EXECUTABLE)
  SET(DOXYGEN_FOUND "YES")
ENDIF()
