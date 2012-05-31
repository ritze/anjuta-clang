# Plugin Icon file
parser_cxx_pixmapsdir = $(anjuta_image_dir)
parser_cxx_pixmaps_DATA = \
  anjuta-parser-cxx-plugin.png

# Plugin description file
plugin_in_files = anjuta-parser-cxx.plugin.in
%.plugin: %.plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

parser_cxx_plugindir = $(anjuta_plugin_dir)
parser_cxx_plugin_DATA = $(plugin_in_files:.plugin.in=.plugin)

# NOTE :
# The naming convention is very intentional
# We are forced to use the prefix 'lib' by automake and libtool
#    There is probably a way to avoid it but it is not worth to effort
#    to find out.
# The 'anjuta_' prfix is a safety measure to avoid conflicts where the
#    plugin 'libpython.so' needs to link with the real 'libpython.so'

# Include paths
AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
  $(WARN_CFLAGS) \
	$(DEPRECATED_FLAGS) \
	$(LIBANJUTA_CFLAGS) \
	$(EXTRA_CFLAGS) \
	-DG_LOG_DOMAIN=\"parser-cxx\"

# Where to install the plugin
plugindir = $(anjuta_plugin_dir)

# The plugin
plugin_LTLIBRARIES = libanjuta-parser-cxx.la

# Plugin sources
libanjuta_parser_cxx_la_SOURCES = \
	plugin.c \
	plugin.h \
	expression-lexer.cpp \
	expression-parser.cpp \
	expression-result.cpp \
	expression-result.h \
	flex-lexer-klass.cpp \
	flex-lexer-klass.h \
	flex-lexer-klass-tab.h \
	flex-lexer-plain.cpp \
	flex-lexer-plain-tab.h \
	variable-parser.cpp \
	variable-result.cpp \
	variable-result.h \
	scope-parser.cpp \
	function-result.cpp \
	function-result.h \
	function-parser.cpp \
	cpp-flex-tokenizer.cpp \
	cpp-flex-tokenizer.h \
	expression-parser.h \
	engine-parser.cpp \
	engine-parser-priv.h \
	engine-parser.h \
	scope-parser.h \
	variable-parser.h \
	function-parser.h

libanjuta_parser_cxx_la_LDFLAGS = $(ANJUTA_PLUGIN_LDFLAGS)

# Use C++ linking
nodist_EXTRA_libanjuta_language_cpp_java_la_SOURCES = dummy.cxx

# Plugin dependencies
libanjuta_parser_cxx_la_LIBADD = $(LIBANJUTA_LIBS) \
	$(GTHREAD_LIBS)

EXTRA_DIST = \
  $(plugin_in_files) \
  $(cpp_java_plugin_DATA) \
  $(cpp_java_pixmaps_DATA) 

-include $(top_srcdir)/git.mk
