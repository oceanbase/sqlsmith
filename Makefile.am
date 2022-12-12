-include Makefile.local

BUILT_SOURCES = gitrev.h

bin_PROGRAMS = sqlsmith

DUT = postgres.cc

if DUT_MONETDB
DUT += monetdb.cc
endif

if DUT_SQLITE
DUT += sqlite.cc
endif

DUT += mysql.cc

sqlsmith_SOURCES = relmodel.cc schema.cc $(DUT)	 			\
    random.cc prod.cc expr.cc grammar.cc log.cc dump.cc impedance.cc	\
    sqlsmith.cc

sqlsmith_LDADD = $(LIBPQXX_LIBS) $(MONETDB_MAPI_LIBS) $(BOOST_REGEX_LIB) $(POSTGRESQL_LIBS)

AM_LDFLAGS = $(BOOST_LDFLAGS) $(POSTGRESQL_LDFLAGS)
AM_CPPFLAGS = $(BOOST_CPPFLAGS) $(LIBPQXX_CFLAGS) $(POSTGRESQL_CPPFLAGS) $(MONETDB_MAPI_CFLAGS) -Wall -Wextra


EXTRA_DIST = gitrev.h dump.hh expr.hh grammar.hh log.hh prod.hh		\
    random.hh relmodel.hh schema.hh impedance.hh known.txt known_re.txt log.sql	\
    README.org TODO.org ast.png logo.png dump.xsl util.hh sqlite.hh	\
    dut.hh postgres.hh monetdb.hh mysql.hh log-v1.0-to-v1.2.sql boring_sqlstates.txt

gitrev.h: $(HEADERS) $(SOURCES)
	-if git describe --exclude='debian*' --dirty --tags --always > /dev/null ; then \
	echo "#define GITREV \"$$(git describe --exclude='debian*' --dirty --tags --always)\"" > $@ ;\
	fi

filterdump:
	psql -c 'copy (select error from known) to stdout' smith|sort -u > known.txt
	psql -c 'copy (select re from known_re) to stdout' smith|sort -u > known_re.txt

.PHONY: filterdump
