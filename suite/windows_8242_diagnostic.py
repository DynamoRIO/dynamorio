"""Temporary diagnostic harness for DynamoRIO PR #8242."""
from pathlib import Path
import subprocess
import sys


def prepare(variant):
    path = Path("clients/drcachesim/tracer/instru_offline.cpp")
    text = path.read_text()
    if variant == "mutex":
        for operation in ("create", "destroy", "write_lock", "write_unlock",
                          "read_lock", "read_unlock", "self_owns_write_lock"):
            replacement = {"write_lock": "lock", "write_unlock": "unlock",
                           "read_lock": "lock", "read_unlock": "unlock",
                           "self_owns_write_lock": "self_owns"}.get(operation, operation)
            text = text.replace("dr_rwlock_" + operation + "(",
                                "dr_mutex_" + replacement + "(")
    # Keep diagnostics out of stdout/stderr: the tests match those exactly.
    helper = r"""
static void
log_8242(const char *stage)
{
    char name[128];
    dr_snprintf(name, sizeof(name), "8242-%d.log", (int)dr_get_process_id());
    file_t out = dr_open_file(name, DR_FILE_WRITE_APPEND | DR_FILE_ALLOW_LARGE);
    if (out != INVALID_FILE) {
        dr_fprintf(out, "%s\n", stage);
        dr_close_file(out);
    }
}

"""
    text = text.replace("static const uint MAX_INSTR_COUNT", helper + "static const uint MAX_INSTR_COUNT", 1)
    lock_create = "dr_mutex_create()" if variant == "mutex" else "dr_rwlock_create()"
    lock_acquire = "dr_mutex_lock(encoding_lock_);" if variant == "mutex" else "dr_rwlock_write_lock(encoding_lock_);"
    text = text.replace("    encoding_lock_ = " + lock_create + ";",
                        '    log_8242("ctor: before lock create");\n    encoding_lock_ = ' + lock_create + ';\n    log_8242("ctor: after lock create");', 1)
    text = text.replace("    hashtable_init_ex(&modoffs2pc_", '    log_8242("ctor: before table init");\n    hashtable_init_ex(&modoffs2pc_', 1)
    text = text.replace("/*hash_key_func=*/nullptr, /*cmp_key_func=*/nullptr);",
                        '/*hash_key_func=*/nullptr, /*cmp_key_func=*/nullptr);\n    log_8242("ctor: after table init");', 1)
    text = text.replace("    hashtable_delete(&modoffs2pc_);",
                        '    log_8242("dtor: before table delete");\n    hashtable_delete(&modoffs2pc_);\n    log_8242("dtor: after table delete");', 1)
    text = text.replace("    " + lock_acquire + "\n    flush_instr_encodings();",
                        '    log_8242("dtor: before lock acquire");\n    ' + lock_acquire + '\n    log_8242("dtor: after lock acquire");\n    flush_instr_encodings();\n    log_8242("dtor: after flush");', 1)
    text = text.replace("    log_(1, \"Wrote ", '    log_8242("dtor: after lock destroy");\n    log_(1, "Wrote ', 1)
    text = text.replace("    " + lock_acquire + "\n    log_(3,",
                        '    if (encoding_id_ == 0)\n        log_8242("record: before first lock acquire");\n    ' + lock_acquire + '\n    if (encoding_id_ == 0)\n        log_8242("record: after first lock acquire");\n    log_(3,', 1)
    path.write_text(text)
    print(subprocess.check_output(["git", "diff", "--", str(path)], text=True))


def run_tests():
    build = Path("build_debug-internal-64")
    command = ["ctest", "--test-dir", str(build), "-VV", "--no-tests=error", "-j1",
               "-R", r"^code_api[|]tool[.]drcacheoff[.](simple|filter)$", "--timeout", "90"]
    result = 1
    with open("windows-8242-test.log", "w") as out:
        proc = subprocess.Popen(command, stdout=out, stderr=subprocess.STDOUT)
        try:
            result = proc.wait(timeout=240)
        except subprocess.TimeoutExpired:
            print("CTest exceeded 240 seconds; capturing processes and killing its tree.")
            subprocess.run(["taskkill", "/PID", str(proc.pid), "/T", "/F"], timeout=30)
            proc.wait(timeout=30)
        finally:
            with open("windows-8242-processes.log", "w") as processes:
                subprocess.run(["powershell", "-NoProfile", "-Command",
                    "Get-CimInstance Win32_Process | Where-Object { $_.Name -match 'simple_app|drrun|ctest|cmake' } | Select-Object ProcessId,ParentProcessId,Name,CommandLine | Format-List"],
                    stdout=processes, stderr=subprocess.STDOUT, timeout=30)
            # A dedicated hosted VM runs only these tests; clean up any orphaned app.
            subprocess.run(["taskkill", "/IM", "simple_app.exe", "/T", "/F"], timeout=30)
    print(Path("windows-8242-test.log").read_text(errors="replace"))
    print(Path("windows-8242-processes.log").read_text(errors="replace"))
    for log in sorted((build / "suite/tests").glob("8242-*.log")):
        print("DIAGNOSTIC", log)
        print(log.read_text(errors="replace"))
    return result


if __name__ == "__main__":
    if sys.argv[1] == "prepare":
        prepare(sys.argv[2])
    else:
        sys.exit(run_tests())
