project(libxmount_input_aaff)

add_library(xmount_input_aaff SHARED libxmount_input_aaff.c)
include_directories(${LIBZ_INCLUDE_DIRS})
target_link_libraries(xmount_input_aaff ${LIBZ_LIBRARIES})

install(TARGETS xmount_input_aaff DESTINATION lib/xmount)

