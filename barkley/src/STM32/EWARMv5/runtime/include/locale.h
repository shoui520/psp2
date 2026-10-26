/*
 * locale.h: the C library's locale (IAR DLib, Dinkumware's): the numeric and monetary
 * conventions, in Dinkumware's order.
 */
#ifndef _LOCALE_H
#define _LOCALE_H

struct lconv {
	/* LC_MONETARY */
	char *currency_symbol;
	char *int_curr_symbol;
	char *mon_decimal_point;
	char *mon_grouping;
	char *mon_thousands_sep;
	char *negative_sign;
	char *positive_sign;
	char frac_digits;
	char n_cs_precedes;
	char n_sep_by_space;
	char n_sign_posn;
	char p_cs_precedes;
	char p_sep_by_space;
	char p_sign_posn;
	char int_frac_digits;
	/* LC_NUMERIC */
	char *decimal_point;
	char *grouping;
	char *thousands_sep;
	char *_Frac_grouping;
	char *_Frac_sep;
	char *_False;
	char *_True;
	/* LC_MESSAGES */
	char *_No;
	char *_Yes;
};

#endif
