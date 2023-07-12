/* **********************************************************
 * Copyright (c) 2015-2023 Google, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/**
 * @file droption.h
 * @brief Options parsing support
 */

#ifndef _DROPTION_H_
#define _DROPTION_H_ 1

#include <string>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include <sstream>
#include <iomanip>
#include <limits.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>

namespace dynamorio { /**< General DynamoRIO namespace. */
namespace droption {  /**< DynamoRIO Option Parser namespace. */

#define TESTALL(mask, var) (((mask) & (var)) == (mask))
#define TESTANY(mask, var) (((mask) & (var)) != 0)

// We are no longer supporting pre-C++11 as it complicates the code too
// much, requiring macros for 'override', etc.
// MSVC 2013 accepts 'override' but returns 199711.
#if (defined(UNIX) && __cplusplus < 201103L) || \
    (defined(WINDOWS) && __cplusplus < 199711L)
#    error This library requires C++11
#endif

#define DROPTION_DEFAULT_VALUE_SEP " "

// XXX: some clients want further distinctions, such as options passed to
// post-processing components, internal (i.e., undocumented) options, etc.
/**
 * These bitfield values specify whether an option is intended for use
 * by a client for a tool frontend, or both.
 */
typedef enum {
    DROPTION_SCOPE_CLIENT = 0x0001,   /**< Acted on by the client only. */
    DROPTION_SCOPE_FRONTEND = 0x0002, /**< Acted on by the frontend only. */
    /** Acted on by both client and frontend. */
    DROPTION_SCOPE_ALL = (DROPTION_SCOPE_CLIENT | DROPTION_SCOPE_FRONTEND),
} droption_scope_t;

/**
 * These bitfield flags further specify the behavior of an option.
 */
typedef enum {
    /**
     * By default, if an option is specified multiple times on the
     * command line, only the last value is honored.  If this flag is
     * set, repeated options accumulate, appending to the prior value
     * (separating each appended value with a space by default or with a
     * user-specified accumulated value separator if it was supplied to the
     * droption_t constructor). This is supported for options of type
     * std::string only.
     */
    // XXX: to support other types of accumulation, we should add explicit
    // support for dr_option_t<std::vector<std::string>>.
    DROPTION_FLAG_ACCUMULATE = 0x0001,
    /**
     * By default, an option that does not match a known name and the
     * current scope results in an error.  If a string option exists
     * with this flag set, however, all unknown options in the current
     * scope that are known in another scope are passed to the last
     * option with this flag set (which will typically also set
     * #DROPTION_FLAG_ACCUMULATE).  Additionally, options that are
     * specified and that have #DROPTION_SCOPE_ALL are swept as well.
     * The scope of an option with this flag is ignored.
     */
    DROPTION_FLAG_SWEEP = 0x0002,
    /**
     * Indicates that this is an internal option and should be excluded from
     * usage messages and documentation.
     */
    DROPTION_FLAG_INTERNAL = 0x0004,
} droption_flags_t;

/**
 * The bytesize_t class exists to provide an option type that accepts suffixes
 * like 'K', 'M', and 'G' when specifying sizes in units of bytes.
 */
class bytesize_t {
public:
    bytesize_t()
        : size_(0)
    {
    }
    // The bytesize_t class is backed by a 64-bit unsigned integer.
    bytesize_t(uint64_t val)
        : size_(val)
    {
    }
    operator uint64_t() const
    {
        return size_;
    }
    uint64_t size_;
};

/** A convenience typedef for options that take in pairs of values. */
typedef std::pair<std::string, std::string> twostring_t;

/**
 * Option parser base class.
 */
class droption_parser_t {
public:
    droption_parser_t(unsigned int scope, std::string name, std::string desc_short,
                      std::string desc_long, unsigned int flags)
        : scope_(scope)
        , names_(std::vector<std::string>(1, name))
        , is_specified_(false)
        , desc_short_(desc_short)
        , desc_long_(desc_long)
        , flags_(flags)
    {
        // We assume no synch is needed as this is a static initializer.
        // XXX: any way to check/assert on that?
        allops().push_back(this);
        if (TESTANY(DROPTION_FLAG_SWEEP, flags_))
            sweeper() = this;
    }

    droption_parser_t(unsigned int scope, std::vector<std::string> names,
                      std::string desc_short, std::string desc_long, unsigned int flags)
        : scope_(scope)
        , names_(names)
        , is_specified_(false)
        , desc_short_(desc_short)
        , desc_long_(desc_long)
        , flags_(flags)
    {
        // We assume no synch is needed as this is a static initializer.
        // XXX: any way to check/assert on that?
        allops().push_back(this);
        if (TESTANY(DROPTION_FLAG_SWEEP, flags_))
            sweeper() = this;
    }

    virtual ~droption_parser_t() = default;

    // We do not provide a string-parsing routine as we assume a client will
    // use dr_get_option_array() to convert to an argv[] array, or use our
    // provided routine dr_parse_options().

    /**
     * Parses the options for client \p client_id and fills in any registered
     * droption_t class fields.
     * On success, returns true, with the index of the start of the remaining
     * unparsed options, if any, returned in \p last_index (typically this
     * will be options separated by "--" or when encountering a token that
     * does not start with a leading "-").
     * On failure, returns false, and if \p error_msg != NULL, stores a string
     * describing the error there.  On failure, \p last_index is set to the
     * index of the problematic option or option value.
     *
     * We recommend that Windows standalone applications use UNICODE and
     * call drfront_convert_args() to convert to UTF-8 prior to passing here,
     * for proper internationalization support.
     */
    static bool
    parse_argv(unsigned int scope, int argc, const char *argv[], std::string *error_msg,
               int *last_index)
    {
        int i;
        bool res = true;
        for (i = 1 /*skip app*/; i < argc; ++i) {
            // We support the universal "--" as a separator
            if (strcmp(argv[i], "--") == 0) {
                ++i; // for last_index
                break;
            }
            // Also stop on a non-leading-dash token to support arguments without
            // a separating "--".
            if (argv[i][0] != '-') {
                break;
            }
            bool matched = false;
            bool swept = false;
            for (std::vector<droption_parser_t *>::iterator opi = allops().begin();
                 opi != allops().end(); ++opi) {
                droption_parser_t *op = *opi;
                // We parse other-scope options and their values, for sweeping.
                if (op->name_match(argv[i])) {
                    if (TESTANY(scope, op->scope_))
                        matched = true;
                    if (sweeper() != NULL &&
                        (!matched ||
                         // Sweep up both-scope options as well as ummatched
                         TESTALL(DROPTION_SCOPE_ALL, op->scope_)) &&
                        sweeper()->convert_from_string(argv[i]) &&
                        sweeper()->clamp_value()) {
                        sweeper()->is_specified_ = true; // *after* convert_from_string()
                        swept = true;
                    }
                    if (op->option_takes_arg()) {
                        ++i;
                        if (op->option_takes_2args() && i < argc)
                            ++i;
                        if (i == argc) {
                            if (error_msg != NULL) {
                                *error_msg =
                                    "Option " + op->get_name() + " missing value";
                            }
                            res = false;
                            goto parse_finished;
                        }
                        if (matched) {
                            if ((!op->option_takes_2args() &&
                                 !op->convert_from_string(argv[i])) ||
                                (op->option_takes_2args() &&
                                 !op->convert_from_string(argv[i - 1], argv[i])) ||
                                !op->clamp_value()) {
                                if (error_msg != NULL) {
                                    *error_msg = "Option " + op->get_name() +
                                        " value out of range";
                                }
                                res = false;
                                goto parse_finished;
                            }
                        }
                        if (swept) {
                            if ((!op->option_takes_2args() &&
                                 !sweeper()->convert_from_string(argv[i])) ||
                                (op->option_takes_2args() &&
                                 !sweeper()->convert_from_string(argv[i - 1], argv[i])) ||
                                !sweeper()->clamp_value()) {
                                if (error_msg != NULL) {
                                    *error_msg = "Option " + op->get_name() +
                                        " value out of range";
                                }
                                res = false;
                                goto parse_finished;
                            }
                        }
                    }
                    if (matched)
                        op->is_specified_ = true; // *after* convert_from_string()
                }
            }
            if (!matched && !swept) {
                if (error_msg != NULL)
                    *error_msg = std::string("Unknown option: ") + argv[i];
                res = false;
                goto parse_finished;
            }
        }
    parse_finished:
        if (last_index != NULL)
            *last_index = i;
        return res;
    }

    /**
     * Returns a string containing a list of all of the parameters, their
     * default values, and their short descriptions.
     */
    static std::string
    usage_short(unsigned int scope)
    {
        std::ostringstream oss;
        for (std::vector<droption_parser_t *>::iterator opi = allops().begin();
             opi != allops().end(); ++opi) {
            droption_parser_t *op = *opi;
            if (!TESTALL(DROPTION_FLAG_INTERNAL, op->flags_) &&
                TESTANY(scope, op->scope_)) {
                oss << " -" << std::setw(20) << std::left << op->get_name() << "["
                    << std::setw(6) << std::right << op->default_as_string() << "]"
                    << "  " << std::left << op->desc_short_ << std::endl;
            }
        }
        return oss.str();
    }

    /**
     * Returns a string containing a list of all of the parameters, their
     * default values, and their long descriptions, with the given pre- and
     * post- values.  This is intended for use in generating documentation.
     */
    static std::string
    usage_long(unsigned int scope, std::string pre_name = "----------\n",
               std::string post_name = "\n", std::string pre_value = "",
               std::string post_value = "\n", std::string pre_desc = "",
               std::string post_desc = "\n")
    {
        std::ostringstream oss;
        for (std::vector<droption_parser_t *>::iterator opi = allops().begin();
             opi != allops().end(); ++opi) {
            droption_parser_t *op = *opi;
            // XXX: we should also add the min and max values
            if (!TESTALL(DROPTION_FLAG_INTERNAL, op->flags_) &&
                TESTANY(scope, op->scope_)) {
                oss << pre_name << "-" << op->get_name() << post_name << pre_value
                    << "default value: " << op->default_as_string() << post_value
                    << pre_desc << op->desc_long_ << post_desc << std::endl;
            }
        }
        return oss.str();
    }

    /** Returns whether this option was specified in the argument list. */
    bool
    specified()
    {
        return is_specified_;
    }
    /** Returns the name of this option. */
    std::string
    get_name()
    {
        return names_[0];
    }

    /**
     * For clients statically linked into the target application, global state is not
     * reset between a detach and re-attach.  Thus, #DROPTION_FLAG_ACCUMULATE options
     * will append to their values from the prior run.  This function is provided for
     * a client to call on detach or re-attach to reset these values.
     */
    static void
    clear_values()
    {
        for (std::vector<droption_parser_t *>::iterator opi = allops().begin();
             opi != allops().end(); ++opi) {
            droption_parser_t *op = *opi;
            op->clear_value();
        }
    }

protected:
    virtual bool
    option_takes_arg() const = 0;
    virtual bool
    option_takes_2args() const = 0;
    virtual bool
    name_match(const char *arg) = 0; // also sets value for bools!
    virtual bool
    convert_from_string(const std::string s) = 0;
    virtual bool
    convert_from_string(const std::string s1, const std::string s2) = 0;
    virtual bool
    clamp_value() = 0;
    virtual std::string
    default_as_string() const = 0;
    virtual void
    clear_value() = 0;

    // To avoid static initializer ordering problems we use a function:
    static std::vector<droption_parser_t *> &
    allops()
    {
        static std::vector<droption_parser_t *> allops_vec_;
        return allops_vec_;
    }
    static droption_parser_t *&
    sweeper()
    {
        static droption_parser_t *global_sweeper_;
        return global_sweeper_;
    }

    unsigned int scope_; // made up of droption_scope_t bitfields
    std::vector<std::string> names_;
    bool is_specified_;
    std::string desc_short_;
    std::string desc_long_;
    unsigned int flags_;
};

/** Option class for declaring new options. */
template <typename T> class droption_t : public droption_parser_t {
public:
    /**
     * Declares a new option of type T with the given scope, default value,
     * and description in short and long forms.
     */
    droption_t(unsigned int scope, std::string name, T defval, std::string desc_short,
               std::string desc_long)
        : droption_parser_t(scope, name, desc_short, desc_long, 0)
        , value_(defval)
        , defval_(defval)
        , valsep_(DROPTION_DEFAULT_VALUE_SEP)
        , has_range_(false)
    {
    }

    /**
     * Declares a new option of type T with the given scope, behavior flags,
     * default value, and description in short and long forms.
     */
    droption_t(unsigned int scope, std::string name, unsigned int flags, T defval,
               std::string desc_short, std::string desc_long)
        : droption_parser_t(scope, name, desc_short, desc_long, flags)
        , value_(defval)
        , defval_(defval)
        , valsep_(DROPTION_DEFAULT_VALUE_SEP)
        , has_range_(false)
    {
    }

    /**
     * Declares a new option of type T with the given scope, behavior flags,
     * accumulated value separator (see #DROPTION_FLAG_ACCUMULATE), default
     * value, and description in short and long forms.
     */
    droption_t(unsigned int scope, std::string name, unsigned int flags,
               std::string valsep, T defval, std::string desc_short,
               std::string desc_long)
        : droption_parser_t(scope, name, desc_short, desc_long, flags)
        , value_(defval)
        , defval_(defval)
        , valsep_(valsep)
        , has_range_(false)
    {
    }

    /**
     * Declares a new option of type T with the given scope, default value,
     * minimum and maximum values, and description in short and long forms.
     */
    droption_t(unsigned int scope, std::string name, T defval, T minval, T maxval,
               std::string desc_short, std::string desc_long)
        : droption_parser_t(scope, name, desc_short, desc_long, 0)
        , value_(defval)
        , defval_(defval)
        , valsep_(DROPTION_DEFAULT_VALUE_SEP)
        , has_range_(true)
        , minval_(minval)
        , maxval_(maxval)
    {
    }

    /**
     * Declares a new option of type T with the given scope, list of alternative names,
     * default value, and description in short and long forms.
     */
    droption_t(unsigned int scope, std::vector<std::string> names, T defval,
               std::string desc_short, std::string desc_long)
        : droption_parser_t(scope, names, desc_short, desc_long, 0)
        , value_(defval)
        , defval_(defval)
        , valsep_(DROPTION_DEFAULT_VALUE_SEP)
        , has_range_(false)
    {
    }

    /**
     * Declares a new option of type T with the given scope, list of alternative names,
     * behavior flags, default value, and description in short and long forms.
     */
    droption_t(unsigned int scope, std::vector<std::string> names, unsigned int flags,
               T defval, std::string desc_short, std::string desc_long)
        : droption_parser_t(scope, names, desc_short, desc_long, flags)
        , value_(defval)
        , defval_(defval)
        , valsep_(DROPTION_DEFAULT_VALUE_SEP)
        , has_range_(false)
    {
    }

    /**
     * Declares a new option of type T with the given scope, list of alternative names,
     * behavior flags,
     * accumulated value separator (see #DROPTION_FLAG_ACCUMULATE), default value, and
     * description in short and long forms. The first listed name is considered the
     * primary name; the others are aliases.
     */
    droption_t(unsigned int scope, std::vector<std::string> names, unsigned int flags,
               std::string valsep, T defval, std::string desc_short,
               std::string desc_long)
        : droption_parser_t(scope, names, desc_short, desc_long, 0)
        , value_(defval)
        , defval_(defval)
        , valsep_(DROPTION_DEFAULT_VALUE_SEP)
        , has_range_(false)
    {
    }

    /**
     * Declares a new option of type T with the given scope, list of alternative names,
     * default value, minimum and maximum values, and description in short and long forms.
     */
    droption_t(unsigned int scope, std::vector<std::string> names, T defval, T minval,
               T maxval, std::string desc_short, std::string desc_long)
        : droption_parser_t(scope, names, desc_short, desc_long, 0)
        , value_(defval)
        , defval_(defval)
        , valsep_(DROPTION_DEFAULT_VALUE_SEP)
        , has_range_(true)
        , minval_(minval)
        , maxval_(maxval)
    {
    }

    /** Returns the value of this option. */
    T
    get_value() const
    {
        return value_;
    }

    /** Resets the value of this option to the default value. */
    void
    clear_value() override
    {
        value_ = defval_;
        is_specified_ = false;
    }

    /** Returns the separator of the option value
     * (see #DROPTION_FLAG_ACCUMULATE).
     */
    std::string
    get_value_separator() const
    {
        return valsep_;
    }

    /** Sets the value of this option, overriding the command line. */
    void
    set_value(T new_value)
    {
        value_ = new_value;
    }

protected:
    bool
    clamp_value() override
    {
        if (has_range_) {
            if (value_ < minval_) {
                value_ = minval_;
                return false;
            } else if (value_ > maxval_) {
                value_ = maxval_;
                return false;
            }
        }
        return true;
    }

    /* Checks if the first non-space character of a string is a negative sign.
     * XXX: This function does not work with UTF-16/UTF-32 formatted strings.
     */
    static bool
    is_negative(const std::string &s)
    {
        for (size_t i = 0; i < s.size(); i++) {
            if (isspace(s[i]))
                continue;
            if (s[i] == '-')
                return true;
            break;
        }
        return false;
    }

    bool
    option_takes_arg() const override;
    bool
    option_takes_2args() const override;
    bool
    name_match(const char *arg) override;
    bool
    convert_from_string(const std::string s) override;
    bool
    convert_from_string(const std::string s1, const std::string s2) override;
    std::string
    default_as_string() const override;

    T value_;
    T defval_;
    std::string valsep_;
    bool has_range_;
    T minval_;
    T maxval_;
};

template <typename T>
inline bool
droption_t<T>::option_takes_arg() const
{
    return true;
}
template <>
inline bool
droption_t<bool>::option_takes_arg() const
{
    return false;
}

template <typename T>
inline bool
droption_t<T>::option_takes_2args() const
{
    return false;
}
template <>
inline bool
droption_t<twostring_t>::option_takes_2args() const
{
    return true;
}

template <typename T>
inline bool
droption_t<T>::name_match(const char *arg)
{
    for (const auto &name : names_) {
        if (std::string("-").append(name) == arg || std::string("--").append(name) == arg)
            return true;
    }
    return false;
}
template <>
inline bool
droption_t<bool>::name_match(const char *arg)
{
    for (const auto &name : names_) {
        if (std::string("-").append(name) == arg ||
            std::string("--").append(name) == arg) {
            value_ = true;
            return true;
        }
    }
    for (const auto &name : names_) {
        if (std::string("-no").append(name) == arg ||
            std::string("-no_").append(name) == arg ||
            std::string("--no").append(name) == arg ||
            std::string("--no_").append(name) == arg) {
            value_ = false;
            return true;
        }
    }
    return false;
}

template <>
inline bool
droption_t<std::string>::convert_from_string(const std::string s)
{
    if (TESTANY(DROPTION_FLAG_ACCUMULATE, flags_) && is_specified_) {
        value_ += valsep_ + s;
    } else
        value_ = s;
    return true;
}
template <>
inline bool
droption_t<int>::convert_from_string(const std::string s)
{
    errno = 0;
    // If we set 0 as the base, strtol() will automatically identify the base of the
    // number to convert. By default, it will assume the number to be converted is
    // decimal, and a number starting with 0 or 0x is assumed to be octal or hexadecimal,
    // respectively.
    long input = strtol(s.c_str(), NULL, 0);

    // strtol returns a long, but this may not always fit into an integer.
    if (input >= (long)INT_MIN && input <= (long)INT_MAX)
        value_ = (int)input;
    else
        return false;

    return errno == 0;
}
template <>
inline bool
droption_t<long>::convert_from_string(const std::string s)
{
    errno = 0;
    value_ = strtol(s.c_str(), NULL, 0);
    return errno == 0;
}
template <>
inline bool
droption_t<long long>::convert_from_string(const std::string s)
{
    errno = 0;
    // If we set 0 as the base, strtoll() will automatically identify the base.
    value_ = strtoll(s.c_str(), NULL, 0);
    return errno == 0;
}
template <>
inline bool
droption_t<unsigned int>::convert_from_string(const std::string s)
{
    errno = 0;
    long input = strtol(s.c_str(), NULL, 0);
    // Is the value positive and fits into an unsigned integer?
    if (input >= 0 && (unsigned long)input <= (unsigned long)UINT_MAX)
        value_ = (unsigned int)input;
    else
        return false;

    return errno == 0;
}
template <>
inline bool
droption_t<unsigned long>::convert_from_string(const std::string s)
{
    if (is_negative(s))
        return false;

    errno = 0;
    value_ = strtoul(s.c_str(), NULL, 0);
    return errno == 0;
}
template <>
inline bool
droption_t<unsigned long long>::convert_from_string(const std::string s)
{
    if (is_negative(s))
        return false;

    errno = 0;
    value_ = strtoull(s.c_str(), NULL, 0);
    return errno == 0;
}
template <>
inline bool
droption_t<double>::convert_from_string(const std::string s)
{
    // strtod will return 0.0 for invalid conversions
    char *pEnd = NULL;
    value_ = strtod(s.c_str(), &pEnd);
    return true;
}
template <>
inline bool
droption_t<bool>::convert_from_string(const std::string s)
{
    // We shouldn't get here
    return false;
}
template <>
inline bool
droption_t<bytesize_t>::convert_from_string(const std::string s)
{
    char suffix = *s.rbegin(); // s.back() only in C++11
    int scale;
    switch (suffix) {
    case 'K':
    case 'k': scale = 1024; break;
    case 'M':
    case 'm': scale = 1024 * 1024; break;
    case 'G':
    case 'g': scale = 1024 * 1024 * 1024; break;
    default: scale = 1;
    }
    std::string toparse = s;
    if (scale > 1)
        toparse = s.substr(0, s.size() - 1); // s.pop_back() only in C++11
    long long input = atoll(toparse.c_str());
    if (input >= 0)
        value_ = (uint64_t)input * scale;
    else {
        value_ = 0;
        return false;
    }
    return true;
}
template <>
inline bool
droption_t<twostring_t>::convert_from_string(const std::string s)
{
    return false;
}

// Default implementations

template <typename T>
inline bool
droption_t<T>::convert_from_string(const std::string sc)
{
    return false;
}

template <typename T>
inline bool
droption_t<T>::convert_from_string(const std::string s1, const std::string s2)
{
    return false;
}

template <typename T>
inline std::string
droption_t<T>::default_as_string() const
{
#if __cplusplus >= 201103L
    static_assert(sizeof(T) == -1, "Use of unsupported droption_t type");
#else
    DR_ASSERT_MSG(false, "Use of unsupported droption_t type");
#endif
    return std::string();
}

template <>
inline bool
droption_t<std::string>::convert_from_string(const std::string s1, const std::string s2)
{
    // This is for the sweeper
    if (TESTANY(DROPTION_FLAG_ACCUMULATE, flags_) && is_specified_) {
        value_ += valsep_ + s1 + valsep_ + s2;
        return true;
    } else
        return false;
}
template <>
inline bool
droption_t<twostring_t>::convert_from_string(const std::string s1, const std::string s2)
{
    if (TESTANY(DROPTION_FLAG_ACCUMULATE, flags_) && is_specified_) {
        value_.first += valsep_ + s1;
        value_.second += valsep_ + s2;
    } else {
        value_.first = s1;
        value_.second = s2;
    }
    return true;
}

template <>
inline std::string
droption_t<std::string>::default_as_string() const
{
    return defval_.empty() ? "\"\"" : defval_;
}
template <>
inline std::string
droption_t<int>::default_as_string() const
{
    std::ostringstream stream;
    stream << std::dec << defval_;
    return stream.str();
}
template <>
inline std::string
droption_t<long>::default_as_string() const
{
    std::ostringstream stream;
    stream << std::dec << defval_;
    return stream.str();
}
template <>
inline std::string
droption_t<long long>::default_as_string() const
{
    std::ostringstream stream;
    stream << std::dec << defval_;
    return stream.str();
}
template <>
inline std::string
droption_t<unsigned int>::default_as_string() const
{
    std::ostringstream stream;
    stream << std::dec << defval_;
    return stream.str();
}
template <>
inline std::string
droption_t<unsigned long>::default_as_string() const
{
    std::ostringstream stream;
    stream << std::dec << defval_;
    return stream.str();
}
template <>
inline std::string
droption_t<unsigned long long>::default_as_string() const
{
    std::ostringstream stream;
    stream << std::dec << defval_;
    return stream.str();
}
template <>
inline std::string
droption_t<double>::default_as_string() const
{
    std::ostringstream stream;
    stream << std::dec << defval_;
    return stream.str();
}
template <>
inline std::string
droption_t<bool>::default_as_string() const
{
    return (defval_ ? "true" : "false");
}
template <>
inline std::string
droption_t<bytesize_t>::default_as_string() const
{
    uint64_t val = defval_;
    std::string suffix = "";
    if (defval_ >= 1024 * 1024 * 1024 && defval_ % 1024 * 1024 * 1024 == 0) {
        suffix = "G";
        val /= 1024 * 1024 * 1024;
    } else if (defval_ >= 1024 * 1024 && defval_ % 1024 * 1024 == 0) {
        suffix = "M";
        val /= 1024 * 1024;
    } else if (defval_ >= 1024 && defval_ % 1024 == 0) {
        suffix = "K";
        val /= 1024;
    }
    std::ostringstream stream;
    stream << std::dec << val;
    return stream.str() + suffix;
}
template <>
inline std::string
droption_t<twostring_t>::default_as_string() const
{
    return (defval_.first.empty() ? "\"\"" : defval_.first) + " " +
        (defval_.second.empty() ? "\"\"" : defval_.second);
}

// Convenience routine for client use
#ifdef DYNAMORIO_API
/**
 * Parses the options for client \p client_id and fills in any registered
 * droption_t class fields.
 * On success, returns true, with the index of the start of the remaining
 * unparsed options, if any, returned in \p last_index (typically this
 * will be options separated by "--").
 * On failure, returns false, and if \p error_msg != NULL, stores a string
 * describing the error there.  On failure, \p last_index is set to the
 * index of the problematic option or option value.
 *
 * \deprecated This routine is not needed with the new dr_client_main()
 * where droption_parser_t::parse_argv() can be invoked directly.
 */
static inline bool
dr_parse_options(client_id_t client_id, std::string *error_msg, int *last_index)
{
    int argc;
    const char **argv;
    bool res = dr_get_option_array(client_id, &argc, &argv);
    if (!res)
        return false;
    return droption_parser_t::parse_argv(DROPTION_SCOPE_CLIENT, argc, argv, error_msg,
                                         last_index);
}
#endif

} // namespace droption
} // namespace dynamorio

#endif /* _DROPTION_H_ */
