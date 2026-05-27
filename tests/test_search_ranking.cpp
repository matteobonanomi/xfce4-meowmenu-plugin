/* test_search_ranking:
 *
 * Fail-closed characterization gate over panel-plugin/query.cpp's three
 * public match functions. Freezes the score values returned for each
 * match class on a fixed dataset, so any subsequent change to the ranking
 * implementation (intentional or accidental) makes this test diverge.
 *
 * Match classes exercised (per RF-SEARCH-001..005, spec §11.RR-03):
 *   Query::match()
 *     0x4   — exact match (haystack == query)
 *     0x8   — prefix match (haystack starts with query)
 *     0x10  — query found at a word boundary inside haystack
 *     0x20  — multi-token query found in order at word boundaries
 *     0x40  — multi-token query found in any order at word boundaries
 *     0x80  — query found as substring not at a word boundary
 *   Query::match_as_characters()
 *     0x100 — every query character is the first letter of some haystack word
 *     0x200 — every query character is present in haystack in order (scattered)
 *   Query::match_fuzzy()
 *     0x400 — Levenshtein distance ≤ max_errors against some haystack word
 *
 * What this test does NOT cover, by design (research.md §3.11):
 *   - launcher.cpp::search() classifier OR-bits (0x400 name, 0x800 generic,
 *     0x1000 comment, 0x2000 keywords, 0x4000 exec) and alias iteration.
 *   - search-page.cpp favorites-boost / frecency arithmetic.
 *   Both are protected by existing tests and the manual matrix; lifting
 *   them into a standalone unit requires the full Settings + Element +
 *   garcon dependency surface and is deferred to a follow-up task.
 *
 * Linking: query.cpp is compiled into this test binary so production code
 * is exercised end-to-end. No shadow re-implementation per RF-TESTS-003.
 */

#include "../panel-plugin/search/query.h"

#include <cassert>
#include <climits>
#include <cstdio>
#include <string>

using WhiskerMenu::Query;

namespace
{

// ---------------------------------------------------------------------------
// Query::match() — substring / prefix / word-boundary scoring
// ---------------------------------------------------------------------------

void test_match_exact()
{
	Query q("firefox");
	const unsigned int s = q.match("firefox");
	assert(s == 0x4 && "exact match must score 0x4");
}

void test_match_prefix()
{
	Query q("fire");
	const unsigned int s = q.match("firefox");
	assert(s == 0x8 && "prefix-but-not-exact must score 0x8");
}

void test_match_word_boundary()
{
	Query q("manager");
	// "manager" appears at offset 5, preceded by a space — word boundary.
	const unsigned int s = q.match("file manager");
	assert(s == 0x10 && "word-boundary substring must score 0x10");
}

void test_match_multi_token_in_order_at_boundaries()
{
	// Two tokens in document order, each at a word boundary, BUT the
	// concatenated query "file utility" is not a contiguous substring of
	// the haystack — so the prefix/word-boundary branches do not fire
	// and we land in the multi-token in-order branch.
	Query q("file utility");
	const unsigned int s = q.match("file manager utility");
	assert(s == 0x20 && "multi-token in-order at boundaries must score 0x20");
}

void test_match_multi_token_any_order_at_boundaries()
{
	// Tokens not in document order. The in-order branch fails because
	// "file" is not found at a boundary after "utility"; the any-order
	// branch then walks each query word independently and succeeds.
	Query q("utility file");
	const unsigned int s = q.match("file manager utility");
	assert(s == 0x40 && "multi-token any-order at boundaries must score 0x40");
}

void test_match_mid_word_substring()
{
	// "ref" appears inside "firefox" but not at a word boundary.
	Query q("ref");
	const unsigned int s = q.match("firefox");
	assert(s == 0x80 && "mid-word substring must score 0x80");
}

void test_match_no_match_returns_uintmax()
{
	Query q("nothing");
	const unsigned int s = q.match("firefox");
	assert(s == UINT_MAX && "no-match must return UINT_MAX sentinel");
}

void test_match_case_insensitive_via_casefold()
{
	// set() casefolds the query; match() compares case-folded forms.
	Query q("FIREFOX");
	const unsigned int s = q.match("firefox");
	assert(s == 0x4 && "casefold must equalise case for exact match");
}

// ---------------------------------------------------------------------------
// Query::match_as_characters() — start-word / scattered char scoring
// ---------------------------------------------------------------------------

void test_match_as_characters_start_words()
{
	// Each character in "fm" matches the first letter of a haystack word
	// (file manager) → start-words match.
	Query q("fm");
	const unsigned int s = q.match_as_characters("file manager");
	assert(s == 0x100 && "start-words char match must score 0x100");
}

void test_match_as_characters_scattered()
{
	// "ffo" is not a start-words sequence but is present scattered in
	// "firefox" (f, f, o).
	Query q("ffo");
	const unsigned int s = q.match_as_characters("firefox");
	assert(s == 0x200 && "scattered char match must score 0x200");
}

void test_match_as_characters_no_match()
{
	Query q("zzz");
	const unsigned int s = q.match_as_characters("firefox");
	assert(s == UINT_MAX && "no char-match must return UINT_MAX");
}

// ---------------------------------------------------------------------------
// Query::match_fuzzy() — Levenshtein-bounded match
// ---------------------------------------------------------------------------

void test_match_fuzzy_one_substitution()
{
	// "firafox" vs "firefox" — distance 1.
	Query q("firafox");
	const unsigned int s = q.match_fuzzy("firefox", 1);
	assert(s == 0x400 && "fuzzy match within max_errors must score 0x400");
}

void test_match_fuzzy_beyond_threshold_no_match()
{
	// Distance 3, but max_errors=1 — must not match.
	Query q("zzzfox");
	const unsigned int s = q.match_fuzzy("firefox", 1);
	assert(s == UINT_MAX && "fuzzy distance beyond max_errors must reject");
}

void test_match_fuzzy_multi_token_query_rejected()
{
	// Multi-token queries are explicitly disallowed (false-positive control).
	Query q("fire fox");
	const unsigned int s = q.match_fuzzy("firefox", 2);
	assert(s == UINT_MAX && "fuzzy must reject multi-token queries");
}

void test_match_fuzzy_haystack_too_short_rejected()
{
	// Haystack length < query length - max_errors → cannot possibly match.
	Query q("manager");
	const unsigned int s = q.match_fuzzy("mg", 1);
	assert(s == UINT_MAX && "fuzzy must reject trivially short haystack");
}

// ---------------------------------------------------------------------------
// Class-ordering invariant
//
// Lower scores rank higher in the production sort. Freeze the strict order
// across match classes so a future ranking change cannot accidentally
// invert two classes.
// ---------------------------------------------------------------------------

void test_class_ordering_is_strict()
{
	const unsigned int exact = 0x4;
	const unsigned int prefix = 0x8;
	const unsigned int wb = 0x10;
	const unsigned int multi_in_order = 0x20;
	const unsigned int multi_any_order = 0x40;
	const unsigned int substring = 0x80;
	const unsigned int char_start_words = 0x100;
	const unsigned int char_scattered = 0x200;
	const unsigned int fuzzy = 0x400;

	assert(exact < prefix);
	assert(prefix < wb);
	assert(wb < multi_in_order);
	assert(multi_in_order < multi_any_order);
	assert(multi_any_order < substring);
	assert(substring < char_start_words);
	assert(char_start_words < char_scattered);
	assert(char_scattered < fuzzy);
}

} // anonymous namespace

int main()
{
	test_match_exact();
	test_match_prefix();
	test_match_word_boundary();
	test_match_multi_token_in_order_at_boundaries();
	test_match_multi_token_any_order_at_boundaries();
	test_match_mid_word_substring();
	test_match_no_match_returns_uintmax();
	test_match_case_insensitive_via_casefold();

	test_match_as_characters_start_words();
	test_match_as_characters_scattered();
	test_match_as_characters_no_match();

	test_match_fuzzy_one_substitution();
	test_match_fuzzy_beyond_threshold_no_match();
	test_match_fuzzy_multi_token_query_rejected();
	test_match_fuzzy_haystack_too_short_rejected();

	test_class_ordering_is_strict();

	std::printf("OK: search-ranking characterization passed\n");
	return 0;
}
