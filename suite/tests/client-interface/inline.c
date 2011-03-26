/* TODO(rnk): Copyright */

/* Most functions here need to be exported and not inlined so that we can
 * find their addresses via symbol lookup and instrument them.
 */
#ifdef WINDOWS
#define EXPORT __declspec(dllexport)
#define NOINLINE __declspec(noinline)
#else
#define EXPORT __attribute__((visibility("default")))
#define NOINLINE __attribute__((noinline))
#endif

EXPORT NOINLINE void empty() {}
EXPORT NOINLINE void inscount() {}
EXPORT NOINLINE void callpic_pop() {}
EXPORT NOINLINE void callpic_mov() {}
EXPORT NOINLINE void cond_br() {}
EXPORT NOINLINE void tls_clobber() {}
EXPORT NOINLINE void nonleaf() {}

int
main(void)
{
    empty();
    inscount();
    callpic_pop();
    callpic_mov();
    cond_br();
    tls_clobber();
    nonleaf();
}
