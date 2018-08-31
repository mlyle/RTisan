#ifndef __RTISAN_ASSERT_H
#define __RTISAN_ASSERT_H

#define assert(x) ((x) ? (void)0 : __assert_impl(__FILE__, __LINE__, #x))

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

inline void abort(void)
{
	while (1);
}

#endif /* __RTISAN_ASSERT_H */
