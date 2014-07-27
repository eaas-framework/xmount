project(libxmount_input_ewf)

add_library(xmount_input_ewf SHARED libxmount_input_ewf.c)
include_directories(${LIBEWF_INCLUDE_DIRS})
set(LIBS ${LIBS} ${LIBEWF_LIBRARIES})
target_link_libraries(xmount_input_ewf ${LIBS})

install(TARGETS xmount_input_ewf DESTINATION lib/xmount)

