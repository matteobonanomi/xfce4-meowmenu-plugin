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

#include "usage-stats.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>

#include <glib/gstdio.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

UsageStats::UsageStats() :
	m_write_idle_id(0)
{
	const gchar* cache_dir = g_get_user_cache_dir();
	m_cache_path = std::string(cache_dir) + "/xfce4/meowmenu/stats";
	load();
}

//-----------------------------------------------------------------------------

/* UsageStats::~UsageStats:
 *
 * Cancels a pending coalesced idle before releasing its retained state and
 * performs the promised write synchronously so the last launch is not lost.
 */
UsageStats::~UsageStats()
{
	if (m_write_idle_id != 0)
	{
		g_source_remove(m_write_idle_id);
		m_write_idle_id = 0;
		save();
	}
}

//-----------------------------------------------------------------------------

double UsageStats::get_frecency(const char* desktop_id, double alpha, int max_launches) const
{
	if (!desktop_id)
		return 0.0;

	auto it = m_stats.find(desktop_id);
	if (it == m_stats.end())
		return 0.0;

	const AppStats& s = it->second;

	// Recency: hyperbolic decay by days since last launch
	gint64 now = g_get_real_time() / G_USEC_PER_SEC;
	gint64 delta_days = (now - s.last_launch_unix) / 86400;
	if (delta_days < 0)
		delta_days = 0;
	const double recency = 1.0 / (static_cast<double>(delta_days) + 1.0);

	// Frequency: log-normalized launch count
	const int capped = (s.launch_count < max_launches) ? s.launch_count : max_launches;
	const double frequency = std::log2(static_cast<double>(capped) + 1.0)
	                       / std::log2(static_cast<double>(max_launches) + 1.0);

	return alpha * recency + (1.0 - alpha) * frequency;
}

//-----------------------------------------------------------------------------

/* UsageStats::record_launch:
 * @desktop_id: application identifier to update; NULL is ignored.
 *
 * Updates the in-memory counters and schedules at most one coalesced write.
 * The stored source identifier is cleared on delivery or settled at teardown.
 */
void UsageStats::record_launch(const char* desktop_id)
{
	if (!desktop_id)
		return;

	AppStats& s = m_stats[desktop_id];
	s.last_launch_unix = g_get_real_time() / G_USEC_PER_SEC;
	s.launch_count     = (s.launch_count < 100000) ? s.launch_count + 1 : 100000;

	if (m_write_idle_id == 0)
	{
		m_write_idle_id = g_idle_add(write_idle_cb, this);
	}
}

//-----------------------------------------------------------------------------

/* UsageStats::write_idle_cb:
 * @data: UsageStats that owns the delivered source.
 *
 * Clears source ownership before saving so teardown cannot remove a delivered
 * identifier or perform a duplicate final flush.
 *
 * Returns: G_SOURCE_REMOVE after the coalesced write.
 */
gboolean UsageStats::write_idle_cb(gpointer data)
{
	UsageStats* self = static_cast<UsageStats*>(data);
	self->m_write_idle_id = 0;
	self->save();
	return G_SOURCE_REMOVE;
}

//-----------------------------------------------------------------------------

/* UsageStats::load:
 *
 * Loads the tab-delimited cache while treating the first tab as the field
 * boundary, so identifiers containing ordinary whitespace remain intact.
 * Malformed records are ignored with the existing warning.
 */
void UsageStats::load()
{
	FILE* f = std::fopen(m_cache_path.c_str(), "r");
	if (!f)
		return;

	char line[512];
	while (std::fgets(line, sizeof(line), f))
	{
		// Trim trailing newline
		const size_t len = std::strlen(line);
		if (len > 0 && line[len - 1] == '\n')
			line[len - 1] = '\0';

		gint64 last_launch;
		int    launch_count;

		// TSV: desktop_id <TAB> last_launch_unix <TAB> launch_count
		const char* separator = std::strchr(line, '\t');
		if (separator && std::sscanf(separator + 1,
				"%" G_GINT64_FORMAT "\t%d",
				&last_launch, &launch_count) == 2)
		{
			const std::string desktop_id(line,
					static_cast<std::size_t>(separator - line));
			AppStats s;
			s.last_launch_unix = last_launch;
			s.launch_count     = launch_count;
			m_stats[desktop_id] = s;
		}
		else
		{
			g_warning("usage-stats: skipping malformed line: %s", line);
		}
	}

	std::fclose(f);
}

//-----------------------------------------------------------------------------

void UsageStats::save() const
{
	// Ensure cache directory exists
	gchar* dir = g_path_get_dirname(m_cache_path.c_str());
	if (g_mkdir_with_parents(dir, 0700) != 0)
	{
		g_warning("usage-stats: failed to create cache directory: %s", dir);
		g_free(dir);
		return;
	}
	g_free(dir);

	// Build content string
	std::string content;
	content.reserve(m_stats.size() * 64);
	for (const auto& kv : m_stats)
	{
		char buf[512];
		std::snprintf(buf, sizeof(buf), "%s\t%lld\t%d\n",
		              kv.first.c_str(),
		              static_cast<long long>(kv.second.last_launch_unix),
		              kv.second.launch_count);
		content += buf;
	}

	GError* error = nullptr;
	if (!g_file_set_contents(m_cache_path.c_str(), content.c_str(), static_cast<gssize>(content.size()), &error))
	{
		g_warning("usage-stats: failed to save stats: %s", error ? error->message : "unknown error");
		if (error)
			g_error_free(error);
	}
}

//-----------------------------------------------------------------------------
