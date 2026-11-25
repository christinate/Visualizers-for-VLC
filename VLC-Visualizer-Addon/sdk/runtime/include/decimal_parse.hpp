// SPDX-License-Identifier: BSD-3-Clause
#pragma once
#include <charconv>
#include <cmath>
#include <string>
#include <string_view>
#ifdef __APPLE__
#include <cerrno>
#include <stdexcept>
#include <xlocale.h>
#endif

namespace suite {
// Full-token finite decimal/scientific parsing, independent of process locale.
// Leading '+' and surrounding whitespace belong to the caller's grammar.
inline bool finiteDecimal(std::string_view text,double& output){
    if(text.empty())return false;
    double number=0;
#ifdef __APPLE__
    // Floating from_chars in this libc++ requires macOS 26. strtod_l works
    // on older targets; restrict its broader C grammar to from_chars general.
    const char* cursor=text.data();const char* end=cursor+text.size();
    if(*cursor=='-')++cursor;
    const auto digits=[&](){const char* start=cursor;while(cursor!=end&&*cursor>='0'&&*cursor<='9')++cursor;return cursor!=start;};
    bool decimal=digits();if(cursor!=end&&*cursor=='.'){++cursor;decimal=digits()||decimal;}
    if(!decimal)return false;
    if(cursor!=end&&(*cursor=='e'||*cursor=='E')){++cursor;if(cursor!=end&&(*cursor=='+'||*cursor=='-'))++cursor;if(!digits())return false;}
    if(cursor!=end)return false;
    struct NumericLocale {locale_t value=newlocale(LC_NUMERIC_MASK,"C",nullptr);~NumericLocale(){if(value)freelocale(value);}};
    static const NumericLocale locale;
    if(!locale.value)throw std::runtime_error("Cannot initialize numeric locale");
    const std::string token(text);char* parsed=nullptr;const int previous=errno;
    errno=0;number=strtod_l(token.c_str(),&parsed,locale.value);const int error=errno;errno=previous;
    // C may report ERANGE for a representable subnormal; keep it. Full
    // underflow to zero and overflow are rejected, matching from_chars.
    if(parsed!=token.data()+token.size()||(error==ERANGE&&number==0))return false;
#else
    const auto result=std::from_chars(text.data(),text.data()+text.size(),number);
    if(result.ec!=std::errc{}||result.ptr!=text.data()+text.size())return false;
#endif
    if(!std::isfinite(number))return false;
    output=number;return true;
}
}
