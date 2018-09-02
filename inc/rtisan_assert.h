#ifndef __RTISAN_ASSERT_H
#define __RTISAN_ASSERT_H

#define assert(x) ((x) ? (void)0 : __assert_impl(__FILE__, __LINE__, #x))

#define abort rtisan_abort

static inline void abort(void)
{
	while (1);
}

static inline void __assert_impl(const char *fn, const int ln,
		const char *expr)
{
#ifdef RT_EXTENDED_DEBUG
	fprintf(stderr, "*** %s:%d assert (%s) ***\n",
			fn, ln, expr);
#else
	(void) fn; (void) ln; (void) expr;
#endif

	abort();
}

#endif /* __RTISAN_ASSERT_H */
