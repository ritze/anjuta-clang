/*
 * Copyright (C) 2008-2010 Abderrahim Kitouni
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

using GLib;
using Anjuta;

public class ParserValaPlugin : Plugin {
	internal weak IAnjuta.Editor current_editor;
	internal GLib.Settings settings = new GLib.Settings ("org.gnome.anjuta.plugins.parser-cxx");
	uint editor_watch_id;
	ulong project_loaded_id;

	Vala.CodeContext context;
	Cancellable cancel;

	AnjutaReport report;
	ValaProvider provider;

	Vala.Parser parser;
	Vala.Genie.Parser genie_parser;

	Vala.Set<string> current_sources = new Vala.HashSet<string> (str_hash, str_equal);
	ParserValaPlugin () {
		Object ();
	}
	public override bool activate () {
		debug("Activating ParserValaPlugin");
		report = new AnjutaReport();
		report.docman = (IAnjuta.DocumentManager) shell.get_object("IAnjutaDocumentManager");
		parser = new Vala.Parser ();
		genie_parser = new Vala.Genie.Parser ();

		init_context ();

		provider = new ValaProvider(this);
		editor_watch_id = add_watch("document_manager_current_document",
		                            editor_value_added,
		                            editor_value_removed);

		return true;
	}

	public override bool deactivate () {
		debug("Deactivating ParserValaPlugin");
		remove_watch(editor_watch_id, true);

		cancel.cancel ();
		lock (context) {
			context = null;
		}

		return true;
	}

	void init_context () {
		context = new Vala.CodeContext();
		context.profile = Vala.Profile.GOBJECT;
		context.report = report;
		report.clear_error_indicators ();

		cancel = new Cancellable ();

		/* This doesn't actually parse anything as there are no files yet,
		   it's just to set the context in the parsers */
		parser.parse (context);
		genie_parser.parse (context);

		current_sources = new Vala.HashSet<string> (str_hash, str_equal);

	}

	void parse () {
		try {
			Thread.create<void>(() => {
				lock (context) {
					Vala.CodeContext.push(context);
					var report = context.report as AnjutaReport;

					foreach (var src in context.get_source_files ()) {
						if (src.get_nodes ().size == 0) {
							debug ("parsing file %s", src.filename);
							genie_parser.visit_source_file (src);
							parser.visit_source_file (src);
						}

						if (cancel.is_cancelled ()) {
							Vala.CodeContext.pop();
							return;
						}
					}

					if (report.get_errors () > 0 || cancel.is_cancelled ()) {
						Vala.CodeContext.pop();
						return;
					}

					context.check ();
					Vala.CodeContext.pop();
				}
			}, false);
		} catch (ThreadError err) {
			warning ("cannot create thread : %s", err.message);
		}
	}

	void add_project_files () {
		var pm = (IAnjuta.ProjectManager) shell.get_object("IAnjutaProjectManager");
		var project = pm.get_current_project ();
		var current_file = (current_editor as IAnjuta.File).get_file (); 
		if (project == null)
			return;

		Vala.CodeContext.push (context);

		var current_src = project.get_root ().get_source_from_file (current_file);
		if (current_src == null)
			return;

		var current_target = current_src.parent_type (Anjuta.ProjectNodeType.TARGET);
		if (current_target == null)
			return;

		current_target.foreach (TraverseType.PRE_ORDER, (node) => {
			if (!(Anjuta.ProjectNodeType.SOURCE in node.get_node_type ()))
				return;

			if (node.get_file () == null)
				return;

			var path = node.get_file ().get_path ();
			if (path == null)
				return;

			if (path.has_suffix (".vala") || path.has_suffix (".vapi") || path.has_suffix (".gs")) {
				if (path in current_sources) {
					debug ("file %s already added", path);
				} else {
					context.add_source_filename (path);
					current_sources.add (path);
					debug ("file %s added", path);
				}
			} else {
				debug ("file %s skipped", path);
			}
		});

		if (!context.has_package ("gobject-2.0")) {
			context.add_external_package("glib-2.0");
			context.add_external_package("gobject-2.0");
			debug ("standard packages added");
		} else {
			debug ("standard packages already added");
		}

		string[] flags = {};
		unowned Anjuta.ProjectProperty prop = current_target.get_property ("VALAFLAGS");
		if (prop != null && prop != prop.info.default_value) {
			GLib.Shell.parse_argv (prop.value, out flags);
		} else {
			/* Fall back to AM_VALAFLAGS */
			var current_group = current_target.parent_type (Anjuta.ProjectNodeType.GROUP);
			prop = current_group.get_property ("VALAFLAGS");
			if (prop != null && prop != prop.info.default_value)
				GLib.Shell.parse_argv (prop.value, out flags);
		}

		string[] packages = {};
		string[] vapidirs = {};

		for (int i = 0; i < flags.length; i++) {
			if (flags[i] == "--vapidir")
				vapidirs += flags[++i];
			else if (flags[i].has_prefix ("--vapidir="))
				vapidirs += flags[i].substring ("--vapidir=".length);
			else if (flags[i] == "--pkg")
				packages += flags[++i];
			else if (flags[i].has_prefix ("--pkg="))
				packages += flags[i].substring ("--pkg=".length);
			else
				debug ("Unknown valac flag %s", flags[i]);
		}

		var srcdir = current_target.parent_type (Anjuta.ProjectNodeType.GROUP).get_file ().get_path ();
		var top_srcdir = project.get_root ().get_file ().get_path ();
		for (int i = 0; i < vapidirs.length; i++) {
			vapidirs[i] = vapidirs[i].replace ("$(srcdir)", srcdir)
			                         .replace ("$(top_srcdir)", top_srcdir);
		}

		context.vapi_directories = vapidirs;
		foreach (var pkg in packages) {
			if (context.has_package (pkg)) {
				debug ("package %s skipped", pkg);
			} else if (context.add_external_package(pkg)) {
				debug ("package %s added", pkg);
			} else {
				debug ("package %s not found", pkg);
			}
		}
		Vala.CodeContext.pop();
	}

	public void on_project_loaded (IAnjuta.ProjectManager pm, Error? e) {
		if (context == null)
			return;
		add_project_files ();
		parse ();
		pm.disconnect (project_loaded_id);
		project_loaded_id = 0;
	}

	/* "document_manager_current_document" watch */
	public void editor_value_added (Anjuta.Plugin plugin, string name, Value value) {
		debug("editor value added");
		assert (current_editor == null);
		if (!(value.get_object() is IAnjuta.Editor)) {
			/* a glade document, for example, isn't an editor */
			return;
		}

		current_editor = value.get_object() as IAnjuta.Editor;
		var current_file = value.get_object() as IAnjuta.File;

		var pm = (IAnjuta.ProjectManager) shell.get_object("IAnjutaProjectManager");
		var project = pm.get_current_project ();

		if (!project.is_loaded()) {
			if (project_loaded_id == 0)
				project_loaded_id = pm.project_loaded.connect (on_project_loaded);
		} else {
			var cur_gfile = current_file.get_file ();
			if (cur_gfile == null) {
				// File hasn't been saved yet
				return;
			}

			if (!(cur_gfile.get_path () in current_sources)) {
				cancel.cancel ();
				lock (context) {
					init_context ();
					add_project_files ();
				}

				parse ();
			}
		}
		if (current_editor != null) {
			if (current_editor is IAnjuta.EditorAssist)
				(current_editor as IAnjuta.EditorAssist).add(provider);
			if (current_editor is IAnjuta.EditorTip)
				current_editor.char_added.connect (on_char_added);
		}
		report.update_errors (current_editor);
	}
	public void editor_value_removed (Anjuta.Plugin plugin, string name) {
		debug("editor value removed");
		if (current_editor is IAnjuta.EditorAssist)
			(current_editor as IAnjuta.EditorAssist).remove(provider);
		if (current_editor is IAnjuta.EditorTip)
			current_editor.char_added.disconnect (on_char_added);

		current_editor = null;
	}

	public void on_char_added (IAnjuta.Editor editor, IAnjuta.Iterable position, char ch) {
		if (!settings.get_boolean (ValaProvider.PREF_CALLTIP_ENABLE))
			return;

		var editortip = editor as IAnjuta.EditorTip;
		if (ch == '(') {
			provider.show_call_tip (editortip);
		} else if (ch == ')') {
			editortip.cancel ();
		}
	}
}

[ModuleInit]
public Type anjuta_glue_register_components (TypeModule module) {
    return typeof (ParserValaPlugin);
}
