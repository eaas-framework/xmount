if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW) # CMake 3.0
endif(POLICY CMP0042)

project(libxmount_input_aff C)

add_library(xmount_input_aff SHARED libxmount_input_aff.c)

if(NOT STATIC_AFF)
  include_directories(${LIBAFF_INCLUDE_DIRS})
  include_directories(${OPENSSL_INCLUDE_DIRS})
  set(LIBS ${LIBS} ${LIBAFF_LIBRARIES})
  set(LIBS ${LIBS} ${OPENSSL_LIBRARIES})
else(NOT STATIC_AFF)
  add_definitions(-DHAVE_LIBAFF_STATIC)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libaff/lib)
  set(LIBS ${LIBS} ${CMAKE_CURRENT_SOURCE_DIR}/libaff/lib/.libs/libafflib.a)
  if(NOT APPLE)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      # 64bit
      set(LIBS ${LIBS} "/usr/lib/x86_64-linux-gnu/libcrypto.a")
      set(LIBS ${LIBS} "/usr/lib/x86_64-linux-gnu/libssl.a")
      set(LIBS ${LIBS} "/usr/lib/x86_64-linux-gnu/libexpat.a")
      set(LIBS ${LIBS} "/usr/lib/x86_64-linux-gnu/librt.a")
    else(CMAKE_SIZEOF_VOID_P EQUAL 8)
      # 32bit
      set(LIBS ${LIBS} "/usr/lib/i386-linux-gnu/libcrypto.a")
      set(LIBS ${LIBS} "/usr/lib/i386-linux-gnu/libssl.a")
      set(LIBS ${LIBS} "/usr/lib/i386-linux-gnu/libexpat.a")
      set(LIBS ${LIBS} "/usr/lib/i386-linux-gnu/librt.a")
    endif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(LIBS ${LIBS} "-lstdc++ -lz")
  else(NOT APPLE)
    # As most of afflib's deps on OSx come from MacPorts, better link in
    # everything statically
    set(LIBS ${LIBS} "/opt/local/lib/libcrypto.a")
    set(LIBS ${LIBS} "/opt/local/lib/libssl.a")
    set(LIBS ${LIBS} "/opt/local/lib/libexpat.a")
    set(LIBS ${LIBS} "/opt/local/lib/libncurses.a")
    set(LIBS ${LIBS} "/opt/local/lib/libreadline.a")
    set(LIBS ${LIBS} "/opt/local/lib/libz.a")
    set(LIBS ${LIBS} "-lstdc++")
  endif(NOT APPLE)
endif(NOT STATIC_AFF)

target_link_libraries(xmount_input_aff ${LIBS})

install(TARGETS xmount_input_aff DESTINATION lib/xmount)

