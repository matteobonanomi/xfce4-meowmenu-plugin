/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef WHISKERMENU_PRESET_IO_H
#define WHISKERMENU_PRESET_IO_H

#include <string>
#include <vector>

#include "preset.h"

namespace WhiskerMenu
{

class Settings;

enum class ImportStatus
{
	Ok,
	ConflictBuiltin,
	ConflictUser,
	ParseError,
	MissingSection,
	MissingKey,
};

struct ImportResult
{
	ImportStatus status = ImportStatus::ParseError;
	std::string  display_name;
	std::string  conflict_uuid;
	std::string  new_uuid;
	std::string  error_message;
};

// Export a user preset to a .meowpreset file.
// Returns false if uuid is not found or file write fails.
bool export_user_preset(const std::string& uuid,
	const std::string& dest_path,
	Settings& settings);

// Import a .meowpreset file and create a user preset.
//   display_name_override — if non-empty, use this name instead of [Preset]/Name.
//   overwrite_uuid        — if non-empty, overwrite the named user preset instead of creating new.
ImportResult import_user_preset(const std::string& file_path,
	Settings& settings,
	const std::string& display_name_override = {},
	const std::string& overwrite_uuid = {});

// Enumerate .meowpreset files from system_dir (first) then user_dir.
// Files with the same id (filename stem without extension) are deduplicated: user wins.
// Malformed or unreadable files are silently skipped (g_debug() only).
// Never crashes, never shows an error dialog (FR-063).
std::vector<LayoutPreset> enumerate_preset_files(const std::string& system_dir,
	const std::string& user_dir);

} // namespace WhiskerMenu

#endif // WHISKERMENU_PRESET_IO_H
