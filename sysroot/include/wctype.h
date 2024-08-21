#ifndef __WCTYPE_H
#define __WCTYPE_H 1

typedef unsigned int wint_t;
typedef unsigned int wctype_t;
typedef unsigned int wctrans_t;

#define WEOF (-1)

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef __STDC_HOSTED__

int       iswalnum(wint_t);
int       iswalpha(wint_t);
int       iswblank(wint_t);
int       iswcntrl(wint_t);
int       iswdigit(wint_t);
int       iswgraph(wint_t);
int       iswlower(wint_t);
int       iswprint(wint_t);
int       iswpunct(wint_t);
int       iswspace(wint_t);
int       iswupper(wint_t);
int       iswxdigit(wint_t);
int       iswctype(wint_t, wctype_t);
wint_t    towctrans(wint_t, wctrans_t);
wint_t    towlower(wint_t);
wint_t    towupper(wint_t);
wctrans_t wctrans(const char *);
wctype_t  wctype(const char *);

#endif

#if defined(__cplusplus)
}
#endif

#endif // _WCTYPE_H