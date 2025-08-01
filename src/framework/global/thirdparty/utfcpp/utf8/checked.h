// Copyright 2006-2016 Nemanja Trifunovic

/*
Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#ifndef UTF8_FOR_CPP_CHECKED_H_2675DCD0_9480_4c0c_B92A_CC14C027B731
#define UTF8_FOR_CPP_CHECKED_H_2675DCD0_9480_4c0c_B92A_CC14C027B731

#include "core.h"
#include <stdexcept>

namespace utf8 {
// Base for the exceptions that may be thrown from the library
class exception : public ::std::exception
{
};

// Exceptions that may be thrown from the library functions.
class invalid_code_point : public exception
{
    utfchar32_t cp;
public:
    invalid_code_point(utfchar32_t codepoint)
        : cp(codepoint) {}
    virtual const char* what() const UTF_CPP_NOEXCEPT UTF_CPP_OVERRIDE { return "Invalid code point"; }
    utfchar32_t code_point() const { return cp; }
};

class invalid_utf8 : public exception
{
    utfchar8_t u8;
public:
    invalid_utf8 (utfchar8_t u)
        : u8(u) {}
    invalid_utf8 (char c)
        : u8(static_cast<utfchar8_t>(c)) {}
    virtual const char* what() const UTF_CPP_NOEXCEPT UTF_CPP_OVERRIDE { return "Invalid UTF-8"; }
    utfchar8_t utf8_octet() const { return u8; }
};

class invalid_utf16 : public exception
{
    utfchar16_t u16;
public:
    invalid_utf16 (utfchar16_t u)
        : u16(u) {}
    virtual const char* what() const UTF_CPP_NOEXCEPT UTF_CPP_OVERRIDE { return "Invalid UTF-16"; }
    utfchar16_t utf16_word() const { return u16; }
};

class not_enough_room : public exception
{
public:
    virtual const char* what() const UTF_CPP_NOEXCEPT UTF_CPP_OVERRIDE { return "Not enough space"; }
};

/// The library API - functions intended to be called by the users

template<typename octet_iterator>
octet_iterator append(utfchar32_t cp, octet_iterator result)
{
    if (!utf8::internal::is_code_point_valid(cp)) {
        throw invalid_code_point(cp);
    }

    return internal::append(cp, result);
}

inline void append(utfchar32_t cp, std::string& s)
{
    append(cp, std::back_inserter(s));
}

template<typename word_iterator>
word_iterator append16(utfchar32_t cp, word_iterator result)
{
    if (!utf8::internal::is_code_point_valid(cp)) {
        throw invalid_code_point(cp);
    }

    return internal::append16(cp, result);
}

template<typename octet_iterator, typename output_iterator>
output_iterator replace_invalid(octet_iterator start, octet_iterator end, output_iterator out, utfchar32_t replacement)
{
    while (start != end) {
        octet_iterator sequence_start = start;
        internal::utf_error err_code = utf8::internal::validate_next(start, end);
        switch (err_code) {
        case internal::UTF8_OK:
            for (octet_iterator it = sequence_start; it != start; ++it) {
                *out++ = *it;
            }
            break;
        case internal::NOT_ENOUGH_ROOM:
            out = utf8::append(replacement, out);
            start = end;
            break;
        case internal::INVALID_LEAD:
            out = utf8::append(replacement, out);
            ++start;
            break;
        case internal::INCOMPLETE_SEQUENCE:
        case internal::OVERLONG_SEQUENCE:
        case internal::INVALID_CODE_POINT:
            out = utf8::append(replacement, out);
            ++start;
            // just one replacement mark for the sequence
            while (start != end && utf8::internal::is_trail(*start)) {
                ++start;
            }
            break;
        }
    }
    return out;
}

template<typename octet_iterator, typename output_iterator>
inline output_iterator replace_invalid(octet_iterator start, octet_iterator end, output_iterator out)
{
    static const utfchar32_t replacement_marker = static_cast<utfchar32_t>(utf8::internal::mask16(0xfffd));
    return utf8::replace_invalid(start, end, out, replacement_marker);
}

inline std::string replace_invalid(const std::string& s, utfchar32_t replacement)
{
    std::string result;
    replace_invalid(s.begin(), s.end(), std::back_inserter(result), replacement);
    return result;
}

inline std::string replace_invalid(const std::string& s)
{
    std::string result;
    replace_invalid(s.begin(), s.end(), std::back_inserter(result));
    return result;
}

template<typename octet_iterator>
utfchar32_t next(octet_iterator& it, octet_iterator end)
{
    utfchar32_t cp = 0;
    internal::utf_error err_code = utf8::internal::validate_next(it, end, cp);
    switch (err_code) {
    case internal::UTF8_OK:
        break;
    case internal::NOT_ENOUGH_ROOM:
        throw not_enough_room();
    case internal::INVALID_LEAD:
    case internal::INCOMPLETE_SEQUENCE:
    case internal::OVERLONG_SEQUENCE:
        throw invalid_utf8(static_cast<utfchar8_t>(*it));
    case internal::INVALID_CODE_POINT:
        throw invalid_code_point(cp);
    }
    return cp;
}

template<typename word_iterator>
utfchar32_t next16(word_iterator& it, word_iterator end)
{
    utfchar32_t cp = 0;
    internal::utf_error err_code = utf8::internal::validate_next16(it, end, cp);
    if (err_code == internal::NOT_ENOUGH_ROOM) {
        throw not_enough_room();
    }
    return cp;
}

template<typename octet_iterator>
utfchar32_t peek_next(octet_iterator it, octet_iterator end)
{
    return utf8::next(it, end);
}

template<typename octet_iterator>
utfchar32_t prior(octet_iterator& it, octet_iterator start)
{
    // can't do much if it == start
    if (it == start) {
        throw not_enough_room();
    }

    octet_iterator end = it;
    // Go back until we hit either a lead octet or start
    while (utf8::internal::is_trail(*(--it))) {
        if (it == start) {
            throw invalid_utf8(*it);     // error - no lead byte in the sequence
        }
    }
    return utf8::peek_next(it, end);
}

template<typename octet_iterator, typename distance_type>
void advance(octet_iterator& it, distance_type n, octet_iterator end)
{
    const distance_type zero(0);
    if (n < zero) {
        // backward
        for (distance_type i = n; i < zero; ++i) {
            utf8::prior(it, end);
        }
    } else {
        // forward
        for (distance_type i = zero; i < n; ++i) {
            utf8::next(it, end);
        }
    }
}

template<typename octet_iterator>
typename std::iterator_traits<octet_iterator>::difference_type
distance(octet_iterator first, octet_iterator last)
{
    typename std::iterator_traits<octet_iterator>::difference_type dist;
    for (dist = 0; first < last; ++dist) {
        utf8::next(first, last);
    }
    return dist;
}

template<typename u16bit_iterator, typename octet_iterator>
octet_iterator utf16to8(u16bit_iterator start, u16bit_iterator end, octet_iterator result)
{
    while (start != end) {
        utfchar32_t cp = static_cast<utfchar32_t>(utf8::internal::mask16(*start++));
        // Take care of surrogate pairs first
        if (utf8::internal::is_lead_surrogate(cp)) {
            if (start != end) {
                const utfchar32_t trail_surrogate = static_cast<utfchar32_t>(utf8::internal::mask16(*start++));
                if (utf8::internal::is_trail_surrogate(trail_surrogate)) {
                    cp = (cp << 10) + trail_surrogate + internal::SURROGATE_OFFSET;
                } else {
                    throw invalid_utf16(static_cast<utfchar16_t>(trail_surrogate));
                }
            } else {
                throw invalid_utf16(static_cast<utfchar16_t>(cp));
            }
        }
        // Lone trail surrogate
        else if (utf8::internal::is_trail_surrogate(cp)) {
            throw invalid_utf16(static_cast<utfchar16_t>(cp));
        }

        result = utf8::append(cp, result);
    }
    return result;
}

template<typename u16bit_iterator, typename octet_iterator>
u16bit_iterator utf8to16(octet_iterator start, octet_iterator end, u16bit_iterator result)
{
    while (start < end) {
        const utfchar32_t cp = utf8::next(start, end);
        if (cp > 0xffff) {     //make a surrogate pair
            *result++ = static_cast<utfchar16_t>((cp >> 10) + internal::LEAD_OFFSET);
            *result++ = static_cast<utfchar16_t>((cp & 0x3ff) + internal::TRAIL_SURROGATE_MIN);
        } else {
            *result++ = static_cast<utfchar16_t>(cp);
        }
    }
    return result;
}

template<typename octet_iterator, typename u32bit_iterator>
octet_iterator utf32to8(u32bit_iterator start, u32bit_iterator end, octet_iterator result)
{
    while (start != end) {
        result = utf8::append(*(start++), result);
    }

    return result;
}

template<typename octet_iterator, typename u32bit_iterator>
u32bit_iterator utf8to32(octet_iterator start, octet_iterator end, u32bit_iterator result)
{
    while (start < end) {
        (*result++) = utf8::next(start, end);
    }

    return result;
}

// The iterator class
template<typename octet_iterator>
class iterator
{
    octet_iterator it;
    octet_iterator range_start;
    octet_iterator range_end;
public:
    typedef utfchar32_t value_type;
    typedef utfchar32_t* pointer;
    typedef utfchar32_t& reference;
    typedef std::ptrdiff_t difference_type;
    typedef std::bidirectional_iterator_tag iterator_category;
    iterator () {}
    explicit iterator (const octet_iterator& octet_it,
                       const octet_iterator& rangestart,
                       const octet_iterator& rangeend)
        : it(octet_it), range_start(rangestart), range_end(rangeend)
    {
        if (it < range_start || it > range_end) {
            throw std::out_of_range("Invalid utf-8 iterator position");
        }
    }

    // the default "big three" are OK
    octet_iterator base() const { return it; }
    utfchar32_t operator *() const
    {
        octet_iterator temp = it;
        return utf8::next(temp, range_end);
    }

    bool operator ==(const iterator& rhs) const
    {
        if (range_start != rhs.range_start || range_end != rhs.range_end) {
            throw std::logic_error("Comparing utf-8 iterators defined with different ranges");
        }
        return it == rhs.it;
    }

    bool operator !=(const iterator& rhs) const
    {
        return !(operator ==(rhs));
    }

    iterator& operator ++()
    {
        utf8::next(it, range_end);
        return *this;
    }

    iterator operator ++(int)
    {
        iterator temp = *this;
        utf8::next(it, range_end);
        return temp;
    }

    iterator& operator --()
    {
        utf8::prior(it, range_start);
        return *this;
    }

    iterator operator --(int)
    {
        iterator temp = *this;
        utf8::prior(it, range_start);
        return temp;
    }
};     // class iterator
} // namespace utf8

#if UTF_CPP_CPLUSPLUS >= 202002L // C++ 20 or later
#include "cpp20.h"
#elif UTF_CPP_CPLUSPLUS >= 201703L // C++ 17 or later
#include "cpp17.h"
#elif UTF_CPP_CPLUSPLUS >= 201103L // C++ 11 or later
#include "cpp11.h"
#endif // C++ 11 or later

#endif //header guard
