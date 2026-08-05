/* Headless coverage for the Places first-result focus lease. */

#include "places/places-page.h"
#include "places/places-section.h"

#include <cassert>
#include <cstdio>

using WhiskerMenu::PlacesFocusLease;
using WhiskerMenu::PlacesFocusLeaseState;

namespace
{

class TestSection : public WhiskerMenu::PlacesSection
{
public:
	std::vector<WhiskerMenu::PlacesItem*> get_items(int) override
	{
		return {};
	}

	void clear_items() override {}
	const gchar* get_icon_name() const override { return "folder"; }
	const gchar* get_display_name() const override { return "Test"; }
};

void current_first_result_claims_once()
{
	PlacesFocusLease lease;
	TestSection section;
	const auto generation = lease.begin("needle", &section, true);
	assert(lease.state() == PlacesFocusLeaseState::AwaitingFirst);
	assert(lease.matches(generation, "needle", &section, true));
	assert(lease.claim_first(generation, "needle", &section, true));
	assert(lease.state() == PlacesFocusLeaseState::Settled);
	assert(!lease.claim_first(generation, "needle", &section, true));
}

void stale_and_relinquished_results_cannot_claim()
{
	PlacesFocusLease lease;
	TestSection first;
	TestSection second;
	const auto old_generation = lease.begin("old", &first, true);
	const auto new_generation = lease.begin("new", &second, true);
	assert(!lease.claim_first(old_generation, "old", &first, true));
	lease.relinquish();
	assert(lease.state() == PlacesFocusLeaseState::Relinquished);
	assert(!lease.claim_first(new_generation, "new", &second, true));
}

void empty_and_inactive_queries_are_idle_or_settled()
{
	PlacesFocusLease lease;
	TestSection section;
	const auto empty = lease.begin("", &section, true);
	assert(lease.state() == PlacesFocusLeaseState::Idle);
	assert(!lease.claim_first(empty, "", &section, true));
	const auto inactive = lease.begin("needle", &section, false);
	assert(lease.state() == PlacesFocusLeaseState::Idle);
	lease.settle_empty(inactive, "needle", &section, false);
	assert(lease.state() == PlacesFocusLeaseState::Idle);
	const auto current = lease.begin("missing", &section, true);
	lease.settle_empty(current, "missing", &section, true);
	assert(lease.state() == PlacesFocusLeaseState::Settled);
}

} // namespace

int main()
{
	current_first_result_claims_once();
	stale_and_relinquished_results_cannot_claim();
	empty_and_inactive_queries_are_idle_or_settled();
	std::puts("test_places_focus_policy: ok");
	return 0;
}
