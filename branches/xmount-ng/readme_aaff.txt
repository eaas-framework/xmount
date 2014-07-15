The aaff module may be compiled separately in order to get a standalone program for
testing the module. Compile, for example, withe helpof Qt/qmake and the following
.pro file:

TEMPLATE = app
CONFIG += console
CONFIG -= qt

QMAKE_CFLAGS += -std=c99
QMAKE_CFLAGS += -D_GNU_SOURCE
QMAKE_CFLAGS += -DAAFF_MAIN_FOR_TESTING


SOURCES += aaff.c

LIBS+= -lz
