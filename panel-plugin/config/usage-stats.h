/*
 * Copyright (C) 2026 MeowMenu Contributors
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

#ifndef WHISKERMENU_USAGE_STATS_H
#define WHISKERMENU_USAGE_STATS_H

#include <string>
#include <unordered_map>

#include <glib.h>

namespace WhiskerMenu
{

struct AppStats
{
	gint64 last_launch_unix = 0;
	int    launch_count     = 0;
};

class UsageStats
{
public:
	UsageStats();

	// O(1) frecency score in [0,1]. alpha = weight for recency vs frequency.
	double get_frecency(const char* desktop_id,
	                    double alpha,
	                    int max_launches = 100) const;

	// Called when user launches an app; updates in-memory map and schedules async write.
	void record_launch(const char* desktop_id);

private:
	void load();
	void save() const;

	static gboolean write_idle_cb(gpointer data);

	std::unordered_map<std::string, AppStats> m_stats;
	std::string m_cache_path;
	bool m_write_scheduled = false;
};

} // namespace WhiskerMenu

#endif // WHISKERMENU_USAGE_STATS_H
