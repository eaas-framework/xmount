project(libxmount_input_aewf)

add_library(xmount_input_aewf SHARED libxmount_input_aewf.c)
include_directories(${LIBZ_INCLUDE_DIRS})
target_link_libraries(xmount_input_aewf ${LIBZ_LIBRARIES})

install(TARGETS xmount_input_aewf DESTINATION lib/xmount)

