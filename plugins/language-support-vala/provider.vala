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

public class ValaProvider : Object {
	weak ValaPlugin plugin;

	static Regex member_access;
	static Regex member_access_split;
	static Regex function_call;

	static construct {
		try {
			member_access = new Regex("""((?:\w+(?:\s*\([^()]*\))?\.)*)(\w*)$""");
			member_access_split = new Regex ("""(\s*\([^()]*\))?\.""");
			function_call = new Regex("""(new )?((?:\w+(?:\s*\([^()]*\))?\.)*)(\w+)\s*\(([^(,)]+,)*([^(,)]*)$""");
		} catch(RegexError err) {
			critical("Regular expressions failed to compile : %s", err.message);
		}
	}

	public ValaProvider(ValaPlugin plugin) {
		this.plugin = plugin;
	}
	public void populate (IAnjuta.Iterable iter) throws GLib.Error {
		if (!plugin.settings.get_boolean (PREF_AUTOCOMPLETE_ENABLE))
			return;

		var editor = plugin.current_editor as IAnjuta.EditorAssist;
		var line_start = editor.get_line_begin_position(editor.get_lineno());
		var current_text = editor.get_text(line_start, iter);

		MatchInfo match_info;
		if (! member_access.match(current_text, 0, out match_info))
			return;
		if (match_info.fetch(0).length < 2)
			return;

		start_pos = iter.clone();
		start_pos.set_position(iter.get_position() - (int) match_info.fetch(2).length);

		var names = member_access_split.split (match_info.fetch(1));

		var syms = plugin.lookup_symbol (construct_member_access (names), match_info.fetch(2),
		                                 true, plugin.get_current_context (editor) as Vala.Block);

		var proposals = new GLib.List<IAnjuta.EditorAssistProposal?>();
		foreach (var symbol in syms) {
			if (symbol is Vala.LocalVariable
			    && symbol.source_reference.first_line > editor.get_lineno())
				continue;
		
			var data = IAnjuta.ParserProposalData();
			data.name = symbol.name;
			
			Vala.List<Vala.Parameter> parameters = null;
			if (sym is Vala.Method) {
				data.is_func = true;
				parameters = ((Vala.Method) sym).get_parameters ();
			} else if (sym is Vala.Signal) {
				data.is_func = true;
				parameters = ((Vala.Signal) sym).get_parameters ();
			} else if (creation_method && sym is Vala.Class) {
				parameters = ((Vala.Class) sym).default_construction_method.get_parameters ();
			} else if (sym is Vala.Variable) {
				var var_type = ((Vala.Variable) sym).variable_type;
				if (var_type is Vala.DelegateType) {
					data.is_func = true;
					parameters = ((Vala.DelegateType) var_type).delegate_symbol.get_parameters ();
				}
			}
			
			data.has_para = !(parameters.length == 0);
			
			var prop = IAnjuta.EditorAssistProposal();
			prop.data = data;
			prop.label = symbol.name;
			proposals.prepend(prop);
		}
		proposals.reverse();
		editor.proposals(this, proposals, null, true);
	}
	public void show_call_tip (IAnjuta.EditorTip editor) {
		var current_position = editor.get_position ();
		var line_start = editor.get_line_begin_position(editor.get_lineno());
		var to_complete = editor.get_text(line_start, current_position);

		List<string> tips = null;

		MatchInfo match_info;
		if (! function_call.match(to_complete, 0, out match_info))
			return;

		var creation_method = (match_info.fetch(1) != "");
		var names = member_access_split.split (match_info.fetch(2));
		var syms = plugin.lookup_symbol (construct_member_access (names), match_info.fetch(3),
		                                 false, plugin.get_current_context (editor) as Vala.Block);
		foreach (var sym in syms) {
			var calltip = new StringBuilder ();
			Vala.List<Vala.Parameter> parameters = null;
			if (sym is Vala.Method) {
				parameters = ((Vala.Method) sym).get_parameters ();
				calltip.append (((Vala.Method) sym).return_type.to_qualified_string() + " ");
				var method_parents = new StringBuilder ();
				var parent = ((Vala.Method) sym).parent_symbol;
				while (parent is Vala.Class) {
					method_parents.prepend (parent.name + ".");
					parent = parent.parent_symbol;
				}
				calltip.append (method_parents.str);
				calltip.append (((Vala.Method) sym).name);
			} else if (sym is Vala.Signal) {
				parameters = ((Vala.Signal) sym).get_parameters ();
				calltip.append (((Vala.Signal) sym).name);
			} else if (creation_method && sym is Vala.Class) {
				parameters = ((Vala.Class) sym).default_construction_method.get_parameters ();
				calltip.append (((Vala.Class) sym).name);
			} else if (sym is Vala.Variable) {
				var var_type = ((Vala.Variable) sym).variable_type;
				if (var_type is Vala.DelegateType) {
					parameters = ((Vala.DelegateType) var_type).delegate_symbol.get_parameters ();
					calltip.append (var_type.to_qualified_string() + " ");
					calltip.append (((Vala.Variable) sym).name);
				} else {
					return;
				}
			} else {
				return;
			}
			calltip.append(" (");
			var prestring = "".nfill (calltip.len, ' ');
			var first = true;
			foreach (var p in parameters) {
				if(first) {
					first = false;
				} else {
					calltip.append (",\n");
					calltip.append (prestring);
				}
				if (p.ellipsis) {
					calltip.append ("...");
				} else {
					calltip.append (p.variable_type.to_qualified_string());
					calltip.append (" ").append (p.name);
				}
			}
			calltip.append (")");
			tips.append (calltip.str);
		}
		editor.show (tips, editor.get_position ());
	}
	Vala.Expression construct_member_access (string[] names) {
		Vala.Expression expr = null;

		for (var i = 0; names[i] != null; i++) {
			if (names[i] != "") {
				expr = new Vala.MemberAccess (expr, names[i]);
				if (names[i+1] != null && names[i+1].chug ().has_prefix ("(")) {
					expr = new Vala.MethodCall (expr);
					i++;
				}
			}
		}

		return expr;
	}
}
