bin_PROGRAMS = darc2json
darc2json_CPPFLAGS = -std=c++11 -Wall -Wextra -Wstrict-overflow -Wshadow \
										$(MACPORTS_CF) \
										-Wuninitialized -pedantic $(RFLAGS)
darc2json_LDADD = $(MACPORTS_LD) -lc $(LIQUID) $(SNDFILE)
darc2json_SOURCES = darc2json.cc input.cc layer1.cc layer2.cc layer3.cc \
										util.cc liquid_wrappers.cc jsoncpp.cpp
