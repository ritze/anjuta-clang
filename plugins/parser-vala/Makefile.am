if ENABLE_VALA

# Plugin description file
plugin_in_files = anjuta-parser-vala.plugin.in
%.plugin: %.plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

# Where to install the plugin
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
	$(WARN_CFLAGS) \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DANJUTA_DATA_DIR=\"$(anjuta_data_dir)\" \
	-DANJUTA_PLUGIN_DIR=\"$(anjuta_plugin_dir)\" \
	-DANJUTA_IMAGE_DIR=\"$(anjuta_image_dir)\" \
	-DANJUTA_GLADE_DIR=\"$(anjuta_glade_dir)\" \
	-DANJUTA_UI_DIR=\"$(anjuta_ui_dir)\" \
	$(LIBANJUTA_CFLAGS) \
	$(VALA_CFLAGS) \
	-DPACKAGE_SRC_DIR=\"$(srcdir)\" \
	-DG_LOG_DOMAIN=\"parser-vala\" \
	--no-warn

# The plugin
plugindir = $(anjuta_plugin_dir)
plugin_LTLIBRARIES = libanjuta-parser-vala.la

# Plugin sources
libanjuta_parser_vala_la_VALASOURCES = plugin.vala provider.vala
nodist_parser_language_vala_la_SOURCES = $(libanjuta_parser_vala_la_VALASOURCES:.vala=.c)
CLEANFILES = $(nodist_libanjuta_language_vala_la_SOURCES) libanjuta_parser_vala_la_vala.stamp

plugin.c provider.c: libanjuta_parser_vala_la_vala.stamp
	@true

libanjuta_parser_vala_la_vala.stamp: $(libanjuta_parser_vala_la_VALASOURCES)
	valac $(VALAFLAGS) --vapidir $(srcdir) --pkg $(LIBVALA) --pkg libanjuta-3.0 -C $^
	touch $@

libanjuta_parser_vala_la_LDFLAGS = $(ANJUTA_PLUGIN_LDFLAGS)

# Plugin dependencies
libanjuta_parser_vala_la_LIBADD = \
	$(LIBANJUTA_LIBS) \
	$(VALA_LIBS)

endif # ENABLE_VALA

EXTRA_DIST = \
	$(plugin_in_files) \
	$(parser_vala_plugin_DATA) \
	$(libanjuta_parser_vala_la_VALASOURCES) \
	libanjuta-3.0.deps \
	libanjuta-3.0.vapi


-include $(top_srcdir)/git.mk

