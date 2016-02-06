#ifndef __UTIL_STR_HPP__
#define __UTIL_STR_HPP__

#include <wchar.h>
#include <ctype.h>
#include <stdlib.h> // strtol(3)
#include <string.h>
#include <limits.h>
#include <iterator>
#include <algorithm>
#include <functional>
#include <stdexcept>
#include <vector>
#include <utility>
#include <util/logging.hpp>

//============================================================================
//
// String Tools
//
//============================================================================
namespace util
{
using namespace std;

static pointer_to_unary_function<int, int> is_alnum = ptr_fun(::isalnum);
static pointer_to_unary_function<int, int> is_alpha = ptr_fun(::isalpha);
static pointer_to_unary_function<int, int> is_ascii = ptr_fun(::isascii);
static pointer_to_unary_function<int, int> is_blank = ptr_fun(::isblank);
static pointer_to_unary_function<int, int> is_cntrl = ptr_fun(::iscntrl);
static pointer_to_unary_function<int, int> is_digit = ptr_fun(::isdigit);
static pointer_to_unary_function<int, int> is_graph = ptr_fun(::isgraph);
static pointer_to_unary_function<int, int> is_lower = ptr_fun(::islower);
static pointer_to_unary_function<int, int> is_print = ptr_fun(::isprint);
static pointer_to_unary_function<int, int> is_punct = ptr_fun(::ispunct);
static pointer_to_unary_function<int, int> is_space = ptr_fun(::isspace);
static pointer_to_unary_function<int, int> is_upper = ptr_fun(::isupper);
static pointer_to_unary_function<int, int> is_xdigit = ptr_fun(::isxdigit);

struct whitespace_tokenizer {
    template<typename InputIterator, typename Container>
    InputIterator operator()(InputIterator first, InputIterator last,
                             Container &result)
    {
        using namespace std;
        first = find_if(first, last, not1(is_space));
        if (first == last) {
            return last;
        }
        InputIterator stop = find_if(first + 1, last, is_space);
        copy(first, stop, back_inserter(result));
        return stop;
    }
    void reset() {}
};

template<typename InputIterator, typename EqualityComparable>
InputIterator
find_escaped(InputIterator first, InputIterator last,
             const EqualityComparable &value,
             const EqualityComparable &escape_value)
{
    for ( ; first != last; ++first) {
        if (*first == escape_value) {
            if (++first == last) {
                break;
            }
        } else if (*first == value) {
            break;
        }
    }
    return first;
}

template<typename EqualityComparable>
struct escaped_find {
    escaped_find(const EqualityComparable &quote_marker,
                 const EqualityComparable &escape)
        : quote_marker_(quote_marker),
          escape_(escape) {}
    bool operator()(const EqualityComparable &value)
    {
        if (escaped_character_) {
            escaped_character_ = false;
            return false;
        }

        if (value == quote_marker_) {
            return true;
        }
        if (value == escape_) {
            escaped_character_ = true;
        }
        return false;
    }

    EqualityComparable quote_marker_;
    EqualityComparable escape_;
    bool escaped_character_;
};

struct token_break {
    template<typename EqualityComparable>
    bool operator()(const EqualityComparable &value)
    {
        return (value == '\'') || (value == '"') || util::is_space(value);
    }
};

template<typename Container>
struct quoted_whitespace_tokenizer {
    typedef Container token_type;
    template<typename InputIterator>
    InputIterator operator()(InputIterator first, InputIterator last,
                             Container &result)
    {
        using namespace std;
        typedef typename iterator_traits<InputIterator>::value_type value_t;
        static const value_t single_quote = '\'';
        static const value_t double_quote = '"';
        static const value_t escape_marker = '\\';

        first = find_if(first, last, not1(is_space));
        if (first == last) {
            return last;
        }
        InputIterator stop;
        if (*first == single_quote) {
            stop = find_escaped(first + 1, last, single_quote, escape_marker);
            copy(first + 1, stop, back_inserter(result));
            if (stop != last) {
                ++stop;
            }
        } else if (*first == double_quote) {
            stop = find_escaped(first + 1, last, double_quote, escape_marker);
            copy(first + 1, stop, back_inserter(result));
            if (stop != last) {
                ++stop;
            }
        } else {
            stop = find_if(first + 1, last, token_break());
            copy(first, stop, back_inserter(result));
        }
        return stop;
    }
    void reset() {}
};

template<typename T>
struct tokenizer_traits {
    typedef typename T::token_type token_type;
};

template<typename InputIterator, typename OutputIterator,
         typename Tokenizer>
OutputIterator
tokenize(InputIterator first, InputIterator last,
         OutputIterator result, Tokenizer tokr)
{
    while (first != last) {
        typename tokenizer_traits<Tokenizer>::token_type token;
        first = tokr(first, last, token);
        if (token.empty()) {
            continue;
        }
        *result++ = token;
    }
    return result;
}

template<typename InputIterator, typename Parser,
         typename Tokenizer>
void
parse(InputIterator first, InputIterator last,
      Parser parser, Tokenizer tokr)
{
    typedef   typename tokenizer_traits<Tokenizer>::token_type token_t;
    while (first != last) {
        token_t token;
        first = tokr(first, last, token);
        if (token.empty()) {
            continue;
        }
        parser(token);
    }
    parser(token_t());
}

template<typename InputIterator, typename Tokenizer>
struct token_generator
        : public std::unary_function<typename tokenizer_traits<Tokenizer>::token_type,
          void> {
public:
    // using Concept PushbackToken
    typedef typename tokenizer_traits<Tokenizer>::token_type token_type;

public:
    token_generator(InputIterator first, InputIterator last, Tokenizer tokenizer)
        : first_(first), last_(last), tokenizer_(tokenizer) {}

    typename tokenizer_traits<Tokenizer>::token_type operator()(void)
    {
        typename tokenizer_traits<Tokenizer>::token_type token;
        first_ = tokenizer_(first_, last_, token);
        return token;
    }

    template<typename T>
    void copy_position(T /* & */alternate_token_generator)
    {
        first_ = alternate_token_generator.first_;
        last_ = alternate_token_generator.last_;
    }

    void push_back(typename tokenizer_traits<Tokenizer>::token_type token)
    {
        first_ = token.first;
    }

public:
    InputIterator first_;
    InputIterator last_;
    Tokenizer tokenizer_;
};

template<typename InputIterator, typename OutputIterator,
         typename EqualityComparable>
inline
InputIterator
copy_until(InputIterator first, InputIterator last,
           OutputIterator result, const EqualityComparable &value)
{
    for ( ; first != last; ++first) {
        if (*first == value) {
            break;
        }
        *result++ = *first;
    }
    return first;
}

template<typename InputIterator, typename OutputIterator,
         typename Predicate>
InputIterator
copy_until_if(InputIterator first, InputIterator last,
              OutputIterator result, Predicate pred)
{
    for ( ; first != last; ++first) {
        if (pred(*first)) {
            break;
        }
        *result++ = *first;
    }
    return first;
}

template<typename InputIterator, typename OutputIterator,
         typename EqualityComparable>
OutputIterator
split_value(InputIterator first, InputIterator last,
            OutputIterator result, const EqualityComparable &value)
{
    typedef std::basic_string<
    typename std::iterator_traits<InputIterator>::value_type> value_string;
    while (first != last) {
        value_string line;
        first = copy_until(first, last, back_inserter(line), value);
        *result++ = line;
        if (first == last) {
            break;
        }
        ++first; // skip split-value
    }
    return result;
}

template<typename ForwardIterator1, typename OutputIterator,
         typename ForwardIterator2>
OutputIterator
split_sequence(ForwardIterator1 first1, ForwardIterator1 last1,
               ForwardIterator2 first2, ForwardIterator2 last2,
               OutputIterator result)
{
    typedef std::basic_string<
    typename std::iterator_traits<ForwardIterator1>::value_type> value_string;
    while (first1 != last1) {
        value_string line;
        ForwardIterator1 next = std::search(first1, last1, first2, last2);
        std::copy(first1, next, back_inserter(line));
        first1 = next;
        *result++ = line;
        if (first1 == last1) {
            break;
        }
        ForwardIterator2 adv = first2;
        // skip the split sequence
        while (adv != last2) {
            ++adv;
            ++first1;
        }
    }
    return result;
}

template<typename Container, typename EqualityComparable>
inline
bool
contains(const Container &container, const EqualityComparable &value)
{
    typename Container::const_iterator last = container.end();
    return last != find(container.begin(), last, value);
}

// template<typename AssocContainer, typename EqualityComparable>
// bool
// contains_key(const AssocContainer &container,
//              const EqualityComparable &value) {
//   using namespace std;
//   typedef typename AssocContainer::value_type value_t;
//   typename AssocContainer::const_iterator last = container.end();
//   return last != find_if(container.begin(), last,
//                          compose1(bind2nd(equal_to<value_t>(), value),
//                                   select1st<value_t>()));
// }
//
// template<typename AssocContainer,
//          typename EqualityComparable>
// bool
// contains_value(const AssocContainer &container,
//                const EqualityComparable &value) {
//   using namespace std;
//   typedef typename AssocContainer::value_type value_t;
//   typename AssocContainer::const_iterator last = container.end();
//   return last != find_if(container.begin(), last,
//                          compose1(bind2nd(equal_to<value_t>(), value),
//                                   select2nd<value_t>()));
// }

template<typename EqualityComparable>
struct sequence_contains {
    explicit sequence_contains(const EqualityComparable &value)
        : value_(value) {}
    template<typename Container>
    bool operator()(Container &container)
    {
        return contains(container, value_);
    }

    EqualityComparable value_;
};

template<typename Container>
class wctomb_insert_iterator
{
protected:
    mbstate_t ps_;
    Container *container_;
public:
    typedef Container          container_type;
    typedef output_iterator_tag iterator_category;
    typedef void                value_type;
    typedef void                difference_type;
    typedef void                pointer;
    typedef void                reference;

    explicit wctomb_insert_iterator(Container& c)
        : ps_(), container_(&c)
    {
        /* mbsinit(&ps_); */
    }

    wctomb_insert_iterator<Container>&
    operator=(const typename Container::value_type& value)
    {
        using namespace std;
        string::value_type conv_buf[MB_LEN_MAX];
        size_t n = ::wcrtomb(conv_buf, value, &ps_);
        if (n == (size_t)-1) {
            throw runtime_error("Can't convert wide character");
        }
        copy(conv_buf, conv_buf + n, back_inserter(*container_));
        return *this;
    }

    wctomb_insert_iterator<Container>& operator*()
    {
        return *this;
    }
    wctomb_insert_iterator<Container>& operator++()
    {
        return *this;
    }
    wctomb_insert_iterator<Container>& operator++(int)
    {
        return *this;
    }
};

template<typename Container>
inline
wctomb_insert_iterator<Container>
wctomb_inserter(Container &c)
{
    return wctomb_insert_iterator<Container>(c);
}

class mbtowc_iterator
{
protected:
    string::iterator i_;

public:
    typedef input_iterator_tag  iterator_category;
    typedef std::wstring::value_type  value_type;
    typedef std::wstring::difference_type difference_type;
    typedef std::wstring::pointer     pointer;
    typedef std::wstring::reference   reference;

    explicit mbtowc_iterator() {}
};

inline
std::string
to_string(const std::wstring &s)
{
    using namespace std;
    string buffer;
    copy(s.begin(), s.end(), wctomb_inserter(buffer));

    return buffer;
}

template<typename CharT, typename Predicate>
inline basic_string<CharT>
strip(const basic_string<CharT> &input, Predicate pred)
{
    typedef typename basic_string<CharT>::const_reverse_iterator
    reverse_iterator;
    if (input.empty()) {
        return basic_string<CharT>();
    }
    typename basic_string<CharT>::const_iterator p1
        = find_if(input.begin(), input.end(), not1(pred));
    typename basic_string<CharT>::const_iterator p2
        = (find_if(input.rbegin(), reverse_iterator(p1), not1(pred))).base();

    return basic_string<CharT>(p1, p2);
}

template<typename RandomAccessIterator, typename Predicate>
inline basic_string<
typename iterator_traits<RandomAccessIterator>::value_type>
strip(RandomAccessIterator first, RandomAccessIterator last, Predicate pred)
{
    typedef typename iterator_traits<RandomAccessIterator>::value_type char_t;
    typedef reverse_iterator<RandomAccessIterator> riterator;
    if (first == last) {
        return basic_string<char_t>();
    }
    RandomAccessIterator p1 = find_if(first, last, not1(pred));
    RandomAccessIterator p2
        = (find_if(riterator(last), riterator(p1), not1(pred))).base();

    return basic_string<char_t>(p1, p2);
}

inline
std::wstring
to_wstring(const std::string &s)
{
    using namespace std;
    // calculate the exact size
    const string::value_type *base = s.c_str();
    mbstate_t ps;
    ::memset(&ps, 0, sizeof ps);
    size_t n = ::mbsrtowcs(0, &base, 0, &ps) + 1;
    if (n == (size_t)-1) {
        throw runtime_error("Invalid multi-byte sequence");
    }
    // allocate the string buffer for that size
    vector<wstring::value_type> buffer(n);
    // convert to multi-byte
    ::mbsrtowcs(&(*buffer.begin()), &base, n, &ps);

    return wstring(buffer.begin(), buffer.end() - 1);
}

struct wstring_to_string
        : public std::unary_function<std::string, const std::wstring&> {
    std::string operator()(const std::wstring &s)
    {
        return to_string(s);
    }
};

template<typename Container>
void
reset(Container &c)
{
    Container t;
    std::swap(c, t);
}

inline
long int long_of_string(const string &s)
{
    char *end;
    long int result = strtol(s.c_str(), &end, 10);
    if (end == 0) {
        throw domain_error("Invalid integer representation");
    }
    return result;
}


template<typename T>
struct token_traits {
    typedef typename T::symbol_type symbol_type;
    typedef typename T::value_type value_type;
};

//////////////////////////////////////////////////////////////////////////////
//
// Recursive Descent Parser Using Pushback Token Generator
//
template<typename TokenGenerator>
class base_parser
{
public:
    typedef typename TokenGenerator::token_type token_type;
    typedef TokenGenerator token_gen;
    typedef typename token_traits<token_type>::symbol_type symbol_type;
    typedef typename token_traits<token_type>::value_type value_type;

public:
    base_parser(token_gen gen)
        : input_gen(gen) {}

protected:
    void expect(symbol_type input)
    {
        if (!accept(input)) {
            throw std::runtime_error("Error parsing");
        }
    }

    bool accept(symbol_type input)
    {
        if (current_token.id == input) {
            token_stack.push_back(current_token);
            current_token = input_gen();
            return true;
        }
        return false;
    }

    void push_back(size_t count=1)
    {
        while (count--) {
            input_gen.push_back(token_stack.back());
            token_stack.pop_back();
        }
    }

    void pop(size_t count=1)
    {
        while (count--) {
            token_stack.pop_back();
        }
    }

public:
    token_gen input_gen;
    token_type current_token;
    std::vector<token_type> token_stack;
};

template<typename StringType>
class line_iterator
{
public:
    typedef typename StringType::const_iterator string_iter;
    typedef typename StringType::value_type value_type;
    typedef std::pair<string_iter, string_iter> T;

public:
    line_iterator()
        : _stop_sequence(StringType("\n"))
        , _valid(false)
        , _end()
        , _cur()
        , _stride()
    {
    }
    line_iterator(const StringType& str, const char *stop_sequence="\n")
        : _stop_sequence(StringType(stop_sequence))
        , _valid(true)
        , _end(str.end())
        , _cur(str.begin())
        , _stride(T(str.begin(), str.begin()))
    {
        _next();
    }
    line_iterator(const line_iterator & rh)
        : _stride(rh._stride)
    {
    }
    line_iterator& operator=(const line_iterator & rh)
    {
        _valid = rh._valid;
        _cur = rh._cur;
        _end = rh._end;
        _stride = rh._stride;
        return *this;
    }

    bool operator==(const line_iterator & rh)
    {
        if (_valid && rh._valid) {
            return (_stride.first == rh._stride.first &&
                    _stride.second == rh._stride.second);
        } else if (!_valid && !rh._valid) {
            // end of stream
            return true;
        } else {
            return false;
        }
    }
    bool operator!=(const line_iterator & rh)
    {
        return !(*this == rh);
    }

    T& operator*()
    {
        return _stride;
    }
    T* operator->()
    {
        return &_stride;
    }
    line_iterator& operator++()
    {
        _next();
        return *this;
    }
    line_iterator& operator++(int)
    {
        line_iterator tmp(*this);
        _next();
        return tmp;
    }
    bool depleted() const
    {
        return _stride.first == _stride.second;
    }

private:
    void _next()
    {
        string_iter s1 = _stop_sequence.begin();
        string_iter s2 = _stop_sequence.end();
        string_iter pos = std::search(_cur, _end, s1, s2);
        if (pos != _end) {
            pos += _stop_sequence.size();
        }
        _stride.first = _cur;
        _stride.second = pos;
        _cur = pos;

        if (_stride.first == _stride.second) {
            *this = line_iterator();
        }
    }

public:
    StringType _stop_sequence;
    bool _valid;
    string_iter _end;
    string_iter _cur;
    T _stride;
};

} // namespace util


#endif
// vim: set sts=2 sw=2 expandtab:
