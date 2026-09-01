/*
 * Headless coverage for asynchronous application-load generation lifecycle.
 */

#include "launcher/application-load-generation.h"

#include <cassert>

using namespace WhiskerMenu;

static void test_successful_commit()
{
	ApplicationLoadGeneration load(7);
	assert(load.status() == ApplicationLoadStatus::Queued);
	assert(load.start());
	assert(load.status() == ApplicationLoadStatus::Loading);
	assert(load.candidate_ready(true));
	assert(load.status() == ApplicationLoadStatus::CandidateReady);
	assert(load.commit(7, true));
	assert(load.status() == ApplicationLoadStatus::Committed);
	load.discard();
	assert(load.status() == ApplicationLoadStatus::Committed);
}

static void test_stale_and_failed_candidates_are_discarded()
{
	ApplicationLoadGeneration stale(7);
	assert(stale.start());
	assert(stale.candidate_ready(true));
	assert(!stale.commit(8, true));
	assert(stale.status() == ApplicationLoadStatus::Discarded);

	ApplicationLoadGeneration failed(9);
	assert(failed.start());
	assert(!failed.candidate_ready(false));
	assert(failed.status() == ApplicationLoadStatus::Discarded);
}

static void test_invalidation_requests_follow_up()
{
	ApplicationLoadGeneration load(11);
	assert(load.start());
	load.invalidate();
	assert(load.follow_up_required());
	assert(!load.candidate_ready(true));
	assert(load.status() == ApplicationLoadStatus::Discarded);
}

static void test_cancel_and_owner_loss_are_terminal()
{
	ApplicationLoadGeneration cancelled(12);
	assert(cancelled.start());
	cancelled.cancel();
	assert(cancelled.status() == ApplicationLoadStatus::Discarded);
	assert(!cancelled.candidate_ready(true));
	cancelled.cancel();

	ApplicationLoadGeneration owner_lost(13);
	assert(owner_lost.start());
	assert(owner_lost.candidate_ready(true));
	assert(!owner_lost.commit(13, false));
	assert(owner_lost.status() == ApplicationLoadStatus::Discarded);
}

int main()
{
	test_successful_commit();
	test_stale_and_failed_candidates_are_discarded();
	test_invalidation_requests_follow_up();
	test_cancel_and_owner_loss_are_terminal();
	return 0;
}
