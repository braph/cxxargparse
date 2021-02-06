#include <functional> // std::function
#include <utility>    // std::forward
#include <string>     // std::char_traits
#include <stdexcept>  // std::invalid_argument
#include <cstdlib>    // std::strtoull
#include <limits>     // std::numeric_limits
#include <vector>     // std::vector
#include <climits>    // INT_MAX
#include <cinttypes>
#include <cassert>//TODO
#include <cstdio>//TODO

#include "actions.hpp"

namespace CommandLine {

// ============================================================================
// Exceptions
// ============================================================================

using Exception = std::invalid_argument;

// ============================================================================
// Option
// ============================================================================

struct Option {
  using callback_t = std::function<void(const char*)>;

  enum : int {
    required = INT_MAX << 1,
    drop_flags = ~(required)
  };

  /*constexpr*/ Option
  (
    const char* long_option,
    char short_option,
    int  num_args,
    callback_t&& callback
  )
  : _long_option(long_option)
  , _short_option(short_option)
  , _num_args(num_args)
  , _callback(std::forward<callback_t>(callback))
  {}

  inline bool        is_positional()     const noexcept { return _short_option == 1;     }
  inline char        short_option()      const noexcept { return _short_option;          }
  inline const char* long_option()       const noexcept { return _long_option;           }
  inline bool        has_arg()           const noexcept { return _num_args & drop_flags; }
  inline int         num_args()          const noexcept { return _num_args & drop_flags; }
  inline void        call(const char* s) const          { return _callback(s);           }
  inline bool        is_required()       const noexcept { return _num_args & required;   }

private:
  const char* _long_option;
  char _short_option;
  int  _num_args;
  callback_t _callback;
};

template<class Iterator>
struct OptionProvider {
  Iterator beg, end;

  Option* find_short(char c) const noexcept {
    for (auto it = beg; it != end; ++it)
      if (! it->is_positional() && it->short_option() == c)
        return &(*it);
    return 0;
  }

  Option* find_long(const char* s, size_t n) const noexcept {
    for (auto it = beg; it != end; ++it)
      if (! it->is_positional() && std::char_traits<char>::compare(it->long_option(), s, n) == 0)
        return &(*it);
    return 0;
  }

  Option* find_positional(int n) const noexcept {
    for (auto it = beg; it != end; ++it)
      if (it->is_positional()) {
        if (n <= 0)
          return &(*it);
        n -= it->num_args();
      }

    return NULL;
  }

#if 0
  Option* next_positional(Option* old_positional) const noexcept {
    for (auto it = beg; it != end; ++it)
      if (it->is_positional()) {
        if (old_positional == NULL)
          return &(*it);
        else if (&(*it) == old_positional)
          old_positional = NULL;
      }

    return NULL;
  }
#endif
};

template<class Iterator>
struct OptionProvider<Iterator>
make_provider(Iterator beg, Iterator end) { return OptionProvider<Iterator>{beg, end}; }

// template<class Container>
// struct OptionProvider<Container>
// make_provider(const Container& c) { return OptionProvider<

// ============================================================================
// Parsing function
// ============================================================================

struct Argument {
  enum argument_type {
    NO_DASH     = 0,
    SINGLE_DASH = 1,
    DOUBLE_DASH = 2,
    OPTIONS_END = 3,
    END         = 4
  };

  const char* s;
  size_t len;
  argument_type type;

  constexpr Argument()
  : s(0)
  , len(0)
  , type(END)
  {}
};

template<class Iterator>
struct Parser {
  Parser(Iterator begin, Iterator end)
  : _args_beg(begin)
  , _args_end(end)
  , _cur_arg()
  , _in_single_arg(false)
  {}

  Iterator _args_beg;
  Iterator _args_end;
  Argument _cur_arg;
  bool     _in_single_arg; // Retrieve next argument from current string

  const Argument& current_argument() const noexcept { return _cur_arg;  }
  Iterator begin()                   const noexcept { return _args_beg; }
  Iterator end()                     const noexcept { return _args_end; }

  const Argument& next() noexcept {
    if (_in_single_arg && _cur_arg.s[1]) {
      _cur_arg.s++;
      return _cur_arg;
    }

    if (_args_beg == _args_end) {
      _cur_arg.type = Argument::END;
      return _cur_arg;
    }

    _cur_arg.type = _get_type(*_args_beg);
    _cur_arg.s    = *_args_beg + _cur_arg.type;
    ++_args_beg;

    if (_cur_arg.type == Argument::SINGLE_DASH) {
      _cur_arg.len = 1;
      _in_single_arg = true;
    }
    else if (_cur_arg.type == Argument::DOUBLE_DASH)
      for (
        _cur_arg.len = 1;
        _cur_arg.s[_cur_arg.len] && _cur_arg.s[_cur_arg.len] != '=';
        _cur_arg.len++
      );

    return _cur_arg;
  }

  /// Read ["-o", "arg"] or ["-oarg"]
  const char* read_short_argument() noexcept {
    _in_single_arg = false;

    if (_cur_arg.s[1]) {
      return _cur_arg.s + 1;
    }
    else if (_args_beg != _args_end) {
      const auto& r = *_args_beg;
      ++_args_beg;
      return r;
    }
    else
      return NULL;
  }

  /// Read ["--option", "arg"] or ["--option=arg"]
  const char* read_long_argument() noexcept {
    if (_cur_arg.s[_cur_arg.len] == '=')
      return &_cur_arg.s[_cur_arg.len + 1];
    else if (_args_beg != _args_end) {
      const auto& r = *_args_beg;
      ++_args_beg;
      return r;
    }
    else
      return NULL;
  }

private:
  static inline Argument::argument_type _get_type(const char* s) {
    return (s[0] != '-' ? Argument::NO_DASH :
           (s[1] != '-' ? Argument::SINGLE_DASH :
           (s[2]        ? Argument::DOUBLE_DASH :
                          Argument::OPTIONS_END)));
  }
};

template<class Iterator>
struct Parser<Iterator>
make_parser(Iterator beg, Iterator end) { return Parser<Iterator>{beg, end}; }

struct options_state {
  struct option_state {
    const Option* option;
    int count;
  };

  options_state()
  : _not_found{NULL, 0}
  {}

  option_state _not_found;
  std::vector<option_state> _option_states;

  using iterator = std::vector<option_state>::iterator;

  void increment_count(Option* option) {
    for (auto& state : _option_states)
      if (state.option == option) {
        ++state.count;
        return;
      }

    _option_states.push_back({option, 1});
  }

  const option_state& find(const Option* option) {
    for (auto& state : _option_states)
      if (state.option == option)
        return state;
    _not_found.option = option;
    return _not_found;
  }

  iterator begin() noexcept { return _option_states.begin(); }
  iterator end()   noexcept { return _option_states.end();   }
};


//static void default_unknown_argument_handler(const char* s, size_t n) {
static void unknown_argument_handler(const char* s, size_t n) {
  throw Exception("Unknown argument ...");
}

template<class OptionProvider, class ArgumentIterator>
int parse_args(
  ArgumentIterator&& arguments_beg,
  ArgumentIterator&& arguments_end,
  OptionProvider&&   options,
  options_state&     state
) {
  int num_positional = 0;
  Option* option;
  auto p = make_parser(arguments_beg, arguments_end);

  do {
    const auto& arg = p.next();
    switch (arg.type) {
    case Argument::NO_DASH:
      option = options.find_positional(num_positional++); // TODO?
      if (option) {
        option->call(arg.s);
        state.increment_count(option);
      }
      else
        unknown_argument_handler(arg.s, -1); // TODO
      break;

    case Argument::SINGLE_DASH:
      if (! (option = options.find_short(*arg.s)))
        throw Exception("Invalid short option");
      if (! option->has_arg())
        option->call(0);
      else {
        const char* arg = p.read_short_argument();
        if (! arg)
          throw Exception("Missing short option argument");
        option->call(arg);
      }
      state.increment_count(option);
      break;

    case Argument::DOUBLE_DASH:
      if (! (option = options.find_long(arg.s, arg.len)))
        throw Exception("Invalid long option");
      if (! option->has_arg())
        option->call(0);
      else {
        const char* arg = p.read_long_argument();
        if (! arg)
          throw Exception("Missing long option argument");
        option->call(arg);
      }
      state.increment_count(option);
      break;

    case Argument::OPTIONS_END:
      break;

    case Argument::END:
      return 0;
    }
  } while(1);

  for (;;) {
    const auto& arg = p.next();
    if (arg.type == Argument::END)
      break;

    option = options.find_positional(num_positional++); // TODO
    if (option) {
      option->call(arg.s);
      state.increment_count(option);
    }
    else
      unknown_argument_handler(arg.s, -1); // TODO
  }

  return 0;
}

} // namespace

