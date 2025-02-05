set(SOURCES
    "src/local_course.cpp"
    "src/interface.cpp"
    "src/matlab_kalman.cpp"
    "src/ekf.cpp"
)

set(HEADERS
    "include/local_course/local_course.h"
    "include/local_course/matlab_kalman.h"
    "include/local_course/ekf.h"
)
include_directories(
        include
        /usr/local/include
        /usr/include
        /usr/local/include/eigen3
        /usr/include/eigen3
        /usr/include/qt5

)

add_definitions(-DEIGEN_NO_DEBUG)

#include_directories(include)

set(KALMAN "Matlab/codegen/lib/kalman_filter_lr")
include_directories("include")
include_directories("${KALMAN}")
file(GLOB kalman "${KALMAN}/*.cpp" "${KALMAN}/*.h")
add_library(local_course SHARED ${SOURCES} ${HEADERS} ${kalman})

target_link_libraries(local_course PRIVATE lmscore street_environment)
