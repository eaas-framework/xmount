if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW) # CMake 3.0
endif(POLICY CMP0042)

project(libxmount_morphing_stripe C)

add_library(xmount_morphing_stripe SHARED libxmount_morphing_stripe.c)

install(TARGETS xmount_morphing_stripe DESTINATION lib/xmount)

