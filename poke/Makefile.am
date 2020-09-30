ACLOCAL_AMFLAGS = -I m4 -I m4/libpoke -I m4/gui
SUBDIRS = jitter gl maps pickles gl-libpoke gl-gui libpoke poke gui doc \
          man testsuite etc po

noinst_SCRIPTS = run

# This (and the accompanying AC_SUBST of WITH_JITTER) is to be removed
# as soon as we bump to a Jitter version not requiring --with-jitter
# if it is installed in a standard location.
AM_DISTCHECK_CONFIGURE_FLAGS = --with-jitter=$(WITH_JITTER) \
                               --enable-debug=$(ENABLE_DEBUG)

# Support for generating a cyclomatic complexity report using the
# pmccabe tool.

CYCLO_SRCS = ${top_srcdir}/poke/*.[ch] $(top_srcdir)/libpoke/*.[ch]

cyclo-$(PACKAGE).html: $(CYCLO_SRCS)
	$(PMCCABE) $(CYCLO_SRCS) \
		| sort -nr \
		| $(AWK) -f ${top_srcdir}/build-aux/pmccabe2html \
			-v lang=html -v name="$(PACKAGE_NAME)" \
			-v vcurl="https://git.savannah.gnu.org/gitweb/?p=$(PACKAGE).git;a=blob;f=%FILENAME%;hb=HEAD" \
			-v url="https://www.jemarch.net/poke" \
			-v css=${top_srcdir}/build-aux/pmccabe.css \
			-v cut_dir=${top_srcdir}/ \
			> $@-tmp
	mv $@-tmp $@
