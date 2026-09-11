// smx_dynamic.h
#pragma once
#include "configure.h"

namespace smx {

using std::int8_t;
using std::int16_t;
using std::int32_t;
using std::int64_t;
using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;

using string_type = std::string;
using hash_type = std::uint64_t;
template <typename T> using vector_type = std::vector<T>;
using buffer_type = vector_type<uint8_t>;

using bound_guard_type  = std::function<bool(const void* ev)>;
using bound_action_type = std::function<void(const void* ev)>;

//==============================================================================
// 配布バイナリのヘッダ（20 バイト固定・raw）。本体（Authored 系）の前に置く。
//   ルール1: magic / format_version 不一致 → 拒否
//   ルール2: data_version の差異は本体側で吸収（フィールド追加はマイナー運用）
//   ルール3: signature_hash 不一致 → 詳細照合へフォールバック（合成方法は PoC 後。0=未検証）
//==============================================================================
constexpr uint32_t kBinMagic         = 0x44584D53; // "SMXD" (little-endian)。エンディアン誤読の検出も兼ねる
constexpr uint16_t kBinFormatVersion = 1;          // コンテナ形式。壊れる変更でのみ +1
constexpr uint16_t kBinDataVersion   = 1;          // ペイロード（AuthoredMachine）構造
constexpr uint32_t kBinHeaderSize    = 20;         // 本体開始オフセット（ヘッダ拡張はここを伸ばす）

struct BinHeader {
	uint32_t magic          = kBinMagic;
	uint16_t format_version = kBinFormatVersion;
	uint16_t data_version   = kBinDataVersion;
	uint32_t header_size    = kBinHeaderSize;
	uint64_t signature      = 0;                   // 参照シンボル集合の合成ハッシュ（0=未検証）
};

//==============================================================================
// 1. ハッシュ基盤 (name_hash)
//    正規名 -> 64bit。pack 相当と実行時エンコーダが同じ関数を共有し同値を保証する。
//    FNV-1a (64bit)。constexpr なのでコンパイル時にも実行時にも評価できる。
//==============================================================================
constexpr hash_type name_hash(std::string_view s)
{
	hash_type h = 1469598103934665603ull; // FNV offset basis
	for (char c: s)
		h = (h ^ static_cast<unsigned char>(c)) * 1099511628211ull; // FNV prime
	return h;
}

// 予約ハッシュ（状態機械の内部イベント）
constexpr hash_type kEntry = name_hash("#entry"); // on_entry
constexpr hash_type kExit  = name_hash("#exit");  // on_exit
constexpr hash_type kAnon  = name_hash("#anon");  // 無イベント(anonymous)遷移
constexpr hash_type kNone  = 0; // 「状態なし / 遷移なし」を表す番兵

// 文字列リテラル -> ハッシュ の UDL（例: "st::Foo"_h）。コンパイル時ハッシュ。
constexpr hash_type operator""_h(const char* s, std::size_t n)
{
	return name_hash(std::string_view{ s, n });
}

// basis を種にバイト列を混ぜて 1 つの Id にする（FNV-1a 継続）。
// ガードキャッシュのキー（guard hash + 固定引数）合成に使う。args が空なら basis のまま。
inline hash_type mix_bytes(hash_type basis, const buffer_type& bytes)
{
	hash_type h = basis;
	for (uint8_t b: bytes)
		h = (h ^ b) * 1099511628211ull;
	return h;
}

//==============================================================================
// 2. バイナリ形式 (軽量 Reader / Writer)
//    引数 blob やイベント payload の直列化に使う。エンディアンは当面ホスト固定。
//==============================================================================
struct BinReader {
	const uint8_t* p = nullptr;
	const uint8_t* end = nullptr;

	BinReader() = default;
	BinReader(const buffer_type& b)
		: p(b.data()), end(b.data() + b.size())
	{
	}

	template <typename T>
	T read()
	{
		static_assert(std::is_trivially_copyable_v<T>, "POD only");
		T v {};
		if (p + sizeof(T) <= end) {
			std::memcpy(&v, p, sizeof(T));
			p += sizeof(T);
		}
		return v;
	}
};


//==============================================================================
// 4. オーサリング（データ）形式：文字列名 + occurrence 固有の固定引数。
//==============================================================================
// ガード式：Leaf（束縛対象のガード）を And / Or / Not で組み合わせる再帰値ツリー。
//   デフォルト（空の And）＝常に true ＝「ガードなし」。旧 negate は Not ノードで表す。
//   自己再帰 vector は AuthoredState.regions と同じパターン。
struct GuardExpr {
	enum class Op: uint8_t { And = 0, Or, Not, Leaf };
	Op op = Op::And;
	hash_type name = kNone;                 // Leaf のみ
	buffer_type args {};                    // Leaf のみ
	vector_type<GuardExpr> children {};     // And/Or: N個（n-ary）/ Not: 1個
};

// オーサリング糖衣。SML の [A && !B] とほぼ同じ見た目で書ける：
//   .guard = guard("ft_A"_h) && !guard("ft_B"_h, pack(1))
inline GuardExpr guard(hash_type name, buffer_type args = {})
{
	return GuardExpr{ GuardExpr::Op::Leaf, name, std::move(args), {} };
}
inline GuardExpr operator&&(GuardExpr a, GuardExpr b)
{
	GuardExpr e{ GuardExpr::Op::And };
	auto add = [&e](GuardExpr&& x) {
		if (x.op == GuardExpr::Op::And)     // And の子は展開（平坦化。空 And=true は消える）
			for (auto& c: x.children)
				e.children.push_back(std::move(c));
		else
			e.children.push_back(std::move(x));
	};
	add(std::move(a));
	add(std::move(b));
	return e;
}
inline GuardExpr operator||(GuardExpr a, GuardExpr b)
{
	GuardExpr e{ GuardExpr::Op::Or };
	auto add = [&e](GuardExpr&& x) {
		if (x.op == GuardExpr::Op::Or)      // Or の子は展開（平坦化。空 Or=false は消える）
			for (auto& c: x.children)
				e.children.push_back(std::move(c));
		else
			e.children.push_back(std::move(x));
	};
	add(std::move(a));
	add(std::move(b));
	return e;
}
inline GuardExpr operator!(GuardExpr a)
{
	GuardExpr e{ GuardExpr::Op::Not };
	e.children.push_back(std::move(a));
	return e;
}

struct AuthoredAction {
	hash_type name = kNone;
	buffer_type args {};
};

// 状態からの遷移。from は所属する AuthoredState なので不要。
struct AuthoredTransition {
	hash_type event = kNone;              // "#anon"_h（＝kAnon）で無イベント遷移
	GuardExpr guard {};                    // ガード式（省略＝常に true）
	vector_type<AuthoredAction> actions {};
	hash_type target = kNone;             // 省略（kNone）で内部遷移（状態変更なし）
};

struct AuthoredMachine; // fwd

// 状態＝atomic / 合成（regions を持つ）。regions が再帰の要。
struct AuthoredState {
	hash_type name = kNone;
	bool initial = false;                 // *state（このリージョンの初期状態）
	vector_type<AuthoredAction> entry {}; // on_entry
	vector_type<AuthoredAction> exit {};  // on_exit
	vector_type<AuthoredTransition> on {};// この状態からのイベント遷移
	vector_type<AuthoredMachine> regions {}; // 0=atomic / 1=階層 / N=直交（各要素が1リージョン）
};

// ＝1リージョン。状態の集合。initial は各 state.initial で表現。
struct AuthoredMachine {
	vector_type<AuthoredState> states;
};

//==============================================================================
// 5. コンパイル済み形式：hash と「束縛済み callable」を持つ。実行時はこれだけを使う。
//==============================================================================
// GuardExpr をミラーしたツリー。Leaf だけ束縛済み callable + キャッシュ slot を持つ。
struct CompiledGuard {
	GuardExpr::Op op = GuardExpr::Op::And;
	bound_guard_type fn;      // Leaf のみ：静的引数束縛済み。呼ぶだけ
	int slot = -1;            // Leaf のみ：ガードキャッシュ index。同ガード同引数は同 slot を共有
	vector_type<CompiledGuard> children;
};

struct CompiledTransition {
	hash_type state;
	hash_type event;
	CompiledGuard guard;      // ガード式（空 And＝常に true）
	vector_type<bound_action_type> actions;  // 静的引数束縛済み。順に呼ぶ
	hash_type next;
};


struct Node; // fwd
struct SubRegions {
	hash_type state;			// この合成状態が活性のとき
	vector_type<Node> children;	// K 本の子リージョン
};

struct Node {
	vector_type<CompiledTransition>	transitions;	// このリージョンの遷移
	vector_type<SubRegions>	composite;				// 合成状態 hash -> 子リージョン群
	hash_type initial_ = kNone;						// このリージョンの初期状態（*state）
	hash_type state_ = kNone;
	mutable vector_type<uint8_t> guard_cache_;

	void invalidate()
	{
		std::fill(guard_cache_.begin(), guard_cache_.end(), uint8_t{ 0 });
	}

	// state_ が合成状態なら、その子リージョン群を返す（leaf は nullptr）。
	SubRegions* active()
	{
		for (auto& s: composite)
			if (s.state == state_)
				return &s;
		return nullptr;
	}

	// ガード式ツリーの評価。And/Or は短絡（C++/SML の &&/|| と同じ）。
	//   キャッシュは Leaf 単位。短絡でスキップされた Leaf は未評価のまま。
	bool eval_guard(const CompiledGuard& g, const void* ev)
	{
		switch (g.op) {
		case GuardExpr::Op::Leaf: {
			uint8_t c = guard_cache_[g.slot];
			if (c != 0)
				return c == 2;
			if (!g.fn)
				return false;
			bool r = g.fn(ev);
			guard_cache_[g.slot] = r ? 2 : 1;
			return r;
		}
		case GuardExpr::Op::And:              // 空 And = true（ガードなし）
			for (const auto& c: g.children)
				if (!eval_guard(c, ev))
					return false;
			return true;
		case GuardExpr::Op::Or:               // 空 Or = false
			for (const auto& c: g.children)
				if (eval_guard(c, ev))
					return true;
			return false;
		case GuardExpr::Op::Not:
			return g.children.empty() ? false : !eval_guard(g.children[0], ev);
		}
		return false;
	}

	void run_actions(const CompiledTransition& tr, const void* ev)
	{
		for (const auto& a: tr.actions) {
			if (!a)
				continue;
			a(ev);
			invalidate();
		}
	}

	// このノードの内部イベント(#entry/#exit)。アクションのみ。
	void fire_state_event(hash_type st, hash_type ev_hash)
	{
		invalidate();
		for (const auto& tr: transitions) {
			if (tr.state != st || tr.event != ev_hash)
				continue;
			if (eval_guard(tr.guard, nullptr))
				run_actions(tr, nullptr);
		}
	}

	// 状態 s へ入る：このノードの #entry → 子リージョン群を start（外→内）。
	void enter(hash_type s)
	{
		state_ = s;
		fire_state_event(s, kEntry);
		if (SubRegions* a = active())
			for (Node& c: a->children)
				c.start();
	}

	// 現状態から抜ける：子リージョンを stop（内→外）→ このノードの #exit。
	void leave()
	{
		if (SubRegions* a = active())
			for (Node& c: a->children)
				c.stop();
		fire_state_event(state_, kExit);
	}

	// このリージョンの初期状態へ入る（*state 相当）。
	void start()
	{
		if (initial_ != kNone)
			enter(initial_);
	}

	void stop()
	{
		leave();
		state_ = kNone;
	}

	// 自ノードの遷移を1つだけ発火（子は dispatch 側が先に処理済み）。
	// 実行順 guard → exit(内→外) → action → entry(外→内)。
	bool step(hash_type event, const void* ev)
	{
		invalidate();
		for (const auto& tr: transitions) {
			if (tr.state != state_ || tr.event != event)
				continue;
			if (!eval_guard(tr.guard, ev))
				continue;
			const bool external = (tr.next != kNone);
			if (external)
				leave();
			run_actions(tr, ev);
			if (external)
				enter(tr.next);
			return true;
		}
		return false;
	}

	// deeper-wins バブリング：活性な子リージョン全部に先に配り(直交)、
	// 誰も処理しなければ自ノードの遷移を試す。
	bool dispatch(hash_type event, const void* ev)
	{
		bool handled = false;
		if (SubRegions* a = active())
			for (Node& c: a->children)
				handled |= c.dispatch(event, ev);
		if (!handled)
			handled = step(event, ev);
		return handled;
	}
};

////////////////////////////////////////////////////////////////////////////////

// CRTP 基盤：汎用ロジック（roots 管理・start/settle・process_event・compile_machine）を提供。
// SM 固有部分（event_hash / bind_action / bind_guard）は Derived が用意する。
//   Derived に必要な静的メンバー：
//     template<class E> static hash_type event_hash();
//     template<class F>  static std::optional<bound_action_type> bind_action(hash_type, F&, BinReader&);
//     template<class F>  static std::optional<bound_guard_type>  bind_guard (hash_type, F&, BinReader&);
template <class Derived>
struct ExecutorBase {
	// トップのリージョン群。1本＝通常のマシン、N本＝トップが直交（ラッパ状態 @root は不要）。
	vector_type<Node> roots;

	bool start()
	{
		for (auto& r: roots)
			r.start();
		settle();
		if (roots.empty())
			return false;
		for (auto& r: roots)
			if (r.state_ == kNone)
				return false;
		return true;
	}

	// 無イベント(anonymous)遷移をツリー全体で落ち着くまで連鎖。
	void settle()
	{
		bool any = true;
		while (any) {
			any = false;
			for (auto& r: roots)
				any |= r.dispatch(kAnon, nullptr);
		}
	}

	// ライブイベントを型ポインタのまま全リージョンへ配送（直交）。ハッシュは Derived が解決。
	template <class E>
	void process_event(const E& e)
	{
		hash_type h = Derived::template event_hash<E>();
		for (auto& r: roots)
			r.dispatch(h, &e);
		settle();
	}

	// 状態中心の AuthoredMachine を再帰的にコンパイルして result に構築。
	// 未登録の action/guard 名（Derived::bind_* が見つからない）や子リージョンの失敗があれば false を返す。
	// features 型 F はテンプレート：Derived の bind_* が目的の feature を取り出せれば何でも渡せる。
	template <class F>
	static bool compile_machine(Node& result, const AuthoredMachine& m, F feats)
	{
		// Authored の名前は "..."_h で既にハッシュ済み。ここでの name_hash は不要。

		// cache_key(=mix_bytes(guard_hash, args)) -> slot。distinct な (ガード,引数) に連番を割当。
		std::unordered_map<hash_type, int> slot_of;

		// action 列 -> 束縛済み callable 列（未登録なら false）。entry/exit/on から使う。
		auto bind_actions = [&](const vector_type<AuthoredAction>& src,
			vector_type<bound_action_type>& dst) -> bool {
			for (auto& a: src) {
				BinReader reader(a.args);
				auto p = Derived::bind_action(a.name, feats, reader);
				if (!p)
					return false;
				dst.push_back(std::move(*p));
			}
			return true;
		};

		// ガード式 -> CompiledGuard（ツリーをミラー。Leaf を束縛し、キャッシュ slot を intern）。
		std::function<bool(const GuardExpr&, CompiledGuard&)> compile_guard =
			[&](const GuardExpr& src, CompiledGuard& dst) -> bool {
			dst.op = src.op;
			if (src.op == GuardExpr::Op::Leaf) {
				BinReader reader(src.args);
				auto p = Derived::bind_guard(src.name, feats, reader);
				if (!p)
					return false; // 未登録ガード → エラー
				dst.fn = std::move(*p);
				hash_type ck = mix_bytes(src.name, src.args);
				dst.slot = slot_of.try_emplace(ck, static_cast<int>(slot_of.size())).first->second;
				return true;
			}
			for (const auto& c: src.children) {
				CompiledGuard cc;
				if (!compile_guard(c, cc))
					return false;
				dst.children.push_back(std::move(cc));
			}
			return true;
		};

		for (const auto& s: m.states) {
			const hash_type sh = s.name;
			if (s.initial)
				result.initial_ = sh;

			// entry / exit は #entry / #exit の内部遷移（アクションのみ）に落とす。
			if (!s.entry.empty()) {
				CompiledTransition tr;
				tr.state = sh; tr.event = kEntry; tr.next = kNone;
				if (!bind_actions(s.entry, tr.actions))
					return false;
				result.transitions.push_back(std::move(tr));
			}
			if (!s.exit.empty()) {
				CompiledTransition tr;
				tr.state = sh; tr.event = kExit; tr.next = kNone;
				if (!bind_actions(s.exit, tr.actions))
					return false;
				result.transitions.push_back(std::move(tr));
			}

			// この状態からのイベント遷移。
			for (const auto& t: s.on) {
				CompiledTransition tr;
				tr.state = sh;
				tr.event = t.event;      // 既にハッシュ（kAnon 含む）
				tr.next  = t.target;     // 既にハッシュ（kNone = 内部遷移）
				if (!compile_guard(t.guard, tr.guard))
					return false;
				if (!bind_actions(t.actions, tr.actions))
					return false; // 未登録アクション → エラー
				result.transitions.push_back(std::move(tr));
			}

			// 合成状態：子リージョン群を再帰コンパイル（0=atomic なので何もしない）。
			if (!s.regions.empty()) {
				SubRegions sr;
				sr.state = sh;
				for (const auto& rm: s.regions) {
					Node child;
					if (!compile_machine(child, rm, feats)) // 子リージョンの失敗を伝播
						return false;
					sr.children.push_back(std::move(child));
				}
				result.composite.push_back(std::move(sr));
			}
		}

		result.guard_cache_.assign(slot_of.size(), uint8_t{ 0 }); // slot 数ぶんのフラット配列
		return true;
	}

	// トップのリージョン配列（＝単一データ）から実行可能な Executor を構築。
	//   通常マシン＝要素1個、トップ直交＝要素N個（ラッパ状態は不要）。
	//   いずれかのリージョンのコンパイルに失敗したら std::nullopt を返す。
	template <class F>
	static std::optional<Derived> compile(const vector_type<AuthoredMachine>& regions, F feats)
	{
		Derived ex;
		for (const auto& m: regions) {
			Node n;
			if (!compile_machine(n, m, feats))
				return std::nullopt;
			ex.roots.push_back(std::move(n));
		}
		return ex;
	}
};


// string フィールドの復元（pack("...") と対）。0終端バイト列（sz）として読む。
inline string_type read_string(BinReader& r)
{
	string_type s;
	while (r.p < r.end) {
		uint8_t c = r.read<uint8_t>();
		if (c == 0)
			break;
		s.push_back(static_cast<char>(c));
	}
	return s;
}

// string_type は非POD（memcpy不可）なので BinReader::read<T>() の汎用実装(POD専用)は使えない。
// 生成コード(abc.h等)が args.read<string_type>() の形で呼び出すため、read_string と等価な特殊化を用意する。
template <>
inline string_type BinReader::read<string_type>()
{
	return read_string(*this);
}

inline buffer_type bin_read_bytes(BinReader& r)
{
	uint32_t n = r.read<uint32_t>();
	buffer_type b;
	b.reserve(n);
	for (uint32_t i = 0; i < n; ++i)
		b.push_back(r.read<uint8_t>());
	return b;
}

// --- 読み込み（overload + 可変長リスト） ---
inline void bin_read(BinReader& r, AuthoredAction& a)
{
	a.name = r.read<hash_type>();
	a.args = bin_read_bytes(r);
}
inline void bin_read(BinReader& r, GuardExpr& g) // 再帰（bin_write と対）
{
	g.op = static_cast<GuardExpr::Op>(r.read<uint8_t>());
	if (g.op == GuardExpr::Op::Leaf) {
		g.name = r.read<hash_type>();
		g.args = bin_read_bytes(r);
	} else {
		uint32_t n = r.read<uint32_t>();
		for (uint32_t i = 0; i < n && r.p < r.end; ++i) {
			GuardExpr c;
			bin_read(r, c);
			g.children.push_back(std::move(c));
		}
	}
}
template <class T>
inline void bin_read_list(BinReader& r, vector_type<T>& v)
{
	uint32_t n = r.read<uint32_t>();
	v.clear();
	v.reserve(n);
	for (uint32_t i = 0; i < n; ++i) {
		T e;
		bin_read(r, e);
		v.push_back(std::move(e));
	}
}
inline void bin_read(BinReader& r, AuthoredTransition& t)
{
	t.event = r.read<hash_type>();
	bin_read(r, t.guard);
	bin_read_list(r, t.actions);
	t.target = r.read<hash_type>();
}
inline void bin_read(BinReader& r, AuthoredMachine& m); // fwd（再帰）
inline void bin_read(BinReader& r, AuthoredState& s)
{
	s.name = r.read<hash_type>();
	s.initial = (r.read<uint8_t>() != 0);
	bin_read_list(r, s.entry);
	bin_read_list(r, s.exit);
	bin_read_list(r, s.on);
	bin_read_list(r, s.regions);
}
inline void bin_read(BinReader& r, AuthoredMachine& m)
{
	bin_read_list(r, m.states);
}

// 読み込み＋検証。成功なら r を本体先頭（header_size 位置）へ進める。
//   header_size でジャンプするので、将来ヘッダにフィールドが増えても古い reader は本体を読める。
inline bool bin_read_header(BinReader& r, BinHeader& h)
{
	const uint8_t* base = r.p;
	h.magic          = r.read<uint32_t>();
	h.format_version = r.read<uint16_t>();
	h.data_version   = r.read<uint16_t>();
	h.header_size    = r.read<uint32_t>();
	h.signature      = r.read<uint64_t>();
	if (h.magic != kBinMagic || h.format_version != kBinFormatVersion)
		return false; // ルール1：拒否
	if (h.header_size < kBinHeaderSize)
		return false; // 壊れた/切り詰められたヘッダ
	r.p = base + h.header_size; // 未知のヘッダ拡張フィールドを読み飛ばして本体へ
	return r.p <= r.end;
}



inline AuthoredMachine from_binary(const buffer_type& bytes, BinHeader* out_header = nullptr)
{
	BinReader r(bytes);
	BinHeader h;
	if (!bin_read_header(r, h))
		return {}; // ヘッダ不一致＝拒否（states 空）
	if (out_header)
		*out_header = h;
	AuthoredMachine m;
	bin_read(r, m);
	return m;
}


} // namespace smx
