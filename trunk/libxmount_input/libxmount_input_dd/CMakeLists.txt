project(libxmount_input_dd)

add_library(xmount_input_dd SHARED libxmount_input_dd.c)

install(TARGETS xmount_input_dd DESTINATION lib/xmount)

