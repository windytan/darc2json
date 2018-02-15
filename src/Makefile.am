bin_PROGRAMS = darcdec
darcdec_CPPFLAGS = -std=c++11 -Wall -Wextra -Wstrict-overflow -Wshadow \
										$(MACPORTS_CF) \
										-Wuninitialized -pedantic $(RFLAGS)
darcdec_LDADD = $(MACPORTS_LD) -lc $(LIQUID) $(SNDFILE)
darcdec_SOURCES = darcdec.cc input.cc layer1.cc layer2.cc liquid_wrappers.cc
