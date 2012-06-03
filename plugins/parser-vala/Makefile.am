# Plugin glade file
parser_vala_gladedir = $(anjuta_glade_dir)
parser_vala_glade_DATA = anjuta-parser-vala.ui

# Plugin Icon file
parser_vala_pixmapsdir = $(anjuta_image_dir)
parser_vala_pixmaps_DATA = \
  anjuta-parser-vala-plugin.png

# Plugin description file
plugin_in_files = anjuta-parser-vala.plugin.in
%.plugin: %.plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

parser_vala_plugindir = $(anjuta_plugin_dir)
parser_vala_plugin_DATA = $(plugin_in_files:.plugin.in=.plugin)

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
	-DG_LOG_DOMAIN=\"parser-vala\"

# Where to install the plugin
plugindir = $(anjuta_plugin_dir)

# The plugin
plugin_LTLIBRARIES = libanjuta-parser-vala.la

# Plugin sources
libanjuta_parser_vala_la_SOURCES = \
	plugin.c \
	plugin.h

libanjuta_parser_vala_la_LDFLAGS = $(ANJUTA_PLUGIN_LDFLAGS)

# Plugin dependencies
libanjuta_parser_vala_la_LIBADD = $(LIBANJUTA_LIBS) \
	$(GTHREAD_LIBS)

gsettings_in_file = org.gnome.anjuta.plugins.parser-vala.gschema.xml.in
gsettings_SCHEMAS = $(gsettings_in_file:.xml.in=.xml)
@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

EXTRA_DIST = \
  $(plugin_in_files) \
  $(parser_vala_plugin_DATA) \
  $(parser_vala_pixmaps_DATA) \
	$(parser_vala_glade_DATA) \
	$(gsettings_in_file)

CLEANFILES = \
  $(gsettings_SCHEMAS)

-include $(top_srcdir)/git.mk
