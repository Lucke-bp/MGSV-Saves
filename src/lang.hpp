#pragma once

#include "common.hpp"

// The game's texts, replaced in memory by key (the same in every language).
// The texts live in LANG v3 blocks loaded on the heap: "LANG", version 3, "BE", entry count, values offset, keys
// offset; keys = big-endian (hash of the text id, value offset) pairs sorted by hash; values = u16 + UTF-8 + NUL.
// A replacement is written over the original text (cut to its length, UTF-8 safe); a longer one also goes to a copy
// in memory of ours, near the block, and the key's entry is pointed at it (the original offset is put back on restore).
// A background thread finds the blocks (they are loaded and freed as the game goes) and keeps the persistent
// replacements applied.
namespace mod::lang
{
	void start();

	// Replaces a text now, in the blocks found so far. False if the key is not in any of them yet (a new scan is
	// then requested).
	bool set(uint32_t key, const std::string &text);
	// Keeps a text replaced, also in blocks loaded later.
	void set_persistent(uint32_t key, const std::string &text);
	// Puts the game's own text back (and drops a persistent replacement).
	void restore(uint32_t key);
	// The game's own text of a key (empty if not found yet).
	std::string original(uint32_t key);
}
