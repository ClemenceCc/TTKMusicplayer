include($$PWD/../../plugins.pri)

HEADERS += decodersidfactory.h \
           decoder_sid.h \
           sidhelper.h
    
SOURCES += decodersidfactory.cpp \
           decoder_sid.cpp \
           sidhelper.cpp

DESTDIR = $$PLUGINS_PREFIX/Input
TARGET = sid

# Disbale gcc warnings
QMAKE_CFLAGS += -w
QMAKE_CXXFLAGS += -w

INCLUDEPATH += $$EXTRA_PREFIX/libsidplayfp/include

unix {
    QMAKE_CLEAN = $$DESTDIR/$$DESTDIR/libsid.so
    LIBS += -L$$EXTRA_PREFIX/libsidplayfp/lib -lsidplayfp$$STATIC_LIBRARY_SUFFIX
}

win32 {
    LIBS += -L$$EXTRA_PREFIX/libsidplayfp/lib -lsidplayfp
}