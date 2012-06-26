# Plugin Icon file
parser_engine_pixmapsdir = $(anjuta_image_dir)
parser_engine_pixmaps_DATA = \
  anjuta-parser-engine-plugin.png

# Plugin description file
plugin_in_files = anjuta-parser-engine.plugin.in
%.plugin: %.plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

parser_engine_plugindir = $(anjuta_plugin_dir)
parser_engine_plugin_DATA = $(plugin_in_files:.plugin.in=.plugin)

# NOTE :
# The naming convention is very intentional
# We are forced to use the prefix 'lib' by automake and libtool
#    There is probably a way to avoid it but it is not worth to effort
#    to find out.
# The 'anjuta_' prfix is a safety measure to avoid conflicts where the
#    plugin 'libpython.so' needs to link with the real 'libpython.so'

# Include paths
AM_CPPFLAGS = \
	$(LIBANJUTA_CFLAGS) 

# Where to install the plugin
plugindir = $(anjuta_plugin_dir)

# The plugin
plugin_LTLIBRARIES = libanjuta-parser-engine.la

# Plugin sources
libanjuta_parser_engine_la_SOURCES = \
	plugin.h \
	plugin.c \
	utils.h \
	utils.c

libanjuta_parser_engine_la_LDFLAGS = \
	$(ANJUTA_PLUGIN_LDFLAGS)

# Plugin dependencies
libanjuta_parser_engine_la_LIBADD = \
	$(LIBANJUTA_LIBS)

EXTRA_DIST = \
  $(plugin_in_files) \
  $(parser_engine_plugin_DATA) \
  $(parser_engine_pixmaps_DATA) 

-include $(top_srcdir)/git.mk
