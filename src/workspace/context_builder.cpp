
#include "workspace/context_builder.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

#include "workspace/search_rg.h"
#include "workspace/java/locator.h"
#include "workspace/java/extractor.h"
#include "workspace/java/dep_harvest.h"
#include "workspace/java/snippet_from_hit.h"


static bool path_is_main_java(const std::string &p)
{
    return p.find("/src/main/java/") != std::string::npos;
}

static int score_snippet(const std::string &anchor_rel, const HitSnippet &snip)
{
    int score = 0;

    if (snip.kind == "method_declaration" || snip.kind == "constructor_declaration") {
        score += 50;
    } else if (snip.kind.find("class") != std::string::npos || snip.kind.find("interface") != std::string::npos) {
        score += 30;
    }

    if (path_is_main_java(snip.rel_path)) {
        score += 20;
    }

    size_t slash = anchor_rel.rfind('/');
    if (slash != std::string::npos) {
        std::string dir = anchor_rel.substr(0, slash + 1);
        if (snip.rel_path.rfind(dir, 0) == 0) {
            score += 20;
        }
    }

    size_t len = snip.end > snip.start ? (snip.end - snip.start) : 0;
    if (len > 8000) {
        score -= 20;
    }
    if (len > 20000) {
        score -= 60;
    }

    return score;
}

static std::string make_snip_key(const HitSnippet &sn)
{
    return sn.rel_path + ":" + std::to_string(sn.start) + ":" + std::to_string(sn.end);
}

static std::string regex_for_symbol_call(const std::string &sym)
{
    // \bSYM\s*\(
    return "\\b" + sym + "\\s*\\(";
}


ContextPack build_context_pack(const ContextRequest &req,
                               const ContextOptions &opt,
                               const std::vector<FileEntry> &files)
{
    ContextPack pack;

    // Resolve anchor class file.
    std::unique_ptr<JavaLocator> locator = make_text_java_locator(files);
    ClassLocation loc = locator->locate_class(req.anchor_class_fqcn);
    if (!loc.found) {
        pack.stats.hops_used = 0;
        return pack;
    }

    // Extract anchor method.
    Method anchor = extract_method_from_file(loc.abs_path, loc.rel_path, req.anchor_method);
    if (!anchor.found) {
        pack.stats.hops_used = 0;
        return pack;
    }

    // Add anchor snippet first.
    if (opt.include_anchor_in_snippets) {
        ContextSnippet s;
        s.rel_path = loc.rel_path;
        s.abs_path = loc.abs_path;
        s.kind = "method_declaration";
        s.start = anchor.start;
        s.end = anchor.end;
        s.score = 1000;
        s.hop = 0;
        s.symbol = "ANCHOR";
        s.text = anchor.text;
        pack.snippets.push_back(std::move(s));
        pack.stats.snippets_written += 1;
        pack.stats.bytes_written += static_cast<int>(anchor.text.size());
    }

    struct Pending
    {
        std::string rel_path;
        std::string abs_path;
        std::string kind;
        size_t start = 0;
        size_t end = 0;
    };

    std::vector<Pending> frontier;
    frontier.push_back(Pending{loc.rel_path, loc.abs_path, "method_declaration", anchor.start, anchor.end});

    std::unordered_set<std::string> seen_snips;
    seen_snips.reserve(512);

    // Prevent re-expanding the exact same symbol at the same hop too much.
    std::unordered_set<std::string> seen_symbols;
    seen_symbols.reserve(512);

    for (int hop = 0; hop < opt.max_hops; hop++) {
        if (pack.stats.snippets_written >= opt.max_snippets || pack.stats.bytes_written >= opt.max_bytes) {
            break;
        }

        std::vector<Pending> next_frontier;

        for (const Pending &p : frontier) {
            if (pack.stats.snippets_written >= opt.max_snippets || pack.stats.bytes_written >= opt.max_bytes) {
                break;
            }

            if (!(p.kind == "method_declaration" || p.kind == "constructor_declaration")) {
                continue;
            }

            std::vector<std::string> callees = harvest_callees_in_range(p.abs_path, p.start, p.end);

            if (static_cast<int>(callees.size()) > opt.max_symbols_per_method) {
                callees.resize(static_cast<size_t>(opt.max_symbols_per_method));
            }

            for (const std::string &sym : callees) {
                if (pack.stats.snippets_written >= opt.max_snippets || pack.stats.bytes_written >= opt.max_bytes) {
                    break;
                }

                pack.stats.symbols_seen += 1;

                // Avoid exploding on repeated symbols.
                std::string sym_key = std::to_string(hop) + ":" + sym;
                if (!seen_symbols.insert(sym_key).second) {
                    continue;
                }

                RgQuery q;
                q.pattern = regex_for_symbol_call(sym);
                q.fixed_string = false;
                q.globs = req.globs;
                q.excludes = req.excludes;

                pack.stats.rg_queries += 1;

                RgResult rr = rg_search_json(req.repo_root, q);
                if (rr.exit_code == 2) {
                    continue;
                }

                pack.stats.rg_hits_total += static_cast<int>(rr.hits.size());

                size_t take = rr.hits.size();
                if (take > static_cast<size_t>(opt.max_rg_hits_per_symbol)) {
                    take = static_cast<size_t>(opt.max_rg_hits_per_symbol);
                }

                struct Cand
                {
                    HitSnippet snip;
                    int score = 0;
                };

                std::vector<Cand> cands;
                cands.reserve(take);

                for (size_t i = 0; i < take; i++) {
                    const RgHit &h = rr.hits[i];
                    HitSnippet sn = snippet_from_hit(h.abs_path, h.rel_path, h.match_byte_offset);
                    if (!sn.found) {
                        continue;
                    }

                    std::string key = make_snip_key(sn);
                    if (seen_snips.find(key) != seen_snips.end()) {
                        continue;
                    }

                    Cand c;
                    c.score = score_snippet(loc.rel_path, sn);
                    c.snip = std::move(sn);
                    cands.push_back(std::move(c));
                }

                if (cands.empty()) {
                    continue;
                }

                std::sort(cands.begin(), cands.end(),
                          [](const Cand &a, const Cand &b)
                          {
                              return a.score > b.score;
                          });

                int emit_count = opt.max_snippets_per_symbol;
                if (emit_count < 1) {
                    emit_count = 1;
                }

                for (int k = 0; k < emit_count && k < static_cast<int>(cands.size()); k++) {
                    const Cand &best = cands[static_cast<size_t>(k)];

                    std::string key = make_snip_key(best.snip);
                    seen_snips.insert(key);

                    ContextSnippet s;
                    s.rel_path = best.snip.rel_path;
                    s.abs_path = best.snip.abs_path;
                    s.kind = best.snip.kind;
                    s.start = best.snip.start;
                    s.end = best.snip.end;
                    s.score = best.score;
                    s.hop = hop + 1;
                    s.symbol = sym;
                    s.text = best.snip.text;

                    if (pack.stats.bytes_written + static_cast<int>(s.text.size()) > opt.max_bytes) {
                        break;
                    }

                    pack.snippets.push_back(std::move(s));
                    pack.stats.snippets_written += 1;
                    pack.stats.bytes_written += static_cast<int>(pack.snippets.back().text.size());

                    // Expand further if this is a method/ctor.
                    if (best.snip.kind == "method_declaration" || best.snip.kind == "constructor_declaration") {
                        Pending np;
                        np.rel_path = best.snip.rel_path;
                        np.abs_path = best.snip.abs_path;
                        np.kind = best.snip.kind;
                        np.start = best.snip.start;
                        np.end = best.snip.end;
                        next_frontier.push_back(std::move(np));
                    }

                    if (pack.stats.snippets_written >= opt.max_snippets || pack.stats.bytes_written >= opt.max_bytes) {
                        break;
                    }
                }
            }
        }

        frontier.swap(next_frontier);
        pack.stats.hops_used = hop + 1;

        if (frontier.empty()) {
            break;
        }
    }

    return pack;
}

