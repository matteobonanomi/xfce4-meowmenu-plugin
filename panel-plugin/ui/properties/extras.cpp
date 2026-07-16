/*
 * Copyright (C) 2026 Matteo Bonanomi
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "settings-dialog.h"

#include "calculator/calculator-engine.h"
#include "core/plugin.h"
#include "settings.h"
#include "ui/properties/common.h"
#include "ui/slot.h"

#include <glib/gi18n-lib.h>

using namespace WhiskerMenu;

namespace
{
enum CalculatorEngineColumn
{
	EngineId,
	EngineLabel,
	EngineAvailable,
	EngineColumnCount
};

/* render_calculator_engine:
 * @layout: unused combo layout supplied by GTK.
 * @cell: text renderer owned by the combo.
 * @model: calculator engine model.
 * @iter: row being rendered.
 * @data: unused.
 *
 * Keeps engine identity, translated label, and runtime availability separate.
 * Missing engines remain selectable so the menu can show its actionable
 * guidance, but are visually distinguished without parsing markup.
 */
void render_calculator_engine(GtkCellLayout*, GtkCellRenderer* cell,
	GtkTreeModel* model, GtkTreeIter* iter, gpointer)
{
	gchar* label = nullptr;
	gboolean available = FALSE;
	gtk_tree_model_get(model, iter,
		EngineLabel, &label,
		EngineAvailable, &available,
		-1);
	const std::string text = available
		? std::string(label ? label : "")
		: std::string(label ? label : "") + " " + _("(not installed)");
	g_object_set(cell,
		"text", text.c_str(),
		"style", available ? PANGO_STYLE_NORMAL : PANGO_STYLE_ITALIC,
		"foreground", available ? nullptr : "gray",
		"foreground-set", !available,
		nullptr);
	g_free(label);
}
}

/* init_extras_tab:
 *
 * Builds the optional-provider settings page. Selecting None preserves the
 * stored subordinate choices but makes them unavailable until an engine is
 * enabled again.
 *
 * Returns: the Extras page wrapped for the dialog stack.
 */
GtkWidget* SettingsDialog::init_extras_tab()
{
	GtkBox* page = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 18));
	gtk_container_set_border_width(GTK_CONTAINER(page), 12);
	GtkWidget* grid = make_two_column_section();
	gtk_box_pack_start(page, make_aligned_frame(_("Calculator"), grid), false, false, 0);

	GtkWidget* engine_label = gtk_label_new_with_mnemonic(_("_Engine:"));
	GtkListStore* engine_store = gtk_list_store_new(EngineColumnCount,
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
	for (CalculatorEngine item : { CalculatorEngine::None, CalculatorEngine::Bc,
			CalculatorEngine::Qalculate, CalculatorEngine::GnomeCalculator })
	{
		const CalculatorEngineDescriptor& descriptor = calculator_engine_descriptor(item);
		GtkTreeIter iter;
		gtk_list_store_append(engine_store, &iter);
		gtk_list_store_set(engine_store, &iter,
			EngineId, descriptor.id,
			EngineLabel, item == CalculatorEngine::None ? _("None") : _(descriptor.label),
			EngineAvailable, item == CalculatorEngine::None || calculator_engine_is_available(item),
			-1);
	}
	GtkWidget* engine = gtk_combo_box_new_with_model(GTK_TREE_MODEL(engine_store));
	g_object_unref(engine_store);
	gtk_combo_box_set_id_column(GTK_COMBO_BOX(engine), EngineId);
	GtkCellRenderer* engine_renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(engine), engine_renderer, true);
	gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(engine), engine_renderer,
		render_calculator_engine, nullptr, nullptr);
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(engine), m_settings->calculator_engine);
	add_form_row(grid, COLUMN_C1, 0, engine_label, engine, true, nullptr);
	gtk_label_set_mnemonic_widget(GTK_LABEL(engine_label), engine);
	m_calculator_engine = engine;

	GtkWidget* font_label = gtk_label_new_with_mnemonic(_("Result font _size:"));
	GtkWidget* font = gtk_combo_box_text_new();
	const char* font_labels[] = { _("Auto"), _("Very Small"), _("Smaller"), _("Small"),
		_("Normal"), _("Large"), _("Larger"), _("Very Large") };
	for (const char* label : font_labels)
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(font), label);
	gtk_combo_box_set_active(GTK_COMBO_BOX(font), m_settings->calculator_result_font_size + 1);
	add_form_row(grid, COLUMN_C2, 0, font_label, font, true, nullptr);
	gtk_label_set_mnemonic_widget(GTK_LABEL(font_label), font);
	m_calculator_result_font_size = font;
	m_calculator_result_font_size_label = font_label;

	GtkWidget* decimal_label = gtk_label_new_with_mnemonic(_("Maximum decimal _places:"));
	GtkWidget* decimals = gtk_spin_button_new_with_range(0, 10, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(decimals), m_settings->calculator_max_decimal_places);
	add_form_row(grid, COLUMN_C1, 1, decimal_label, decimals, false, nullptr);
	gtk_label_set_mnemonic_widget(GTK_LABEL(decimal_label), decimals);
	m_calculator_max_decimal_places = decimals;
	m_calculator_max_decimal_places_label = decimal_label;

	auto update_sensitivity = [this]()
	{
		const bool enabled = g_strcmp0(m_settings->calculator_engine, "none") != 0;
		gtk_widget_set_sensitive(m_calculator_result_font_size, enabled);
		gtk_widget_set_sensitive(m_calculator_max_decimal_places, enabled);
		gtk_widget_set_sensitive(m_calculator_result_font_size_label, enabled);
		gtk_widget_set_sensitive(m_calculator_max_decimal_places_label, enabled);
	};
	update_sensitivity();

	connect(engine, "changed", [this, update_sensitivity](GtkComboBox* combo)
		{
			if (m_programmatic_update)
				return;
			const gchar* id = gtk_combo_box_get_active_id(combo);
			if (!id)
				return;
			m_settings->calculator_engine = id;
			update_sensitivity();
			m_plugin->reload_menu();
		});
	connect(font, "changed", [this](GtkComboBox* combo)
		{
			if (m_programmatic_update)
				return;
			m_settings->calculator_result_font_size = gtk_combo_box_get_active(combo) - 1;
			m_plugin->reload_menu();
		});
	connect(decimals, "value-changed", [this](GtkSpinButton* spin)
		{
			if (m_programmatic_update)
				return;
			m_settings->calculator_max_decimal_places = gtk_spin_button_get_value_as_int(spin);
			m_plugin->reload_menu();
		});

	return wrap_in_scrolled(GTK_WIDGET(page));
}
