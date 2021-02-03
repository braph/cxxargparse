#include <functional> // std::function
#include <utility>    // std::forward
#include <string>     // std::char_traits
#include <stdexcept>  // std::invalid_argument
#include <cstdlib>    // std::strtoull
#include <limits>     // std::numeric_limits
#include <cinttypes>

/* TODO:
 * - I don't like to use std::function for the action callback as std::function is not constexpr.
 *   Maybe reimplement it? :
 *     void (*callback)(void*);
 *     void *data;
 *
 * - Implement following python-like actions
 *   - store with specialization for the following types:
 *     [u]int{8,16,32,64}_t  << use strtoull()
 *     float, double         << use strod()
 *     [w]char[_t], char*    << use mbstowc/ws
 *     std::filesystem::path << maybe?
 *   - maybe store_string that works with *arbirtary* string types (also QString)
 *   - store_const / store_true / store_false
 *   - append / append_const
 *   - extend
 *   - count
 *   - choices (aaaargh, fuck it?)
 *   - XXX Seems we need to changed has_args to nargs, just like in python.
 *
 * - append/extend should work on arbitrary container types....
 *   I don't know yet how to do this....
 *
 * - Add groups?
 * - Add mutually exclusive groups?
 *
 * - add even parse_intermixed_args()? (fuck my life)
 *
 * - Hm... python lets us use subparsers/subcommands (like git, you know?)
 *   We also want to provide this feature (without bloating up our parser, ofc!)
 *   It should be relatively easy! For each subparser set up a new options array.
 *   the argument_handler() should detect the command and switch to the
 *   commands options array. [well, maybe create a new OptionProvider class for this?)
 *
 * - In argument_handler(): Should we count the number of args and pass them to
 *   argument_handler(current_arg, arg_no)? Then the user wouldn't have to count...
 *   seems sensible <<< nope. or maybe. but we need add_argument() behavior from python...
 *
 * - Although `help` and `version` actions should not be part of this lib
 *   we should think about if this is also solvable in a clean way...
 *
 * - Oh oh oh I have an idea!!! Since the action objects are holding
 *   references we can read out the default values!
 *
 * - Currently Options are stored inside an array. That's cool.
 *   But maybe we could create a wrapper class? That would be a bit complicated if
 *   we want to stay constexpr.... damn.
 *
 * - Detection of duplicate options, *at least in debug (#ifndef NDEBUG)*
 *
 * - Although it seems a bit hacky I really would like to use embedded \0 for long/short options:
 *   --foo -f would become Option("foo\0f",...) 
 *
 * - Testing the parser!!! (Maybe write test cases)
 *   Following cases should be covered...
 *     --arg X, --arg=X
 *     --flag, -f, -blfa
 */

namespace CommandLine {

// ============================================================================
// Exceptions
// ============================================================================

using Exception = std::invalid_argument;

// ============================================================================
// Actions
// ============================================================================

#if 0
static int8_t  str2T(const char* s, char* e, int b) { return strtoll(s, e, b); }
static int16_t str2T(const char* s, char* e, int b) { return strtoll(s, e, b); }
static int32_t str2T(const char* s, char* e, int b) { return strtoll(s, e, b); }
static int64_t str2T(const char* s, char* e, int b) { return strtoll(s, e, b); }
#endif

namespace impl {

template<class T>
struct store_const {
  T& _store;
  const T _value;

  constexpr store_const(T& store, const T& value)
  : _store(store)
  , _value(value)
  {}

  void operator()(const char*) const { _store = _value; }
};

template<class T>
struct increment {
  T& _store;
  void operator()(const char*) const { ++_store; }
};

template<class T>
struct decrement {
  T& _store;
  void operator()(const char*) const { --_store; }
};

template<class T>
struct store_u_integral /* unsigned integrals */
{
  T& _store;

  constexpr store_u_integral(T& store)
  : _store(store)
  {}

  void operator()(const char* s) const {
    errno = 0;
    char* end;
    auto i = std::strtoull(s, &end, 10);
    if (!*s || *end)
      throw Exception("invalid int");
    if (errno || i > std::numeric_limits<T>::max())
      throw std::overflow_error(s);
    _store = i;
  }
};

template<class T>
struct store_integral /* signed integrals */
{
  T& _store;

  constexpr store_integral(T& store)
  : _store(store)
  {}

  void operator()(const char* s) const {
    errno = 0;
    char* end;
    auto i = std::strtoll(s, &end, 10);
    if (!*s || *end)
      throw Exception("invalid int");
    if (errno || i < std::numeric_limits<T>::min() || i > std::numeric_limits<T>::max())
      throw std::overflow_error(s);
    _store = i;
  }
};

template<class T>
struct store { /* generic store */
  T& _store;

  constexpr store(T& store)
  : _store(store)
  {}

  void operator()(const char* s) const { _store = s; }
};

template<> struct store<int8_t>   : store_integral<int8_t>  { using store_integral::store_integral; };
template<> struct store<int16_t>  : store_integral<int16_t> { using store_integral::store_integral; };
template<> struct store<int32_t>  : store_integral<int32_t> { using store_integral::store_integral; };
template<> struct store<int64_t>  : store_integral<int64_t> { using store_integral::store_integral; };

template<> struct store<uint8_t>  : store_u_integral<uint8_t>  { using store_u_integral::store_u_integral; };
template<> struct store<uint16_t> : store_u_integral<uint16_t> { using store_u_integral::store_u_integral; };
template<> struct store<uint32_t> : store_u_integral<uint32_t> { using store_u_integral::store_u_integral; };
template<> struct store<uint64_t> : store_u_integral<uint64_t> { using store_u_integral::store_u_integral; };

} // namespace impl

template<class T> struct impl::store<T>       store(T& store)       { return {store};    }
template<class T> struct impl::store_const<T> store_true(T& store)  { return {store, 1}; }
template<class T> struct impl::store_const<T> store_false(T& store) { return {store, 0}; }
template<class T> struct impl::increment<T>       count(T& store)   { return {store};    }
template<class T> struct impl::increment<T>   increment(T& store)   { return {store};    }
template<class T> struct impl::decrement<T>   decrement(T& store)   { return {store};    }

#define CommandLine_Create_Convenience_Action(f) \
  template<class T> std::function<void(const char*)> f(T& store) \
  { return [&](const char* s){ store.f(s); }; }
CommandLine_Create_Convenience_Action(push_back)
CommandLine_Create_Convenience_Action(emplace_back)
CommandLine_Create_Convenience_Action(insert)
CommandLine_Create_Convenience_Action(emplace)
CommandLine_Create_Convenience_Action(push_front)
CommandLine_Create_Convenience_Action(add)
CommandLine_Create_Convenience_Action(append)
#undef CommandLine_Create_Convenience_Action

// ============================================================================
// Option
// ============================================================================

struct Option {

  using callback_t = std::function<void(const char*)>;

  /*constexpr*/ Option
  (
    const char* long_option,
    char short_option,
    bool has_arg,
    callback_t&& callback
  )
  : _long_option(long_option)
  , _short_option(short_option)
  , _has_arg(has_arg)
  , _callback(std::forward<callback_t>(callback))
  {}

  inline char        short_option()      const noexcept { return _short_option; }
  inline const char* long_option()       const noexcept { return _long_option;  }
  inline bool        has_arg()           const noexcept { return _has_arg;      }
  inline void        call(const char* s) const noexcept { return _callback(s);  }

private:
  const char* _long_option;
  char _short_option;
  bool _has_arg;
  callback_t _callback;
};

template<class Iterator>
struct OptionProvider {
  Iterator beg, end;

  Option* find_short(char c) const noexcept {
    for (auto it = beg; it != end; ++it)
      if (it->short_option() == c)
        return &(*it);
    return 0;
  }

  Option* find_long(const char* s, size_t n) const noexcept {
    for (auto it = beg; it != end; ++it)
      if (! std::char_traits<char>::compare(it->long_option(), s, n))
        return &(*it);
    return 0;
  }
};

template<class Iterator>
struct OptionProvider<Iterator>
make_provider(Iterator beg, Iterator end) { return OptionProvider<Iterator>{beg, end}; }

// ============================================================================
// Parsing function
// ============================================================================

static void default_unknown_argument_handler(const char* s, size_t n) {
  throw Exception("Option not found");
}

template<
  class OptionProvider,
  class ArgumentIterator,
  class ArgumentHandler,
  class UnknownArgumentHandler
>
int parse_args(
  ArgumentIterator arguments_beg,
  ArgumentIterator arguments_end,
  const OptionProvider& options,
  ArgumentHandler&& argument_handler,
  UnknownArgumentHandler&& unknown_argument_handler
) {
  enum {
    NO_DASH     = 0,
    SINGLE_DASH = 1,
    DOUBLE_DASH = 2,
    OPTIONS_END = 3
  };

  for (;arguments_beg != arguments_end; ++arguments_beg) {
    const char* arg = *arguments_beg;
    const int dash_count = (arg[0]=='-'?(arg[1]=='-'?(!arg[2]?3:2):1):0);
    arg += dash_count;

    if (dash_count == SINGLE_DASH) {
      for (;*arg; ++arg) {
        const Option* opt = options.find_short(*arg);
        if (! opt)
          unknown_argument_handler(arg, 1);
        else if (! opt->has_arg())
          opt->call(0);
        else if (arg[1]) {
          opt->call(arg + 1);
          break;
        }
        else if (++arguments_beg != arguments_end) {
          opt->call(*arguments_beg);
          break;
        }
        else throw Exception("missing arg");
      }
    }
    else if (dash_count == DOUBLE_DASH) {
      size_t arg_len;
      for (arg_len = 0; arg[arg_len] && arg[arg_len] != '='; ++arg_len);

      const Option* opt = options.find_long(arg, arg_len);
      if (! opt)
        unknown_argument_handler(arg, arg_len);
      else if (! opt->has_arg()) {
        if (arg[arg_len])
          throw Exception("trailing....");
        opt->call(0);
      }
      else if (arg[arg_len]) {
        opt->call(arg);
      }
      else if (++arguments_beg != arguments_end)
        opt->call(*arguments_beg);
      else
        throw Exception("missing argument");
    }
    else if (dash_count == OPTIONS_END) {
      ++arguments_beg;
      break;
    }
    else
      argument_handler(arg);
  }

  for (;arguments_beg != arguments_end; ++arguments_beg)
    argument_handler(*arguments_beg);

  return 0;
}

template<class OptionProvider, class ArgumentIterator, class ArgumentHandler>
int parse_args(
  ArgumentIterator&& arguments_beg,
  ArgumentIterator&& arguments_end,
  OptionProvider&&   options,
  ArgumentHandler&&  argument_handler
) {
  return parse_args(arguments_beg, arguments_end, options, argument_handler, default_unknown_argument_handler);
}

} // namespace

#include <cstdio>
#include <array>
#include <vector>
#include <iostream>

int main(int argc, const char**argv)
try {
  using namespace CommandLine;

  const char* program = "diff";
  bool dry_run = 0;
  int  i = 0;
  int  verbose = 0;
  std::vector<std::string> args;

  std::array<Option, 5> options = {{
    {"program",   'p', 1,  store(program)},
    {"dry-run",   'n', 0,  store_true(dry_run)},
    {"int",       'i', 1,  store(i)},
    {"verbose",   'v', 0,  count(verbose)},
    {"push_back", 'P', 1,  push_back(args)}
  }};

  parse_args(
    &argv[1], argv + argc,
    make_provider(options.begin(), options.end()),
    [](const char* arg) { std::printf("arg: %s\n", arg); }
  );

  std::printf("program = %s\ndry-run=%d\ni=%d\nverbose=%d\n", program, dry_run, i, verbose);
  for (const auto& s : args)
    std::cout << s << std::endl;

  return 0;
}
catch (const std::exception& e) {
  std::printf("%s\n", e.what());
  return 1;
}

