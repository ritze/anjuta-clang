# Plugin glade file
code_analyzer_uidir = $(anjuta_glade_dir)
code_analyzer_ui_DATA = code-analyzer.ui

# Plugin Icon file
code_analyzer_pixmapsdir = $(anjuta_image_dir)
code_analyzer_pixmaps_DATA = \
	code-analyzer.png
# Plugin description file
plugin_in_files = code-analyzer.plugin.in
%.plugin: %.plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

code_analyzer_plugindir = $(anjuta_plugin_dir)
code_analyzer_plugin_DATA = $(plugin_in_files:.plugin.in=.plugin)

AM_CPPFLAGS = \
	$(WARN_CFLAGS) \
	$(DEPRECATED_FLAGS) \
	$(LIBANJUTA_CFLAGS) \
	-DG_LOG_DOMAIN=\"code_analyzer\"

# Where to install the plugin
plugindir = $(anjuta_plugin_dir)

# The plugin
plugin_LTLIBRARIES = libanjuta-code-analyzer.la

# Plugin sources
libanjuta_code_analyzer_la_SOURCES = \
	plugin.c \
	plugin.h

libanjuta_code_analyzer_la_LDFLAGS = $(ANJUTA_PLUGIN_LDFLAGS)

# Plugin dependencies
libanjuta_code_analyzer_la_LIBADD = \
	$(LIBANJUTA_LIBS)

gsettings_in_file = org.gnome.anjuta.code-analyzer.gschema.xml.in
gsettings_SCHEMAS = $(gsettings_in_file:.xml.in=.xml)
@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

EXTRA_DIST = \
	$(plugin_in_files) \
	$(code_analyzer_plugin_DATA) \
	$(code_analyzer_pixmaps_DATA) \
	$(code_analyzer_ui_DATA) \
	$(gsettings_in_file)

CLEANFILES = \
        $(gsettings_SCHEMAS)

-include $(top_srcdir)/git.mk
