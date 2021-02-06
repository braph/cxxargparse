#ifndef LIB_COMMANDLINE_PARSER_ACTIONS_HPP
#define LIB_COMMANDLINE_PARSER_ACTIONS_HPP

#include <stdexcept>  // std::invalid_argument
#include <type_traits>

namespace CommandLine {
namespace impl {

template<class T>
struct increment {
  T& _var;
  void operator()(const char*) const { ++_var; }
};

template<class T>
struct decrement {
  T& _var;
  void operator()(const char*) const { --_var; }
};

template<class T>
struct toggle {
  T& _var;
  void operator()(const char*) const { _var = !_var; }
};

template<class T>
struct store_const {
  T& _var;
  const T _value;
  void operator()(const char*) const { _var = _value; }
};

template<class T>
struct store { /* generic store */
  T& _var;
  void operator()(const char* s) const { _var = s; }
};

// Macros because taking address of fuctions in std:: is UB
#define make_parse_func(T, CALLBACK, NEED_NARROWING_CHECK)                    \
template<> inline T parse<T>(const char* s) {                                 \
  errno = 0;                                                                  \
  char *end;                                                                  \
  auto r = CALLBACK;                                                          \
  if (!*s || *end)                                                            \
    throw std::invalid_argument(s);                                           \
  if (errno)                                                                  \
    throw std::range_error(s);                                                \
  if (NEED_NARROWING_CHECK && r < std::numeric_limits<T>::min())              \
    throw std::underflow_error(s);                                            \
  if (NEED_NARROWING_CHECK && r > std::numeric_limits<T>::max())              \
    throw std::overflow_error(s);                                             \
  return r;                                                                   \
}                                                                             \

template<class T> T parse(const char*s);
make_parse_func(float,       std::strtof(s,   &end),  false)
make_parse_func(double,      std::strtod(s,   &end),  false)
make_parse_func(long double, std::strtold(s,  &end),  false)
make_parse_func(int8_t,      std::strtoll(s,  &end, 10),  true)
make_parse_func(int16_t,     std::strtoll(s,  &end, 10),  true)
make_parse_func(int32_t,     std::strtoll(s,  &end, 10),  true)
make_parse_func(int64_t,     std::strtoll(s,  &end, 10),  false)
make_parse_func(uint8_t,     std::strtoull(s, &end, 10),  true)
make_parse_func(uint16_t,    std::strtoull(s, &end, 10),  true)
make_parse_func(uint32_t,    std::strtoull(s, &end, 10),  true)
make_parse_func(uint64_t,    std::strtoull(s, &end, 10),  false)
#undef make_parse_func

#define make_struct(T) \
template<> struct store<T> { T& _var; void operator()(const char*s) { _var = parse<T>(s); } }
make_struct(float);
make_struct(double);
make_struct(long double);
make_struct(int8_t);
make_struct(int16_t);
make_struct(int32_t);
make_struct(int64_t);
make_struct(uint8_t);
make_struct(uint16_t);
make_struct(uint32_t);
make_struct(uint64_t);
#undef make_struct

} // namespace impl

template<class T> struct impl::store<T>       store(T& var)       { return {var}; }
template<class T> struct impl::store_const<T> store_true(T& var)  { return {var, 1}; }
template<class T> struct impl::store_const<T> store_false(T& var) { return {var, 0}; }
template<class T> struct impl::store_const<T> store_const(T& var, const T& val) { return {var, val}; }
template<class T> struct impl::toggle<T>      toggle(T& var)      { return {var};    }
template<class T> struct impl::increment<T>   count(T& var)       { return {var};    }
template<class T> struct impl::increment<T>   increment(T& var)   { return {var};    }
template<class T> struct impl::decrement<T>   decrement(T& var)   { return {var};    }

#if 0 // TODO
#define make_lambda(NAME, ARGS, ...) \
template<class T> std::function<void(const char*)> NAME ARGS { return [=](const char* s){ __VA_ARGS__ }; }
make_lambda(toggle, (T& var), var = !!var;)
#undef make_lambda
#endif

#define make_convenience_func(f) \
template<class T> std::function<void(const char*)> f(T& var) \
{ return [&](const char* s){ var.f(s); }; }
make_convenience_func(push_back)
make_convenience_func(emplace_back)
make_convenience_func(insert)
make_convenience_func(emplace)
make_convenience_func(push_front)
make_convenience_func(add)
make_convenience_func(append)
#undef make_convenience_func

//#define make_convenience_func
#if 0
template<class TVar, class TValue>
std::function<void(const char*)> push_back(T& var, const TValue& val)
{ return [&](const char* s){ var.push_back(val); }; }
#endif

}

#endif
