/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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
} droption_flags_t;

// XXX: for tools who want to generate html docs from the same option spec,
// could we provide a solution where at build time some code is run to
// produce the docs?  That is one area where an optionsx.h C macro style
// solution is superior to this C++ approach.

/**
 * The bytesize_t class exists to provide an option type that accepts suffixes
 * like 'K', 'M', and 'G' when specifying sizes in units of bytes.
 */
class bytesize_t
{
 public:
    bytesize_t() : size(0) {}
    bytesize_t(unsigned int val) : size(val) {}
    operator unsigned int() const { return size; }
    unsigned int size;
};

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
                        if (i == argc) {
                            if (error_msg != NULL)
                                *error_msg = "Option " + op->name + " missing value";
                            res = false;
                            goto parse_finished;
                        }
                        if (matched) {
                            if (!op->convert_from_string(argv[i]) ||
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
                            if (!sweeper()->convert_from_string(argv[i]) ||
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
            if (TESTANY(scope, op->scope)) {
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
     * default values, and their long descriptions.
     */
    static std::string
    usage_long(unsigned int scope)
    {
        std::ostringstream oss;
        for (std::vector<droption_parser_t*>::iterator opi = allops().begin();
             opi != allops().end();
             ++opi) {
            droption_parser_t *op = *opi;
            // XXX: we should also add the min and max values
            if (TESTANY(scope, op->scope)) {
                oss << "----------" << std::endl
                    << "-" << op->name << std::endl
                    << "default value: "
                    << op->default_as_string() << std::endl
                    << op->desc_long << std::endl << std::endl;
            }
        }
        oss << "----------" << std::endl;
        return oss.str();
    }

    /** Returns whether this option was specified in the argument list. */
    bool specified() { return is_specified; }
    /** Returns the name of this option. */
    std::string get_name() { return name; }

 protected:
    virtual bool option_takes_arg() const = 0;
    virtual bool name_match(const char *arg) = 0; // also sets value for bools!
    virtual bool convert_from_string(const std::string s) = 0;
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
    T get_value() { return value; }

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
    bool name_match(const char *arg);
    bool convert_from_string(const std::string s);
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
    int input = atoi(toparse.c_str());
    if (input >= 0)
        value = input * scale;
    else {
        value = 0;
        return false;
    }
    return true;
}

template<> inline std::string
droption_t<std::string>::default_as_string() const
{
    return defval;
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
    unsigned int val = defval;
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
 */
static inline bool
dr_parse_options(client_id_t client_id, std::string *error_msg, int *last_index)
{
    int argc;
    const char **argv;
    bool res = dr_get_option_array(client_id, &argc, &argv, MAXIMUM_PATH);
    if (!res)
        return false;
    res = droption_parser_t::parse_argv(DROPTION_SCOPE_CLIENT, argc, argv,
                                        error_msg, last_index);
    if (res)
        res = dr_free_option_array(argc, argv);
    return res;
}
#endif

#endif /* _DROPTION_H_ */
