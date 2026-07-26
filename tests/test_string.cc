// include/util/string.hpp — tokenizers and line iteration.
//
// This is the highest-value test file in the tree. string.hpp is 688 lines, a
// fifth of the codebase, and it MUST change for C++17: it is built on
// std::ptr_fun, std::not1 and std::unary_function, all of which C++17 removed
// or deprecated (libstdc++ still provides them, so it compiles today with
// warnings — but only just).
//
// Its only consumer is loaders/obj.cc, so a tokenizer regression surfaces as
// malformed geometry rather than a compile error. These tests pin down current
// behaviour first, so the refactor has something to be checked against.
//
// See docs/TARGETS.md § "Where the test surface actually is".

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <util/string.hpp>

#include <string>
#include <vector>

namespace
{

using Tokenizer = util::quoted_whitespace_tokenizer<std::string>;
using TokenGen = util::token_generator<std::string::const_iterator, Tokenizer>;

// The exact loop loaders/obj.cc runs over each line.
std::vector<std::string> tokenize_line(const std::string& line)
{
    std::vector<std::string> tokens;
    TokenGen gen(line.begin(), line.end(), Tokenizer());
    for (std::string t = gen(); !t.empty(); t = gen()) {
        tokens.push_back(t);
    }
    return tokens;
}

std::vector<std::string> split_lines(const std::string& text)
{
    std::vector<std::string> lines;
    util::line_iterator<std::string> it(text);
    const util::line_iterator<std::string> end;
    for (; it != end; ++it) {
        lines.push_back(std::string(it->first, it->second));
    }
    return lines;
}

} // namespace

// ---------------------------------------------------------------------------
// whitespace_tokenizer
// ---------------------------------------------------------------------------
//
// Note: whitespace_tokenizer defines no token_type, so it cannot be used with
// util::tokenize() — only called directly. That asymmetry with
// quoted_whitespace_tokenizer is worth removing during the refactor.

TEST_CASE("whitespace_tokenizer extracts one token per call")
{
    const std::string input = "  alpha beta ";
    util::whitespace_tokenizer tok;

    std::string first;
    auto pos = tok(input.begin(), input.end(), first);
    CHECK(first == "alpha");

    std::string second;
    pos = tok(pos, input.end(), second);
    CHECK(second == "beta");

    std::string third;
    tok(pos, input.end(), third);
    CHECK(third.empty());
}

// ---------------------------------------------------------------------------
// quoted_whitespace_tokenizer — what the OBJ loader depends on
// ---------------------------------------------------------------------------

TEST_CASE("plain whitespace-separated tokens")
{
    const auto tokens = tokenize_line("v 1.0 -2.5 3.0");

    REQUIRE(tokens.size() == 4);
    CHECK(tokens[0] == "v");
    CHECK(tokens[1] == "1.0");
    CHECK(tokens[2] == "-2.5");
    CHECK(tokens[3] == "3.0");
}

TEST_CASE("leading and repeated whitespace is skipped")
{
    const auto tokens = tokenize_line("   f    1   2   3   ");

    REQUIRE(tokens.size() == 4);
    CHECK(tokens[0] == "f");
    CHECK(tokens[3] == "3");
}

TEST_CASE("tabs separate tokens")
{
    const auto tokens = tokenize_line("v\t1\t2\t3");
    CHECK(tokens.size() == 4);
}

TEST_CASE("an empty or whitespace-only line yields no tokens")
{
    CHECK(tokenize_line("").empty());
    CHECK(tokenize_line("    ").empty());
    CHECK(tokenize_line("\t ").empty());
}

TEST_CASE("single-quoted spans are one token, quotes stripped")
{
    const auto tokens = tokenize_line("name 'hello world' end");

    REQUIRE(tokens.size() == 3);
    CHECK(tokens[0] == "name");
    CHECK(tokens[1] == "hello world");
    CHECK(tokens[2] == "end");
}

TEST_CASE("double-quoted spans are one token, quotes stripped")
{
    const auto tokens = tokenize_line("name \"hello world\" end");

    REQUIRE(tokens.size() == 3);
    CHECK(tokens[1] == "hello world");
}

TEST_CASE("a quote terminates an unquoted token")
{
    // token_break treats both quote characters as separators, so `abc"def"`
    // splits rather than concatenating.
    const auto tokens = tokenize_line("abc\"def\"");

    REQUIRE(tokens.size() == 2);
    CHECK(tokens[0] == "abc");
    CHECK(tokens[1] == "def");
}

// Escapes are *found* but not *removed*: copy() moves the raw range, so the
// backslash survives into the token. Pinning this down because it is
// surprising, and because any refactor is likely to change it by accident.
TEST_CASE("escaped quotes keep their backslash in the token")
{
    const auto tokens = tokenize_line("\"a\\\"b\"");

    REQUIRE(tokens.size() == 1);
    CHECK(tokens[0] == "a\\\"b");
}

TEST_CASE("an unterminated quote consumes the rest of the input")
{
    const auto tokens = tokenize_line("start 'never closed");

    REQUIRE(tokens.size() == 2);
    CHECK(tokens[0] == "start");
    CHECK(tokens[1] == "never closed");
}

// ---------------------------------------------------------------------------
// util::tokenize
// ---------------------------------------------------------------------------

TEST_CASE("tokenize fills an output iterator and drops empty tokens")
{
    const std::string input = "  a  b  c  ";
    std::vector<std::string> tokens;

    util::tokenize(input.begin(), input.end(), std::back_inserter(tokens),
                   Tokenizer());

    REQUIRE(tokens.size() == 3);
    CHECK(tokens[0] == "a");
    CHECK(tokens[2] == "c");
}

// ---------------------------------------------------------------------------
// line_iterator
// ---------------------------------------------------------------------------
//
// Lines INCLUDE their trailing newline — _next() advances past the stop
// sequence and stores [cur, pos). loaders/obj.cc relies on the tokenizer
// treating that '\n' as whitespace.

TEST_CASE("lines retain their trailing newline")
{
    const auto lines = split_lines("a\nb\n");

    REQUIRE(lines.size() == 2);
    CHECK(lines[0] == "a\n");
    CHECK(lines[1] == "b\n");
}

TEST_CASE("a final line without a newline is still produced")
{
    const auto lines = split_lines("a\nb");

    REQUIRE(lines.size() == 2);
    CHECK(lines[0] == "a\n");
    CHECK(lines[1] == "b");
}

TEST_CASE("an empty input produces no lines")
{
    CHECK(split_lines("").empty());
}

TEST_CASE("blank lines are yielded and do not terminate iteration")
{
    // _next() ends iteration when the stride comes out empty, and a stride is
    // empty only when pos == _cur. A blank line puts the stop sequence *at*
    // _cur, so pos advances past it and the stride is the one-character "\n" —
    // non-empty. The empty stride therefore happens only at _cur == _end, which
    // is genuine exhaustion.
    //
    // Asserted exactly rather than loosely: a weak assertion here would hold
    // whether or not blank lines terminate iteration, which is the property
    // under test.
    const std::vector<std::string> middle = split_lines("a\n\nb\n");
    REQUIRE(middle.size() == 3);
    CHECK(middle[0] == "a\n");
    CHECK(middle[1] == "\n");
    CHECK(middle[2] == "b\n");

    const std::vector<std::string> consecutive = split_lines("a\n\n\nb\n");
    REQUIRE(consecutive.size() == 4);
    CHECK(consecutive[1] == "\n");
    CHECK(consecutive[2] == "\n");
    CHECK(consecutive[3] == "b\n");

    const std::vector<std::string> leading = split_lines("\na\n");
    REQUIRE(leading.size() == 2);
    CHECK(leading[0] == "\n");
    CHECK(leading[1] == "a\n");

    const std::vector<std::string> trailing = split_lines("a\n\n");
    REQUIRE(trailing.size() == 2);
    CHECK(trailing[1] == "\n");

    // A final line with no terminator is still yielded.
    const std::vector<std::string> unterminated = split_lines("a\nb");
    REQUIRE(unterminated.size() == 2);
    CHECK(unterminated[1] == "b");

    CHECK(split_lines("").empty());
    REQUIRE(split_lines("\n").size() == 1);
}

TEST_CASE("a copied line_iterator carries its whole state")
{
    // The copy has to be independently usable, which needs _cur, _end, _valid
    // and _stop_sequence — not just the current stride. Iterating the copy all
    // the way to the end is what exercises that.
    const std::string text = "a\nb\nc\n";
    util::line_iterator<std::string> it(text);
    const util::line_iterator<std::string> end;

    util::line_iterator<std::string> copy(it);
    CHECK(copy == it);
    CHECK(std::string(copy->first, copy->second) == "a\n");

    std::vector<std::string> from_copy;
    for (; copy != end; ++copy) {
        from_copy.push_back(std::string(copy->first, copy->second));
    }
    REQUIRE(from_copy.size() == 3);
    CHECK(from_copy[0] == "a\n");
    CHECK(from_copy[2] == "c\n");

    // The original is undisturbed by the copy's traversal.
    CHECK(std::string(it->first, it->second) == "a\n");
}

TEST_CASE("line_iterator assignment carries its whole state")
{
    const std::string text = "a\nb\nc\n";
    util::line_iterator<std::string> it(text);
    const util::line_iterator<std::string> end;

    util::line_iterator<std::string> assigned;
    assigned = it;
    CHECK(assigned == it);

    std::vector<std::string> from_assigned;
    for (; assigned != end; ++assigned) {
        from_assigned.push_back(std::string(assigned->first, assigned->second));
    }
    CHECK(from_assigned.size() == 3);
}

TEST_CASE("post-increment returns the previous position by value")
{
    const std::string text = "a\nb\nc\n";
    util::line_iterator<std::string> it(text);

    auto before = it++;

    CHECK(std::string(before->first, before->second) == "a\n");
    CHECK(std::string(it->first, it->second) == "b\n");
    CHECK(before != it);

    // The returned object outlives the expression and stays usable.
    ++before;
    CHECK(std::string(before->first, before->second) == "b\n");
}

TEST_CASE("line and token iteration compose, as in the OBJ loader")
{
    const std::string obj = "v 0.0 0.0 0.0\n"
                            "v 1.0 0.0 0.0\n"
                            "f 1 2 3\n";

    std::vector<std::string> keywords;
    for (const auto& line : split_lines(obj)) {
        const auto tokens = tokenize_line(line);
        if (!tokens.empty()) {
            keywords.push_back(tokens[0]);
        }
    }

    REQUIRE(keywords.size() == 3);
    CHECK(keywords[0] == "v");
    CHECK(keywords[1] == "v");
    CHECK(keywords[2] == "f");
}

// ---------------------------------------------------------------------------
// find_escaped
// ---------------------------------------------------------------------------

TEST_CASE("find_escaped skips escaped occurrences")
{
    const std::string input = "ab\\cd|ef";

    const auto hit = util::find_escaped(input.begin(), input.end(), '|', '\\');
    REQUIRE(hit != input.end());
    CHECK(static_cast<std::size_t>(hit - input.begin()) == 5);
}

TEST_CASE("find_escaped returns last when the value never appears unescaped")
{
    const std::string input = "a\\|b";

    const auto hit = util::find_escaped(input.begin(), input.end(), '|', '\\');
    CHECK(hit == input.end());
}

// ---------------------------------------------------------------------------
// Coverage notes
// ---------------------------------------------------------------------------
//
// What is deliberately not asserted: escaped_find has no consumer — the
// tokenizer calls the free find_escaped instead — so it is covered by
// construction only, not by behaviour.
