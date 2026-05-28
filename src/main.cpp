#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory_resource>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ninja_build_stats {

using mem_res = std::pmr::memory_resource;

auto operator""_MB(uint64_t v) -> uint64_t { return 1024 * 1024 * v; }

struct context {
	mem_res* mem = nullptr;
};

struct cmdline_args {
	std::pmr::string ninja_log;
	uint64_t count = 10;
};

struct spec {
	std::filesystem::path ninja_log;
	uint64_t count = 10;
};

struct obj {
	std::chrono::time_point<std::chrono::steady_clock> beg;
	std::chrono::time_point<std::chrono::steady_clock> end;
	std::chrono::time_point<std::chrono::steady_clock> timestamp;
	std::pmr::string path;
	std::pmr::string hash;
};

using svspan = std::span<const std::string_view>;

auto print_help() -> void {
	std::cout << "Usage: ./ninja-build-stats ../path/to/.ninja_log [line count]\n";
}

[[nodiscard]]
auto duration(const obj& v) -> std::chrono::milliseconds {
	return std::chrono::duration_cast<std::chrono::milliseconds>(v.end - v.beg);
}

[[nodiscard]]
auto to_u64(std::string_view text) -> std::optional<uint64_t> {
	auto value = uint64_t{0};
	auto [_, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (ec == std::errc{}) { return value; }
	else                   { return std::nullopt; }
}

[[nodiscard]]
auto parse_cmdline(context* ctx, svspan in) -> std::optional<cmdline_args> {
	auto args = cmdline_args{};
	if (in.size() < 2) {
		print_help();
		return std::nullopt;
	}
	if (in.size() >= 2) { args.ninja_log = std::pmr::string{in[1], ctx->mem}; }
	if (in.size() >= 3) { args.count     = to_u64(in[2]).value_or(args.count); }
	return args;
}

[[nodiscard]]
auto make_spec(const cmdline_args& args) -> spec {
	return {
		{args.ninja_log},
		args.count,
	};
}

[[nodiscard]]
auto read_file(context* ctx, const std::filesystem::path& path) -> std::pmr::string {
	auto file = std::ifstream{path, std::ios::binary | std::ios::ate};
	auto size = file.tellg();
	auto text = std::pmr::string(size, '\0', ctx->mem);
	file.seekg(0);
	file.read(text.data(), size);
	return text;
}

[[nodiscard]]
auto fn_sv_begins_with(std::string_view v) {
	return [v](std::string_view sv) { return sv.starts_with(v); };
}

auto fn_char_is_whitespace = [](char c) { return std::isspace(static_cast<unsigned char>(c)); };
auto fn_char_is_newline    = [](char c) { return c == '\n' || c == '\r'; };
auto fn_sv_is_empty        = [](std::string_view sv) { return sv.empty(); };
auto fn_chunk_to_sv        = [](auto chunk) { return std::string_view{chunk.data(), chunk.size()}; };
auto fn_sv_is_log_comment  = fn_sv_begins_with("#");

auto fn_obj_order_by_duration_asc = [](const obj& a, const obj& b) -> bool {
	return duration(a) < duration(b);
};

auto fn_obj_order_by_duration_desc = [](const obj& a, const obj& b) -> bool {
	return duration(a) > duration(b);
};

[[nodiscard]]
auto trim(std::string_view text) -> std::string_view {
	auto beg = size_t{0};
	auto end = text.size();
	while (beg < text.size() && fn_char_is_whitespace(text[beg])) { beg++; }
	while (end > beg && fn_char_is_whitespace(text[end - 1])) { end--; }
	return text.substr(beg, end - beg);
}

auto fn_trim = [](std::string_view text) { return trim(text); };

[[nodiscard]]
auto view_split_by(auto pred) {
	auto chunk_fn = [pred](char a, char b) { return pred(a) == pred(b); };
	return
		  std::views::chunk_by(chunk_fn)
		| std::views::transform(fn_chunk_to_sv)
		| std::views::transform(fn_trim)
		| std::views::filter(std::not_fn(fn_sv_is_empty));
}

auto view_split_by_whitespace = view_split_by(fn_char_is_whitespace);
auto view_split_by_newline    = view_split_by(fn_char_is_newline);
auto view_log_lines           = view_split_by_newline | std::views::filter(std::not_fn(fn_sv_is_log_comment));

[[nodiscard]]
auto split_by_whitespace(context* ctx, std::string_view text) -> std::pmr::vector<std::string_view> {
	auto parts = std::pmr::vector<std::string_view>{ctx->mem};
	std::ranges::copy(text | view_split_by_whitespace, std::back_inserter(parts));
	return parts;
}

[[nodiscard]]
auto to_time_point(uint64_t timestamp) -> std::chrono::time_point<std::chrono::steady_clock> {
	return std::chrono::time_point<std::chrono::steady_clock>{std::chrono::milliseconds{timestamp}};
}

[[nodiscard]]
auto read_objs(context* ctx, std::span<std::string_view> lines) -> std::pmr::vector<obj> {
	auto objs = std::pmr::vector<obj>{ctx->mem};
	for (const auto line : lines) {
		const auto parts = split_by_whitespace(ctx, line);
		if (parts.size() >= 5) {
			auto beg       = to_u64(parts[0]);
			auto end       = to_u64(parts[1]);
			auto timestamp = to_u64(parts[2]);
			auto path      = std::pmr::string{parts[3], ctx->mem};
			auto hash      = std::pmr::string{parts[4], ctx->mem};
			if (!beg || !end || !timestamp) {
				continue;
			}
			objs.emplace_back(to_time_point(*beg), to_time_point(*end), to_time_point(*timestamp), path, hash);
		}
	}
	return objs;
}

[[nodiscard]]
auto get_log_lines(context* ctx, std::string_view text) -> std::pmr::vector<std::string_view> {
	auto lines = std::pmr::vector<std::string_view>{ctx->mem};
	std::ranges::copy(text | view_log_lines, std::back_inserter(lines));
	return lines;
}

[[nodiscard]]
auto to_string(context* ctx, const ninja_build_stats::obj& obj) -> std::pmr::string {
	auto str = std::pmr::string{ctx->mem};
	auto dur = std::chrono::duration_cast<std::chrono::duration<double>>(duration(obj));
	std::format_to(std::back_inserter(str), "{:>10.3f} seconds | {}", dur.count(), obj.path);
	return str;
}

auto analyze(context* ctx, const ninja_build_stats::spec& spec) -> void {
	auto text  = read_file(ctx, spec.ninja_log);
	auto lines = get_log_lines(ctx, text);
	auto objs  = read_objs(ctx, lines);
	std::ranges::sort(objs, fn_obj_order_by_duration_desc);
	for (const auto& obj : objs | std::views::take(spec.count)) {
		std::cout << to_string(ctx, obj) << "\n";
	}
}

auto main_(context* ctx, svspan in) -> int {
	if (const auto args = parse_cmdline(ctx, in)) {
		const auto spec = make_spec(*args);
		analyze(ctx, spec);
		return 0;
	}
	return 1;
}

[[nodiscard]]
auto make_context(mem_res* mem) -> context {
	return context{
		.mem = mem,
	};
}

[[nodiscard]]
auto make_arg_list(context* ctx, int argc, const char* argv[]) -> std::pmr::vector<std::string_view> {
	auto args_beg = argv;
	auto args_end = std::next(argv, static_cast<std::ptrdiff_t>(argc));
	auto args     = std::pmr::vector<std::string_view>{args_beg, args_end, ctx->mem};
	return args;
}

auto fail(std::string_view what) -> int {
	std::cerr << what << "\n";
	return 1;
}

auto main_(int argc, const char* argv[]) -> int {
	auto mem  = std::pmr::monotonic_buffer_resource{16_MB};
	auto ctx  = make_context(&mem);
	auto args = make_arg_list(&ctx, argc, argv);
	return main_(&ctx, args);
}

auto main(int argc, const char* argv[]) -> int {
	try                               { return main_(argc, argv); }
	catch (const std::exception& err) { return fail(err.what()); }
	catch (...)                       { return fail("Unknown exception."); }
}

} // ninja_build_stats

auto main(int argc, const char* argv[]) -> int {
	return ninja_build_stats::main(argc, argv);
}
