#include "lang.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace mod::lang
{
	namespace
	{
		constexpr uint8_t k_magic[] = { 'L', 'A', 'N', 'G', 3, 0, 0, 0, 'B', 'E' };
		constexpr size_t k_max_region = 512ull << 20;
		constexpr ULONGLONG k_missing_rescan_ms = 5000; // a key not loaded yet: look for new blocks at most this often
		constexpr ULONGLONG k_index_rescan_ms = 5000;   // the same for the game's index
		constexpr size_t k_pool_size = 64 * 1024;
		constexpr size_t k_slot_size = 512; // u16 + text + NUL
		constexpr size_t k_max_anchors = 64;

		struct block
		{
			uint8_t *header = nullptr;
			uint32_t count = 0, values = 0, keys = 0;
		};

		// The game's index of the texts: a list of nodes sorted by key, built as blocks load. Lookups go through it
		// (not through the blocks' key tables), so a longer text is put in by pointing a node at a copy of ours.
		struct node
		{
			uint8_t *prev;
			uint8_t *next;
			uint8_t *value; // the u16 before a text
			uint32_t key;
			uint32_t flags;
		};
		constexpr size_t k_node_value = 0x10;

		// A text longer than the game's: its node points at one of two copies (the one the game may be reading is
		// never rewritten).
		struct relocation
		{
			uint8_t *node = nullptr;
			uint8_t *original_value = nullptr;
			uint8_t *slots[2] = {};
			int active = -1; // copy the node points at; -1: the game's own text
		};

		struct pool
		{
			uint8_t *base = nullptr;
			size_t used = 0;
		};

		std::mutex s_mutex;
		std::vector<block> s_blocks;
		std::map<std::pair<uint8_t *, uint32_t>, std::string> s_originals; // (text address, key) -> the game's text
		std::map<std::pair<uint8_t *, uint32_t>, relocation> s_relocations; // (block header, key)
		std::map<uint8_t *, uint8_t *> s_anchors;                           // block header -> one of its index nodes
		std::vector<pool> s_pools;
		std::map<uint32_t, std::string> s_persistent;
		std::atomic<bool> s_rescan = true;
		std::atomic<bool> s_index_wanted = false; // a node was not found: look for the index again

		// Plain functions so SEH can guard the accesses (the blocks are loaded and freed by the game).
		bool read(const void *source, void *destination, size_t size)
		{
			__try
			{
				std::memcpy(destination, source, size);
				return true;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				return false;
			}
		}

		bool write(void *destination, const void *source, size_t size)
		{
			__try
			{
				std::memcpy(destination, source, size);
				return true;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				return false;
			}
		}

		// One aligned 8-byte store: the game never sees half a pointer.
		void write_pointer(uint8_t *address, const void *value)
		{
			write(address, &value, sizeof(value));
		}

		template <typename Visit>
		void for_each_region(Visit visit)
		{
			MEMORY_BASIC_INFORMATION mbi;
			uintptr_t address = 0x10000;
			while (VirtualQuery(reinterpret_cast<const void *>(address), &mbi, sizeof(mbi)) == sizeof(mbi))
			{
				const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
				if (mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE && (mbi.Protect & 0xFF) == PAGE_READWRITE &&
					(mbi.Protect & (PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE)) == 0 && mbi.RegionSize <= k_max_region)
					visit(reinterpret_cast<const uint8_t *>(base), mbi.RegionSize);
				if (base + mbi.RegionSize <= address)
					break;
				address = base + mbi.RegionSize;
			}
		}

		// Headers in a region (4-byte aligned "LANG" + the rest of the magic). Returns how many were stored.
		size_t scan_region(const uint8_t *base, size_t size, const uint8_t **out, size_t capacity)
		{
			size_t found = 0;
			__try
			{
				const auto *words = reinterpret_cast<const uint32_t *>(base);
				const size_t count = size / 4;
				for (size_t i = 0; i + 4 < count && found < capacity; ++i)
					if (words[i] == 0x474E414C && std::memcmp(&words[i], k_magic, sizeof(k_magic)) == 0)
						out[found++] = reinterpret_cast<const uint8_t *>(&words[i]);
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
			}
			return found;
		}

		bool read_block(uint8_t *header, block &b)
		{
			uint8_t magic[sizeof(k_magic)] = {};
			uint32_t fields[3] = {};
			if (!read(header, magic, sizeof(magic)) || std::memcmp(magic, k_magic, sizeof(magic)) != 0 || !read(header + 12, fields, sizeof(fields)))
				return false;
			if (fields[0] == 0 || fields[0] > 200000 || fields[1] != 0x18 || fields[2] <= fields[1])
				return false;
			b.header = header;
			b.count = fields[0];
			b.values = fields[1];
			b.keys = fields[2];
			return true;
		}

		bool still_valid(const block &b)
		{
			block now;
			return read_block(b.header, now) && now.count == b.count && now.keys == b.keys;
		}

		std::vector<block> scan()
		{
			std::vector<block> blocks;
			std::vector<const uint8_t *> candidates(4096);
			for_each_region([&](const uint8_t *base, size_t size) {
				const size_t n = scan_region(base, size, candidates.data(), candidates.size());
				for (size_t i = 0; i < n; ++i)
				{
					block b;
					if (read_block(const_cast<uint8_t *>(candidates[i]), b))
						blocks.push_back(b);
				}
			});
			return blocks;
		}

		uint32_t big_endian(const uint8_t *p)
		{
			return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
		}

		// Address of a key's text in a block (its u16 is 2 bytes before), or null. Keys are sorted.
		uint8_t *find(const block &b, uint32_t key)
		{
			uint32_t low = 0, high = b.count;
			while (low < high)
			{
				const uint32_t middle = low + (high - low) / 2;
				uint8_t entry[8];
				if (!read(b.header + b.keys + 8ull * middle, entry, sizeof(entry)))
					return nullptr;
				const uint32_t k = big_endian(entry);
				if (k == key)
					return b.header + b.values + big_endian(entry + 4) + 2;
				if (k < key)
					low = middle + 1;
				else
					high = middle;
			}
			return nullptr;
		}

		bool read_node(uint8_t *address, node &n)
		{
			return address != nullptr && (reinterpret_cast<uintptr_t>(address) & 0xF) == 0 && read(address, &n, sizeof(n));
		}

		bool in_values(const block &b, const uint8_t *p)
		{
			return p >= b.header + b.values && p < b.header + b.keys;
		}

		// The node of (key, one of the values), walking the sorted list from start.
		uint8_t *walk(uint8_t *start, uint32_t key, const uint8_t *value, const uint8_t *other_value)
		{
			node n;
			uint8_t *p = start;
			for (int steps = 0;; ++steps)
			{
				if (steps > 400000 || !read_node(p, n))
					return nullptr;
				if (n.key == key)
					break;
				p = n.key < key ? n.next : n.prev;
			}
			for (uint8_t *q = p; read_node(q, n) && n.key == key; q = n.next)
				if (n.value == value || (other_value != nullptr && n.value == other_value))
					return q;
			for (uint8_t *q = p; read_node(q, n) && n.key == key; q = n.prev)
				if (n.value == value || (other_value != nullptr && n.value == other_value))
					return q;
			return nullptr;
		}

		// Index nodes in a region: a pointer to one of the wanted values followed by its key.
		void scan_index_region(const uint8_t *base, size_t size, const uint64_t *values, const uint32_t *keys, size_t count,
			uint64_t low, uint64_t high, const uint8_t **found)
		{
			__try
			{
				const auto *words = reinterpret_cast<const uint64_t *>(base);
				const size_t n = size / 8;
				for (size_t i = 2; i + 1 < n; i += 2) // nodes are 16-byte aligned: value at +0x10
				{
					const uint64_t w = words[i];
					if (w < low || w > high)
						continue;
					for (size_t j = 0; j < count; ++j)
						if (w == values[j] && static_cast<uint32_t>(words[i + 1]) == keys[j])
							found[j] = reinterpret_cast<const uint8_t *>(&words[i - 2]);
				}
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
			}
		}

		// Finds one index node of each block that has none yet (one pass over memory, without the lock).
		void find_index(const std::vector<block> &blocks)
		{
			uint64_t values[k_max_anchors] = {};
			uint32_t keys[k_max_anchors] = {};
			uint8_t *headers[k_max_anchors] = {};
			const uint8_t *found[k_max_anchors] = {};
			size_t count = 0;
			uint64_t low = UINT64_MAX, high = 0;
			for (const block &b : blocks)
			{
				uint8_t entry[8];
				if (count == k_max_anchors || !read(b.header + b.keys, entry, sizeof(entry)))
					continue;
				values[count] = reinterpret_cast<uint64_t>(b.header + b.values + big_endian(entry + 4));
				keys[count] = big_endian(entry);
				headers[count] = b.header;
				low = std::min(low, values[count]);
				high = std::max(high, values[count]);
				++count;
			}
			if (count == 0)
				return;
			const ULONGLONG started = GetTickCount64();
			for_each_region([&](const uint8_t *base, size_t size) { scan_index_region(base, size, values, keys, count, low, high, found); });
			size_t anchors = 0;
			std::lock_guard lock(s_mutex);
			for (size_t i = 0; i < count; ++i)
				if (found[i] != nullptr)
				{
					s_anchors[headers[i]] = const_cast<uint8_t *>(found[i]);
					++anchors;
				}
			log_info("Textos: indice do jogo achado para {} de {} blocos ({} ms)", anchors, count, GetTickCount64() - started);
		}

		// Caller holds s_mutex. The index node of a key's text in a block, or null (a search for the index is asked for).
		uint8_t *node_of(const block &b, uint32_t key, relocation &r)
		{
			const uint8_t *ours = r.active >= 0 ? r.slots[r.active] : nullptr;
			node n;
			if (read_node(r.node, n) && n.key == key && (n.value == r.original_value || (ours != nullptr && n.value == ours)))
				return r.node;
			const auto anchor = s_anchors.find(b.header);
			if (anchor != s_anchors.end() && read_node(anchor->second, n) && in_values(b, n.value))
				if (uint8_t *found = walk(anchor->second, key, r.original_value, ours))
					return found;
			s_index_wanted = true;
			return nullptr;
		}

		// Copies' memory is never freed: the game could still be reading one.
		uint8_t *take_slot()
		{
			if (s_pools.empty() || s_pools.back().used + k_slot_size > k_pool_size)
			{
				void *memory = VirtualAlloc(nullptr, k_pool_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
				if (memory == nullptr)
					return nullptr;
				s_pools.push_back({ static_cast<uint8_t *>(memory), 0 });
			}
			uint8_t *slot = s_pools.back().base + s_pools.back().used;
			s_pools.back().used += k_slot_size;
			return slot;
		}

		std::string read_text(const uint8_t *p)
		{
			std::string text;
			char chunk[64];
			while (text.size() < 4096 && read(p + text.size(), chunk, sizeof(chunk)))
			{
				const void *end = std::memchr(chunk, 0, sizeof(chunk));
				if (end != nullptr)
				{
					text.append(chunk, static_cast<const char *>(end) - chunk);
					return text;
				}
				text.append(chunk, sizeof(chunk));
			}
			return text;
		}

		// The game's text at p (remembered the first time it is touched: it is also the room available).
		const std::string &original_at(uint8_t *p, uint32_t key)
		{
			auto it = s_originals.find({ p, key });
			if (it == s_originals.end())
				it = s_originals.emplace(std::pair{ p, key }, read_text(p)).first;
			return it->second;
		}

		// The text as it fits over the original: cut at a UTF-8 boundary, padded with NULs.
		std::string fitted(const std::string &text, size_t room)
		{
			size_t n = std::min(text.size(), room);
			while (n > 0 && n < text.size() && (static_cast<uint8_t>(text[n]) & 0xC0) == 0x80)
				--n;
			std::string bytes(room, '\0');
			std::memcpy(bytes.data(), text.data(), n);
			return bytes;
		}

		void write_if_different(uint8_t *p, const std::string &bytes)
		{
			char current[k_slot_size];
			if (bytes.size() > sizeof(current) || !read(p, current, bytes.size()) || std::memcmp(current, bytes.data(), bytes.size()) != 0)
				write(p, bytes.data(), bytes.size());
		}

		// Points a key's node back at the game's own text (if it still points at a copy of ours).
		void unrelocate(uint32_t key, relocation &r)
		{
			if (r.active < 0)
				return;
			node n;
			if (read_node(r.node, n) && n.key == key && (n.value == r.slots[0] || n.value == r.slots[1]))
				write_pointer(r.node + k_node_value, r.original_value);
			r.active = -1;
		}

		// Caller holds s_mutex. The text goes over the game's, cut to its length; a longer one also goes to a copy of
		// ours that the key's index node points at (until the index is found, the cut text shows).
		bool apply(const block &b, uint32_t key, const std::string &text)
		{
			uint8_t *in_place = find(b, key);
			if (in_place == nullptr)
				return false;
			const std::string original = original_at(in_place, key);
			write_if_different(in_place, fitted(text, original.size()));
			const auto existing = s_relocations.find({ b.header, key });
			if (text.size() <= original.size())
			{
				if (existing != s_relocations.end())
					unrelocate(key, existing->second);
				return true;
			}
			relocation &r = s_relocations[{ b.header, key }];
			r.original_value = in_place - 2;
			std::string bytes(2, '\0');
			read(in_place - 2, bytes.data(), 2); // the u16 before the game's text
			bytes += fitted(text, k_slot_size - 3).c_str();
			bytes.push_back('\0');
			uint8_t *at = node_of(b, key, r);
			if (at == nullptr)
				return true;
			r.node = at;
			if (r.active >= 0)
			{
				char current[k_slot_size];
				node n;
				if (read_node(at, n) && n.value == r.slots[r.active] && read(r.slots[r.active], current, bytes.size()) &&
					std::memcmp(current, bytes.data(), bytes.size()) == 0)
					return true; // already showing
			}
			const int next = r.active == 0 ? 1 : 0;
			if (r.slots[next] == nullptr)
				r.slots[next] = take_slot();
			if (r.slots[next] == nullptr)
				return true;
			write(r.slots[next], bytes.data(), bytes.size());
			write_pointer(at + k_node_value, r.slots[next]);
			r.active = next;
			return true;
		}

		// Caller holds s_mutex.
		bool replace_all(uint32_t key, const std::string &text)
		{
			bool any = false;
			for (const block &b : s_blocks)
				if (apply(b, key, text))
					any = true;
			return any;
		}

		void background()
		{
			const quiet_faults quiet;
			ULONGLONG last_scan = 0;
			ULONGLONG last_index = 0;
			ULONGLONG interval = k_missing_rescan_ms; // grows while searches for a missing key find nothing new
			for (;;)
			{
				Sleep(1000);
				bool rescan = false;
				bool missing = s_rescan.load(); // a key was asked for and not found
				{
					std::lock_guard lock(s_mutex);
					for (const block &b : s_blocks)
						if (!still_valid(b))
							rescan = true;
					for (const auto &[key, text] : s_persistent)
						if (!replace_all(key, text))
							missing = true;
				}
				const ULONGLONG now = GetTickCount64();
				if (missing && now - last_scan >= interval)
					rescan = true;
				if (!rescan)
				{
					if (s_index_wanted && now - last_index >= k_index_rescan_ms)
					{
						s_index_wanted = false;
						last_index = now;
						std::vector<block> blocks;
						{
							std::lock_guard lock(s_mutex);
							for (const block &b : s_blocks)
								if (!s_anchors.contains(b.header))
									blocks.push_back(b);
						}
						find_index(blocks);
					}
					continue;
				}
				last_scan = now;
				s_rescan = false;
				std::vector<block> blocks = scan();
				const ULONGLONG elapsed = GetTickCount64() - now;
				{
					std::lock_guard lock(s_mutex);
					if (blocks.size() != s_blocks.size())
					{
						log_info("Textos: {} blocos de idioma na memoria (busca em {} ms)", blocks.size(), elapsed);
						interval = k_missing_rescan_ms;
					}
					else
					{
						interval = std::min<ULONGLONG>(interval * 2, 120000);
					}
					s_blocks = blocks;
					// Forget what belonged to blocks that went away (copies' memory is kept, see take_slot).
					const auto gone = [](const uint8_t *header) {
						return std::none_of(s_blocks.begin(), s_blocks.end(), [header](const block &b) { return b.header == header; });
					};
					std::erase_if(s_originals, [](const auto &entry) {
						const uint8_t *p = entry.first.first;
						return std::none_of(s_blocks.begin(), s_blocks.end(), [p](const block &b) { return p >= b.header && p < b.header + b.keys; });
					});
					std::erase_if(s_relocations, [&](const auto &entry) { return gone(entry.first.first); });
					std::erase_if(s_anchors, [&](const auto &entry) { return gone(entry.first); });
					std::erase_if(blocks, [](const block &b) { return s_anchors.contains(b.header); });
				}
				// The index of new blocks, now, so long texts show the first time they are used.
				last_index = GetTickCount64();
				find_index(blocks);
				std::lock_guard lock(s_mutex);
				for (const auto &[key, text] : s_persistent)
					replace_all(key, text);
			}
		}
	}

	void start()
	{
		std::thread(background).detach();
	}

	bool set(uint32_t key, const std::string &text)
	{
		const quiet_faults quiet;
		std::lock_guard lock(s_mutex);
		const bool found = replace_all(key, text);
		if (!found)
			s_rescan = true;
		return found;
	}

	void set_persistent(uint32_t key, const std::string &text)
	{
		const quiet_faults quiet;
		std::lock_guard lock(s_mutex);
		s_persistent[key] = text;
		if (!replace_all(key, text))
			s_rescan = true;
	}

	void restore(uint32_t key)
	{
		const quiet_faults quiet;
		std::lock_guard lock(s_mutex);
		s_persistent.erase(key);
		for (const block &b : s_blocks)
		{
			uint8_t *p = find(b, key);
			if (p == nullptr)
				continue;
			const auto relocated = s_relocations.find({ b.header, key });
			if (relocated != s_relocations.end())
				unrelocate(key, relocated->second);
			const auto it = s_originals.find({ p, key });
			if (it != s_originals.end())
			{
				const std::string bytes = fitted(it->second, it->second.size());
				write(p, bytes.data(), bytes.size());
			}
		}
	}

	std::string original(uint32_t key)
	{
		const quiet_faults quiet;
		std::lock_guard lock(s_mutex);
		for (const block &b : s_blocks)
			if (uint8_t *p = find(b, key))
				return original_at(p, key);
		s_rescan = true; // maybe in a block loaded since the last search
		return {};
	}
}
