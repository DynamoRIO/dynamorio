/* **********************************************************
 * Copyright (c) 2015-2017 Google, Inc.  All rights reserved.
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

/* droption: option parsing support */

#ifndef _DROPTION_H_
#define _DROPTION_H_ 1

#include <string>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include <sstream>
#include <iomanip>
#include <stdint.h> /* for supporting 64-bit integers*/

#define TESTALL(mask, var) (((mask) & (var)) == (mask))
#define TESTANY(mask, var) (((mask) & (var)) != 0)

// XXX: some clients want further distinctions, such as options passed to
// post-processing components, internal (i.e., undocumented) options, etc.
/**
 * These bitfield values specify whether an option is intended for use
 * by a client for a tool frontend, or both.
 */
typedef enum {
    DROPTION_SCOPE_CLIENT    = 0x0001,   /**< Acted on by the client only. */
    DROPTION_SCOPE_FRONTEND  = 0x0002, /**< Acted on by the frontend only. */
    /** Acted on by both client and frontend. */
    DROPTION_SCOPE_ALL       = (DROPTION_SCOPE_CLIENT|DROPTION_SCOPE_FRONTEND),
} droption_scope_t;

/**
 * These bitfield flags further specify the behavior of an option.
 */
typedef enum {
    /**
     * By default, if an option is specified multiple times on the
     * command line, only the last value is honored.  If this flag is
     * set, repeated options accumulate, appending to the prior value
     * (separating each appended value with a space).  This is
     * supported for options of type std::string only.
     */
    // XXX: to support other types of accumulation, we should add explicit
    // support for dr_option_t<std::vector<std::string> >.
    DROPTION_FLAG_ACCUMULATE     = 0x0001,
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
    DROPTION_FLAG_SWEEP          = 0x0002,
    /**
     * Indicates that this is an internal option and should be excluded from
     * usage messages and documentation.
     */
    DROPTION_FLAG_INTERNAL       = 0x0004,
} droption_flags_t;

/**
 * The bytesize_t class exists to provide an option type that accepts suffixes
 * like 'K', 'M', and 'G' when specifying sizes in units of bytes.
 */
class bytesize_t
{
 public:
    bytesize_t() : size(0) {}
    // The bytesize_t class is backed by a 64-bit unsigned integer.
    bytesize_t(uint64_t val) : size(val) {}
    operator uint64_t() const { return size; }
    uint64_t size;
};

/** A convenience typedef for options that take in pairs of values. */
typedef std::pair<std::string, std::string> twostring_t;

/**
 * Option parser base class.
 */
class droption_parser_t
{
 public:
    droption_parser_t(unsigned int scope_, std::string name_,
                      std::string desc_short_, std::string desc_long_,
                      unsigned int flags_)
        : scope(scope_), name(name_), is_specified(false),
        desc_short(desc_short_), desc_long(desc_long_), flags(flags_)
    {
        // We assume no synch is needed as this is a static initializer.
        // XXX: any way to check/assert on that?
        allops().push_back(this);
        if (TESTANY(DROPTION_FLAG_SWEEP, flags))
            sweeper() = this;
    }

    // We do not provide a string-parsing routine as we assume a client will
    // use dr_get_option_array() to convert to an argv[] array, or use our
    // provided routine dr_parse_options().

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
     * We recommend that Windows standalone applications use UNICODE and
     * call drfront_convert_args() to convert to UTF-8 prior to passing here,
     * for proper internationalization support.
     */
    static bool
    parse_argv(unsigned int scope, int argc, const char *argv[],
               std::string *error_msg, int *last_index)
    {
        int i;
        bool res = true;
        for (i = 1/*skip app*/; i < argc; ++i) {
            // We support the universal "--" as a separator
            if (strcmp(argv[i], "--") == 0) {
                ++i; // for last_index
                break;
            }
            bool matched = false;
            bool swept = false;
            for (std::vector<droption_parser_t*>::iterator opi = allops().begin();
                 opi != allops().end();
                 ++opi) {
                droption_parser_t *op = *opi;
                // We parse other-scope options and their values, for sweeping.
                if (op->name_match(argv[i])) {
                    if (TESTANY(scope, op->scope))
                        matched = true;
                    if (sweeper() != NULL &&
                        (!matched ||
                         // Sweep up both-scope options as well as ummatched
                         TESTALL(DROPTION_SCOPE_ALL, op->scope)) &&
                        sweeper()->convert_from_string(argv[i]) &&
                        sweeper()->clamp_value()) {
                        sweeper()->is_specified = true; // *after* convert_from_string()
                        swept = true;
                    }
                    if (op->option_takes_arg()) {
                        ++i;
                        if (op->option_takes_2args() && i < argc)
                            ++i;
                        if (i == argc) {
                            if (error_msg != NULL)
                                *error_msg = "Option " + op->name + " missing value";
                            res = false;
                            goto parse_finished;
                        }
                        if (matched) {
                            if ((!op->option_takes_2args() &&
                                 !op->convert_from_string(argv[i])) ||
                                (op->option_takes_2args() &&
                                 !op->convert_from_string(argv[i-1], argv[i])) ||
                                !op->clamp_value()) {
                                if (error_msg != NULL) {
                                    *error_msg = "Option " + op->name +
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
                                 !sweeper()->convert_from_string(argv[i-1], argv[i])) ||
                                !sweeper()->clamp_value()) {
                                if (error_msg != NULL) {
                                    *error_msg = "Option " + op->name +
                                        " value out of range";
                                }
                                res = false;
                                goto parse_finished;
                            }
                        }
                    }
                    if (matched)
                        op->is_specified = true; // *after* convert_from_string()
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
        for (std::vector<droption_parser_t*>::iterator opi = allops().begin();
             opi != allops().end();
             ++opi) {
            droption_parser_t *op = *opi;
            if (!TESTALL(DROPTION_FLAG_INTERNAL, op->flags) &&
                TESTANY(scope, op->scope)) {
                oss << " -" << std::setw(20) << std::left << op->name
                    << "[" << std::setw(6) << std::right
                    << op->default_as_string() << "]"
                    << "  " << std::left << op->desc_short << std::endl;
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
    usage_long(unsigned int scope,
               std::string pre_name = "----------\n", std::string post_name = "\n",
               std::string pre_value = "", std::string post_value = "\n",
               std::string pre_desc = "", std::string post_desc = "\n")
    {
        std::ostringstream oss;
        for (std::vector<droption_parser_t*>::iterator opi = allops().begin();
             opi != allops().end();
             ++opi) {
            droption_parser_t *op = *opi;
            // XXX: we should also add the min and max values
            if (!TESTALL(DROPTION_FLAG_INTERNAL, op->flags) &&
                TESTANY(scope, op->scope)) {
                oss << pre_name << "-" << op->name << post_name
                    << pre_value << "default value: "
                    << op->default_as_string() << post_value
                    << pre_desc << op->desc_long << post_desc << std::endl;
            }
        }
        return oss.str();
    }

    /** Returns whether this option was specified in the argument list. */
    bool specified() { return is_specified; }
    /** Returns the name of this option. */
    std::string get_name() { return name; }

 protected:
    virtual bool option_takes_arg() const = 0;
    virtual bool option_takes_2args() const = 0;
    virtual bool name_match(const char *arg) = 0; // also sets value for bools!
    virtual bool convert_from_string(const std::string s) = 0;
    virtual bool convert_from_string(const std::string s1, const std::string s2) = 0;
    virtual bool clamp_value() = 0;
    virtual std::string default_as_string() const = 0;

    // To avoid static initializer ordering problems we use a function:
    static std::vector<droption_parser_t*>& allops()
    {
        static std::vector<droption_parser_t*> allops_vec;
        return allops_vec;
    }
    static droption_parser_t *& sweeper()
    {
        static droption_parser_t *global_sweeper;
        return global_sweeper;
    }

    unsigned int scope; // made up of droption_scope_t bitfields
    std::string name;
    bool is_specified;
    std::string desc_short;
    std::string desc_long;
    unsigned int flags;
};

/** Option class for declaring new options. */
template <typename T> class droption_t : public droption_parser_t
{
 public:
    /**
     * Declares a new option of type T with the given scope, default value,
     * and description in short and long forms.
     */
    droption_t(unsigned int scope_, std::string name_, T defval_,
               std::string desc_short_, std::string desc_long_)
        : droption_parser_t(scope_, name_, desc_short_, desc_long_, 0),
        value(defval_), defval(defval_), has_range(false) {}

    /**
     * Declares a new option of type T with the given scope, behavior flags,
     * default value, and description in short and long forms.
     */
    droption_t(unsigned int scope_, std::string name_, unsigned int flags_,
               T defval_, std::string desc_short_, std::string desc_long_)
        : droption_parser_t(scope_, name_, desc_short_, desc_long_, flags_),
        value(defval_), defval(defval_), has_range(false) {}

    /**
     * Declares a new option of type T with the given scope, default value,
     * minimum and maximum values, and description in short and long forms.
     */
    droption_t(unsigned int scope_, std::string name_, T defval_,
               T minval_, T maxval_,
               std::string desc_short_, std::string desc_long_)
        : droption_parser_t(scope_, name_, desc_short_, desc_long_, 0),
        value(defval_), defval(defval_), has_range(true),
        minval(minval_), maxval(maxval_) {}

    /** Returns the value of this option. */
    T get_value() const { return value; }

    /** Sets the value of this option, overriding the command line. */
    void set_value(T new_value) { value = new_value; }

 protected:
    bool clamp_value()
    {
        if (has_range) {
            if (value < minval) {
                value = minval;
                return false;
            } else if (value > maxval) {
                value = maxval;
                return false;
            }
        }
        return true;
    }

    bool option_takes_arg() const;
    bool option_takes_2args() const;
    bool name_match(const char *arg);
    bool convert_from_string(const std::string s);
    bool convert_from_string(const std::string s1, const std::string s2);
    std::string default_as_string() const;

    T value;
    T defval;
    bool has_range;
    T minval;
    T maxval;
};

template <typename T> inline bool droption_t<T>::option_takes_arg() const { return true; }
template<> inline bool droption_t<bool>::option_takes_arg() const { return false; }

template <typename T> inline bool
droption_t<T>::option_takes_2args() const
{
    return false;
}
template<> inline bool
droption_t<twostring_t>::option_takes_2args() const
{
    return true;
}

template <typename T> inline bool
droption_t<T>::name_match(const char *arg)
{
    return (std::string("-").append(name) == arg ||
            std::string("--").append(name) == arg);
}
template<> inline bool
droption_t<bool>::name_match(const char *arg)
{
    if (std::string("-").append(name) == arg ||
        std::string("--").append(name) == arg) {
        value = true;
        return true;
    }
    if (std::string("-no").append(name) == arg ||
        std::string("-no_").append(name) == arg ||
        std::string("--no").append(name) == arg ||
        std::string("--no_").append(name) == arg) {
        value = false;
        return true;
    }
    return false;
}

template<> inline bool
droption_t<std::string>::convert_from_string(const std::string s)
{
    if (TESTANY(DROPTION_FLAG_ACCUMULATE, flags) && is_specified) {
        // We hardcode a space separator for string accumulations.
        // The user can use a vector of strings for other uses.
        value += " " + s;
    } else
        value = s;
    return true;
}
template<> inline bool
droption_t<int>::convert_from_string(const std::string s)
{
    value = atoi(s.c_str());
    return true;
}
template<> inline bool
droption_t<unsigned int>::convert_from_string(const std::string s)
{
    int input = atoi(s.c_str());
    if (input >= 0)
        value = input;
    else {
        value = 0;
        return false;
    }
    return true;
}
template<> inline bool
droption_t<bool>::convert_from_string(const std::string s)
{
    // We shouldn't get here
    return false;
}
template<> inline bool
droption_t<bytesize_t>::convert_from_string(const std::string s)
{
    char suffix = *s.rbegin(); // s.back() only in C++11
    int scale;
    switch (suffix) {
    case 'K':
    case 'k': scale = 1024; break;
    case 'M':
    case 'm': scale = 1024*1024; break;
    case 'G':
    case 'g': scale = 1024*1024*1024; break;
    default: scale = 1;
    }
    std::string toparse = s;
    if (scale > 1)
        toparse = s.substr(0, s.size()-1); // s.pop_back() only in C++11
    // While the overall size is likely too large to be represented
    // by a 32-bit integer, the prefix number is usually not.
    int input = atoi(toparse.c_str());
    if (input >= 0)
        value = (uint64_t)input * scale;
    else {
        value = 0;
        return false;
    }
    return true;
}
template<> inline bool
droption_t<twostring_t>::convert_from_string(const std::string s)
{
    return false;
}

template <typename T> inline bool
droption_t<T>::convert_from_string(const std::string s1, const std::string s2)
{
    return false;
}
template<> inline bool
droption_t<std::string>::convert_from_string(const std::string s1, const std::string s2)
{
    // This is for the sweeper
    if (TESTANY(DROPTION_FLAG_ACCUMULATE, flags) && is_specified) {
        value += " " + s1 + " " + s2;
        return true;
    } else
        return false;
}
template<> inline bool
droption_t<twostring_t>::convert_from_string(const std::string s1, const std::string s2)
{
    if (TESTANY(DROPTION_FLAG_ACCUMULATE, flags) && is_specified) {
        // Just like for single strings, we hardcode a space separator.
        value.first += " " + s1;
        value.second += " " + s2;
    } else {
        value.first = s1;
        value.second = s2;
    }
    return true;
}

template<> inline std::string
droption_t<std::string>::default_as_string() const
{
    return defval.empty() ? "\"\"" : defval;
}
template<> inline std::string
droption_t<int>::default_as_string() const
{
    return dynamic_cast< std::ostringstream & >
        ((std::ostringstream() << std::dec << defval)).str();
}
template<> inline std::string
droption_t<unsigned int>::default_as_string() const
{
    return dynamic_cast< std::ostringstream & >
        ((std::ostringstream() << std::dec << defval)).str();
}
template<> inline std::string
droption_t<bool>::default_as_string() const
{
    return (defval ? "true" : "false");
}
template<> inline std::string
droption_t<bytesize_t>::default_as_string() const
{
    uint64_t val = defval;
    std::string suffix = "";
    if (defval >= 1024*1024*1024 && defval % 1024*1024*1024 == 0) {
        suffix = "G";
        val /= 1024*1024*1024;
    } else if (defval >= 1024*1024 && defval % 1024*1024 == 0) {
        suffix = "M";
        val /= 1024*1024;
    } else if (defval >= 1024 && defval % 1024 == 0) {
        suffix = "K";
        val /= 1024;
    }
    return dynamic_cast< std::ostringstream & >
        ((std::ostringstream() << std::dec << val)).str() + suffix;
}
template<> inline std::string
droption_t<twostring_t>::default_as_string() const
{
    return (defval.first.empty() ? "\"\"" : defval.first) + " " +
        (defval.second.empty() ? "\"\"" : defval.second);
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
    return droption_parser_t::parse_argv(DROPTION_SCOPE_CLIENT, argc, argv,
                                         error_msg, last_index);
}
#endif

#endif /* _DROPTION_H_ */
