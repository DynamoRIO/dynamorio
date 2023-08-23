// The program extracts STROFF(foo) strings from source files
// and generates a single global concatenated string STROFF_foo defines.
// It was run as:
//	go run make/stroff_gen.go -hdr=core/ir/stroff.h -src=core/ir/stroff.c \
//		core/ir/x86/decode_table.c core/ir/x86/decode.c
// You can see the results in the corresponding files.

package main

import (
	"flag"
	"fmt"
	"os"
	"regexp"
	"sort"
	"strings"
)

const macroName = "STROFF"

type Val struct {
	str    string
	off    int
	suffix *Val
}

func main() {
	flagHeader := flag.String("hdr", "", "output header file name")
	flagSource := flag.String("src", "", "output source file name")
	flag.Parse()
	var vals []*Val
	dedup := make(map[string]bool)
	re := regexp.MustCompile(macroName + `\((.*?)\)`)
	for _, file := range flag.Args() {
		data, err := os.ReadFile(file)
		if err != nil {
			panic(err)
		}
		for _, match := range re.FindAllSubmatch(data, -1) {
			str := string(match[1])
			if dedup[str] {
				continue
			}
			dedup[str] = true
			vals = append(vals, &Val{str: str})
		}
	}
	sort.Slice(vals, func(i, j int) bool {
		return vals[i].str < vals[j].str
	})
	off := 0
next:
	for _, val := range vals {
		for _, other := range vals {
			if val != other && strings.HasSuffix(other.str, val.str) {
				val.suffix = other
				continue next
			}
		}
		val.off = off
		off += len(val.str) + 1
	}
	for _, val := range vals {
		if val.suffix == nil {
			continue
		}
		for val.suffix.suffix != nil {
			val.suffix = val.suffix.suffix
		}
		val.off = val.suffix.off + len(val.suffix.str) - len(val.str)
	}
	hdr, err := os.Create(*flagHeader)
	if err != nil {
		panic(err)
	}
	src, err := os.Create(*flagSource)
	if err != nil {
		panic(err)
	}
	typ := "int"
	if off <= 1<<16 {
		typ = "short"
	}
	fmt.Fprintf(hdr, "typedef unsigned %s stroff_t;\n", typ)
	fmt.Fprintf(hdr, "static const char* stroffstr(stroff_t off)\n{\n")
	fmt.Fprintf(hdr, "	extern const char stroff_data[];\n")
	fmt.Fprintf(hdr, "	return stroff_data + off;\n")
	fmt.Fprintf(hdr, "}\n")
	fmt.Fprintf(hdr, "#define STROFF(str) STROFF_ ## str\n")
	for _, val := range vals {
		fmt.Fprintf(hdr, "#define %v_%v %v\n", macroName, val.str, val.off)
	}
	fmt.Fprintf(src, "const char stroff_data[] =\n")
	for _, val := range vals {
		if val.suffix != nil {
			continue
		}
		fmt.Fprintf(src, "\t/* %v */ \"%v\\0\"\n", val.off, val.str)
	}
	fmt.Fprintf(src, ";\n")
}
