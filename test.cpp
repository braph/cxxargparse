#include "argparse.hpp"

#include <set>
#include <array>
#include <vector>
#include <cstdio>
#include <cassert>
#include <iostream>

/* TODO:
 * - exceptionless usage
 *   - either using errorcodes
 *   - or by using fallback errhandlers that do `printf(); exit(1)`
 *
 * - add descriptions (aka "help" in python)?
 * - add required flag
 *     -> by checking if option_state.count() > 0
 *
 * - zomg, maybe even --pair a b -> map[a]=b? crazy?
 *
 * - Implement following python-like actions
 *   - store with specialization for the following types:
 *     char<>[u]int8_t
 *     unsigned char - ?
 *     signed char - ??????????????
 *     wchar_t*    << use mbstowc/ws
 *     std::filesystem::path << maybe?
 *   - maybe store_string that works with *arbirtary* string types (also QString)
 *   - append_const -> push_back_const, insert_const ????
 *   - extend
 *   - choices (aaaargh, fuck it?)
 *
 * - append/extend should work on arbitrary container types....
 *   I don't know yet how to do this....
 *
 * - Add groups?
 * - Add mutually exclusive groups?
 *   -> by using option_state
 *
 * - add parse_intermixed_args()? (fuck my life)
 *
 * - Hm... python lets us use subparsers/subcommands (like git, you know?)
 *   We also want to provide this feature (without bloating up our parser, ofc!)
 *   It should be relatively easy! For each subparser set up a new options array.
 *   the argument_handler() should detect the command and switch to the
 *   commands options array. [well, maybe create a new OptionProvider class for this?)
 *
 * - Although `help` should not be part of this lib
 *   we should think about if this is also solvable in a clean way...
 *
 * - Oh oh oh I have an idea!!! Since the action objects are holding
 *   references we can read out the default values! ... yep that is zero useful, since action objects are std::function now.
 *
 * - Detection of duplicate options, *at least in debug (#ifndef NDEBUG)*
 *
 * - An option should be able to have multiple short/long names
 *   - char[8] for maximum 8 short names?
 *   - how to encoded if it's a positional or named option?
 *   - shall we use embedded \0 to avoid copies?
 *
 * - Testing the parser!!! (Maybe write test cases)
 *   Following cases should be covered...
 *     --arg X, --arg=X
 *     --flag, -f, -blfa
 */

int main(int argc, const char**argv)
try {
  using std::cout;
  using namespace CommandLine;

  int         verbose = 0;
  const char* infile  = "";
  std::string outfile;

  bool        _bool = false;

  int8_t      _int8  = 0;
  int16_t     _int16 = 0;
  int32_t     _int32 = 0;
  int64_t     _int64 = 0;

  uint8_t     _uint8  = 0;
  uint16_t    _uint16 = 0;
  uint32_t    _uint32 = 0;
  uint64_t    _uint64 = 0;

  float       _float = 0;
  double      _double = 0;
  long double _long_double = 0;

  std::vector<std::string> _vec_str;
  std::set<std::string> _set_str;

  Option options[] = {
    {"verbose",      'v', 0,  increment(verbose)},
    {"version",      'V', 0,  [](const char*s){ std::printf("Test 0.0.0\n"); std::exit(0); }},

    {"int8_t",        0,  1,  store(_int8)},
    {"int16_t",       0,  1,  store(_int16)},
    {"int32_t",       0,  1,  store(_int32)},
    {"int64_t",       0,  1,  store(_int64)},

    {"uint8_t",       0,  1,  store(_uint8 )},
    {"uint16_t",      0,  1,  store(_uint16)},
    {"uint32_t",      0,  1,  store(_uint32)},
    {"uint64_t",      0,  1,  store(_uint64)},

    {"float",         0,  1,  store(_float)},
    {"double",        0,  1,  store(_double)},
    {"long_double",   0,  1,  store(_long_double)},

    {"toggle",        0,  0,  toggle(_bool)},
    {"store_true",    0,  0,  store_true(_bool)},
    {"store_false",   0,  0,  store_false(_bool)},

    {"vec_str",       0,  1,  push_back(_vec_str)},
    {"set_str",       0,  1,  insert(_set_str)},

    {"INFILE",        1,  1,  store(infile)},
    {"OUTFILE",       1,  1,  store(outfile)},
  };

  options_state state;
  parse_args(
    &argv[1], argv + argc,
    make_provider(std::begin(options), std::end(options)),
    state
  );

#if 0
  for (auto& opt : state)
    cout << opt.option->long_option() << opt.count << std::endl;
#endif

  cout
    << "\nverbose      " << verbose

    << "\nint8_t       " << _int8
    << "\nint16_t      " << _int16
    << "\nint32_t      " << _int32
    << "\nint64_t      " << _int64

    << "\nuint8_t      " << _uint8
    << "\nuint16_t     " << _uint16
    << "\nuint32_t     " << _uint32
    << "\nuint64_t     " << _uint64

    << "\nfloat        " << _float
    << "\ndouble       " << _double
    << "\nlong_double  " << _long_double

    << "\nbool         " << _bool

    << "\nINFILE       " << infile
    << "\nOUTFILE      " << outfile;

  cout << "\nvec str:     "; for (auto& s : _vec_str) cout << s << ',';
  cout << "\nset str:     "; for (auto& s : _set_str) cout << s << ',';
  return 0;
}
catch (const std::exception& e) {
  std::printf("Error: %s\n", e.what());
  return 1;
}

