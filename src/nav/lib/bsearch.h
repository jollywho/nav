#ifndef FN_BSEARCH_H
#define FN_BSEARCH_H

#include <stddef.h>

static inline void* bsearch_r(
	register const void *key,
	const void *base0,
	size_t nmemb,
	register size_t size,
	register int (*compar)(const void *, const void *, void *),
  void *arg)
{
	register const char *base = base0;
	register size_t lim;
	register int cmp;
	register const void *p;

	for (lim = nmemb; lim != 0; lim >>= 1) {
		p = base + (lim >> 1) * size;
		cmp = (*compar)(key, p, arg);
		if (cmp == 0)
			return ((void *)p);
		if (cmp > 0) {
			base = (char *)p + size;
			lim--;
		}
	}
	return (NULL);
}

#endif
