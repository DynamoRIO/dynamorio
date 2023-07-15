// The program converts opcode name strings "foo" to STROFF(foo).
// Also containts rules for conversion of strings that are not
// valid C identifiers.
// This is supposed to be used once to create the change,
// and then thrown away (not committed).

package main

import (
	"bufio"
	"bytes"
	"fmt"
	"os"
	"regexp"
	"strings"
)

const macroName = "STROFF"

func main() {
	data, err := os.ReadFile(os.Args[1])
	if err != nil {
		panic(err)
	}
	extracter := regexp.MustCompile(`.*{.*?(".*?").*`)
	checker := regexp.MustCompile(`^[a-zA-Z0-9_]+$`)
	converter := strings.NewReplacer(
		"(bad)", "BAD",
		" + ", "_",
		" ", "_",
		"+", "_",
		"/", "_",
		".", "",
		"(", "",
		")", "",
		"<", "",
		">", "",
		"*", "",
		"!", "",
		"cont'd", "cont",
	)
	out := new(bytes.Buffer)
	for s := bufio.NewScanner(bytes.NewReader(data)); s.Scan(); {
		ln := s.Bytes()
		match := extracter.FindSubmatchIndex(ln)
		if match == nil {
			out.Write(ln)
		} else {
			orig := string(ln[match[2]+1 : match[3]-1])
			res := converter.Replace(orig)
			if !checker.MatchString(res) {
				fmt.Fprintf(os.Stderr, "invalid string %q -> %q\n", orig, res)
				os.Exit(1)
			}
			out.Write(ln[:match[2]])
			fmt.Fprintf(out, "%v(%v)", macroName, res)
			out.Write(ln[match[3]:])
		}
		out.WriteByte('\n')
	}
	if err := os.WriteFile(os.Args[1], out.Bytes(), 0664); err != nil {
		panic(err)
	}
}
