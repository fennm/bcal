/*
 * Byte CALculator
 *
 * Author: Arun Prakash Jana <engineerarun@gmail.com>
 * Copyright (C) 2016 by Arun Prakash Jana <engineerarun@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bcal.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ctype.h>
#include <quadmath.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "dslib.h"
#include "log.h"

#define TRUE 1
#define FALSE !TRUE

#define SECTOR_SIZE 512 /* 0x200 */
#define MAX_HEAD 16 /* 0x10 */
#define MAX_SECTOR 63 /* 0x3f */
#define UINT_BUF_LEN 40 /* log10(1 << 128) + '\0' */
#define FLOAT_BUF_LEN 128
#define FLOAT_WIDTH 40
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#ifdef __SIZEOF_INT128__
typedef __uint128_t maxuint_t;
typedef __float128 maxfloat_t;
#else
typedef __uint64_t maxuint_t;
typedef double maxfloat_t;
#endif

typedef unsigned char bool;
typedef unsigned long ulong;
typedef unsigned long long ull;

typedef struct {
	ulong c;
	ulong h;
	ulong s;
} t_chs;

static char *VERSION = "1.6";
static char *units[] = {"b", "kib", "mib", "gib", "tib", "kb", "mb", "gb", "tb"};

static char uint_buf[UINT_BUF_LEN];
static char float_buf[FLOAT_BUF_LEN];
static int unitless;

static int minimal;
int cur_loglevel = INFO;

static void binprint(maxuint_t n)
{
	int count = 127;
	char binstr[129] = {0};

	if (!n) {
		printf("0b0");
		return;
	}

	while (n && count >= 0) {
		binstr[count] = "01"[n & 1];
		--count;
		n >>= 1;
	}

	++count;

	printf("0b%s", binstr + count);
}

static char *getstr_u128(maxuint_t n, char *buf)
{
	if (n == 0) {
		buf[0] = '0';
		buf[1] = '\0';
		return buf;
	}

	memset(buf, 0, UINT_BUF_LEN);
	char *loc = buf + UINT_BUF_LEN - 1; /* start at the end */

	while (n != 0) {
		if (loc == buf)
			return NULL; /* should not happen */

		*--loc = "0123456789"[n % 10]; /* save the last digit */
		n /= 10; /* drop the last digit */
	}

	return loc;
}

static char *getstr_f128(maxfloat_t val, char *buf)
{
	int n = quadmath_snprintf(buf, FLOAT_BUF_LEN,
				  "%#*.10Qe", FLOAT_WIDTH, val);

	buf[n] = '\0';
	return buf;
}

static void printval(maxfloat_t val, char *unit)
{
	if (val - (maxuint_t)val == 0)
		printf("%40s %s\n", getstr_u128((maxuint_t)val, uint_buf), unit);
	else
		printf("%s %s\n", getstr_f128(val, float_buf), unit);
}

static void printhex_u128(maxuint_t n)
{
	ull high = (ull)(n >> (sizeof(maxuint_t) << 2));

	if (high)
		printf("0x%llx%llx", high, (ull)n);
	else
		printf("0x%llx", (ull)n);
}

#if 0
static char *strtolower(char *buf)
{
	char *p = buf;

	for (; *p; ++p)
		*p = tolower(*p);

	return buf;
}
#endif

/* This function adds check for binary input to strtoul() */
static ulong strtoul_b(char *token)
{
	int base = 0;

	/* NOTE: no NULL check here! */

	if (strlen(token) > 2 && token[0] == '0' &&
			(token[1] == 'b' || token[1] == 'B')) {
		base = 2;
	}

	return strtoul(token + base, NULL, base);
}

/* This function adds check for binary input to strtoull() */
static ull strtoull_b(char *token)
{
	int base = 0;

	/* NOTE: no NULL check here! */

	if (strlen(token) > 2 && token[0] == '0' &&
			(token[1] == 'b' || token[1] == 'B')) {
		base = 2;
	}

	return strtoull(token + base, NULL, base);
}

#if 0
/* This function adds check for binary input to strtoull() */
static maxuint_t strtoval_b(char *token)
{
	return 0;
}
#endif

static maxuint_t convertbyte(char *buf, int *ret)
{
	maxfloat_t val;
	char *pch;
	/* Convert and print in bytes */
	maxuint_t bytes = (maxuint_t)strtoflt128(buf, &pch);
	if (*pch) {
		*ret = -1;
		return 0;
	} else
		*ret = 0;

	if (minimal) {
		printf("%s\n", getstr_u128(bytes, uint_buf));
		return bytes;
	}

	printf("%40s B\n", getstr_u128(bytes, uint_buf));

	/* Convert and print in IEC standard units */

	printf("\n            IEC standard (base 2)\n\n");
	val = bytes / (maxfloat_t)1024;
	printval(val, "KiB");

	val = bytes / (maxfloat_t)(1 << 20);
	printval(val, "MiB");

	val = bytes / (maxfloat_t)(1 << 30);
	printval(val, "GiB");

	val = bytes / (maxfloat_t)((maxuint_t)1 << 40);
	printval(val, "TiB");

	/* Convert and print in SI standard values */

	printf("\n            SI standard (base 10)\n\n");
	val = bytes / (maxfloat_t)1000;
	printval(val, "kB");

	val = bytes / (maxfloat_t)1000000;
	printval(val, "MB");

	val = bytes / (maxfloat_t)1000000000;
	printval(val, "GB");

	val = bytes / (maxfloat_t)1000000000000;
	printval(val, "TB");

	return bytes;
}

static maxuint_t convertkib(char *buf, int *ret)
{
	char *pch;
	maxfloat_t val, kib = strtoflt128(buf, &pch);
	if (*pch) {
		*ret = -1;
		return 0;
	} else
		*ret = 0;

	maxuint_t bytes = (maxuint_t)(kib * 1024);

	if (minimal) {
		printf("%s\n", getstr_u128(bytes, uint_buf));
		return bytes;
	}

	printf("%40s B\n", getstr_u128(bytes, uint_buf));

	printf("\n            IEC standard (base 2)\n\n");
	printval(kib, "KiB");

	val = kib / 1024;
	printval(val, "MiB");

	val = kib / (1 << 20);
	printval(val, "GiB");

	val = kib / (1 << 30);
	printval(val, "TiB");

	printf("\n            SI standard (base 10)\n\n");
	val = kib * 1024 / 1000;
	printval(val, "kB");

	val = kib * 1024 / 1000000;
	printval(val, "MB");

	val = kib * 1024 / 1000000000;
	printval(val, "GB");

	val = kib * 1024 / 1000000000000;
	printval(val, "TB");

	return bytes;
}

static maxuint_t convertmib(char *buf, int *ret)
{
	char *pch;
	maxfloat_t val, mib = strtoflt128(buf, &pch);
	if (*pch) {
		*ret = -1;
		return 0;
	} else
		*ret = 0;

	maxuint_t bytes = (maxuint_t)(mib * (1 << 20));

	if (minimal) {
		printf("%s\n", getstr_u128(bytes, uint_buf));
		return bytes;
	}

	printf("%40s B\n", getstr_u128(bytes, uint_buf));

	printf("\n            IEC standard (base 2)\n\n");
	val = mib * 1024;
	printval(val, "KiB");

	printval(mib, "MiB");

	val = mib / 1024;
	printval(val, "GiB");

	val = mib / (1 << 20);
	printval(val, "TiB");

	printf("\n            SI standard (base 10)\n\n");
	val = mib * (1 << 20) / 1000;
	printval(val, "kB");

	val = mib * (1 << 20) / 1000000;
	printval(val, "MB");

	val = mib * (1 << 20) / 1000000000;
	printval(val, "GB");

	val = mib * (1 << 20) / 1000000000000;
	printval(val, "TB");

	return bytes;
}

static maxuint_t convertgib(char *buf, int *ret)
{
	char *pch;
	maxfloat_t val, gib = strtoflt128(buf, &pch);
	if (*pch) {
		*ret = -1;
		return 0;
	} else
		*ret = 0;

	maxuint_t bytes = (maxuint_t)(gib * (1 << 30));

	if (minimal) {
		printf("%s\n", getstr_u128(bytes, uint_buf));
		return bytes;
	}

	printf("%40s B\n", getstr_u128(bytes, uint_buf));

	printf("\n            IEC standard (base 2)\n\n");
	val = gib * (1 << 20);
	printval(val, "KiB");

	val = gib * 1024;
	printval(val, "MiB");

	printval(gib, "GiB");

	val = gib / 1024;
	printval(val, "TiB");

	printf("\n            SI standard (base 10)\n\n");
	val = gib * (1 << 30) / 1000;
	printval(val, "kB");

	val = gib * (1 << 30) / 1000000;
	printval(val, "MB");

	val = gib * (1 << 30) / 1000000000;
	printval(val, "GB");

	val = gib * (1 << 30) / 1000000000000;
	printval(val, "TB");

	return bytes;
}

static maxuint_t converttib(char *buf, int *ret)
{
	char *pch;
	maxfloat_t val, tib = strtoflt128(buf, &pch);
	if (*pch) {
		*ret = -1;
		return 0;
	} else
		*ret = 0;

	maxuint_t bytes = (maxuint_t)(tib * ((maxuint_t)1 << 40));

	if (minimal) {
		printf("%s\n", getstr_u128(bytes, uint_buf));
		return bytes;
	}

	printf("%40s B\n", getstr_u128(bytes, uint_buf));

	printf("\n            IEC standard (base 2)\n\n");
	val = tib * (1 << 30);
	printval(val, "KiB");

	val = tib * (1 << 20);
	printval(val, "MiB");

	val = tib * 1024;
	printval(val, "GiB");

	printval(tib, "TiB");

	printf("\n            SI standard (base 10)\n\n");
	val = tib * ((maxuint_t)1 << 40) / 1000;
	printval(val, "kB");

	val = tib * ((maxuint_t)1 << 40) / 1000000;
	printval(val, "MB");

	val = tib * ((maxuint_t)1 << 40) / 1000000000;
	printval(val, "GB");

	val = tib * ((maxuint_t)1 << 40) / 1000000000000;
	printval(val, "TB");

	return bytes;
}

static maxuint_t convertkb(char *buf, int *ret)
{
	char *pch;
	maxfloat_t val, kb = strtoflt128(buf, &pch);
	if (*pch) {
		*ret = -1;
		return 0;
	} else
		*ret = 0;

	maxuint_t bytes = (maxuint_t)(kb * 1000);

	if (minimal) {
		printf("%s\n", getstr_u128(bytes, uint_buf));
		return bytes;
	}

	printf("%40s B\n", getstr_u128(bytes, uint_buf));

	printf("\n            IEC standard (base 2)\n\n");
	val = kb * 1000 / 1024;
	printval(val, "KiB");

	val = kb * 1000 / (1 << 20);
	printval(val, "MiB");

	val = kb * 1000 / (1 << 30);
	printval(val, "GiB");

	val = kb * 1000 / ((maxuint_t)1 << 40);
	printval(val, "TiB");

	printf("\n            SI standard (base 10)\n\n");
	printval(kb, "kB");

	val = kb / 1000;
	printval(val, "MB");

	val = kb / 1000000;
	printval(val, "GB");

	val = kb / 1000000000;
	printval(val, "TB");

	return bytes;
}

static maxuint_t convertmb(char *buf, int *ret)
{
	char *pch;
	maxfloat_t val, mb = strtoflt128(buf, &pch);
	if (*pch) {
		*ret = -1;
		return 0;
	} else
		*ret = 0;

	maxuint_t bytes = (maxuint_t)(mb * 1000000);

	if (minimal) {
		printf("%s\n", getstr_u128(bytes, uint_buf));
		return bytes;
	}

	printf("%40s B\n", getstr_u128(bytes, uint_buf));

	printf("\n            IEC standard (base 2)\n\n");
	val = mb * 1000000 / 1024;
	printval(val, "KiB");

	val = mb * 1000000 / (1 << 20);
	printval(val, "MiB");

	val = mb * 1000000 / (1 << 30);
	printval(val, "GiB");

	val = mb * 1000000 / ((maxuint_t)1 << 40);
	printval(val, "TiB");

	printf("\n            SI standard (base 10)\n\n");
	val = mb * 1000;
	printval(val, "kB");

	printval(mb, "MB");

	val = mb / 1000;
	printval(val, "GB");

	val = mb / 1000000;
	printval(val, "TB");

	return bytes;
}

static maxuint_t convertgb(char *buf, int *ret)
{
	char *pch;
	maxfloat_t val, gb = strtoflt128(buf, &pch);
	if (*pch) {
		*ret = -1;
		return 0;
	} else
		*ret = 0;

	maxuint_t bytes = (maxuint_t)(gb * 1000000000);

	if (minimal) {
		printf("%s\n", getstr_u128(bytes, uint_buf));
		return bytes;
	}

	printf("%40s B\n", getstr_u128(bytes, uint_buf));

	printf("\n            IEC standard (base 2)\n\n");
	val = gb * 1000000000 / 1024;
	printval(val, "KiB");

	val = gb * 1000000000 / (1 << 20);
	printval(val, "MiB");

	val = gb * 1000000000 / (1 << 30);
	printval(val, "GiB");

	val = gb * 1000000000 / ((maxuint_t)1 << 40);
	printval(val, "TiB");

	printf("\n            SI standard (base 10)\n\n");
	val = gb * 1000000;
	printval(val, "kB");

	val = gb * 1000;
	printval(val, "MB");

	printval(gb, "GB");

	val = gb / 1000;
	printval(val, "TB");

	return bytes;
}

static maxuint_t converttb(char *buf, int *ret)
{
	char *pch;
	maxfloat_t val, tb = strtoflt128(buf, &pch);
	if (*pch) {
		*ret = -1;
		return 0;
	} else
		*ret = 0;

	maxuint_t bytes = (__uint128_t)(tb * 1000000000000);

	if (minimal) {
		printf("%s\n", getstr_u128(bytes, uint_buf));
		return bytes;
	}

	printf("%40s B\n", getstr_u128(bytes, uint_buf));

	printf("\n            IEC standard (base 2)\n\n");
	val = tb * 1000000000000 / 1024;
	printval(val, "KiB");

	val = tb * 1000000000000 / (1 << 20);
	printval(val, "MiB");

	val = tb * 1000000000000 / (1 << 30);
	printval(val, "GiB");

	val = tb * 1000000000000 / ((maxuint_t)1 << 40);
	printval(val, "TiB");

	printf("\n            SI standard (base 10)\n\n");
	val = tb * 1000000000;
	printval(val, "kB");

	val = tb * 1000000;
	printval(val, "MB");

	val = tb * 1000;
	printval(val, "GB");

	printval(tb, "TB");

	return bytes;
}

static bool chs2lba(char *chs, maxuint_t *lba)
{
	int token_no = 0;
	char *ptr, *token;
	ulong param[5] = {0, 0, 0, MAX_HEAD, MAX_SECTOR};

	ptr = token = chs;

	while (*ptr && token_no < 5) {
		if (*ptr == '-') {
			/* Replace '-' with NULL and get the token */
			*ptr = '\0';
			param[token_no] = strtoul_b(token);
			++token_no;
			/* Restore the '-' */
			*ptr = '-';
			++ptr;
			/* Point to start of next token */
			token = ptr;

			if (*ptr == '\0' && token_no < 5) {
				param[token_no] = strtoul_b(token);
				++token_no;
			}

			continue;
		}

		++ptr;

		if (*ptr == '\0' && token_no < 5) {
			param[token_no] = strtoul_b(token);
			++token_no;
		}
	}

	/* Fail if CHS is omitted */
	if (token_no < 3) {
		log(ERROR, "CHS missing\n");
		return FALSE;
	}

	if (!param[3]) {
		log(ERROR, "MAX_HEAD = 0\n");
		return FALSE;
	}

	if (!param[4]) {
		log(ERROR, "MAX_SECTOR = 0\n");
		return FALSE;
	}

	if (!param[2]) {
		log(ERROR, "S = 0\n");
		return FALSE;
	}

	if (param[1] > param[3]) {
		log(ERROR, "H > MAX_HEAD\n");
		return FALSE;
	}

	if (param[2] > param[4]) {
		log(ERROR, "S > MAX_SECTOR\n");
		return FALSE;
	}


	*lba = param[3] * param[4] * param[0]; /* MH * MS * C */
	*lba += param[4] * param[1]; /* MS * H */

	*lba += param[2] - 1; /* S - 1 */

	printf("\033[1mCHS2LBA\033[0m\n");
	printf("  C:%lu  H:%lu  S:%lu  MAX_HEAD:%lu  MAX_SECTOR:%lu\n",
		param[0], param[1], param[2], param[3], param[4]);

	return TRUE;
}

static bool lba2chs(char *lba, t_chs *p_chs)
{
	int token_no = 0;
	char *ptr, *token;
	maxuint_t param[3] = {0, MAX_HEAD, MAX_SECTOR};

	ptr = token = lba;

	while (*ptr && token_no < 3) {
		if (*ptr == '-') {
			*ptr = '\0';
			param[token_no] = strtoull_b(token);
			++token_no;
			*ptr = '-';
			++ptr;
			token = ptr;

			if (*ptr == '\0' && token_no < 3) {
				param[token_no] = strtoull_b(token);
				++token_no;
			}

			continue;
		}

		++ptr;

		if (*ptr == '\0' && token_no < 3) {
			param[token_no] = strtoull_b(token);
			++token_no;
		}
	}

	/* Fail if LBA is omitted */
	if (!token_no) {
		log(ERROR, "LBA missing\n");
		return FALSE;
	}

	if (!param[1]) {
		log(ERROR, "MAX_HEAD = 0\n");
		return FALSE;
	}

	if (!param[2]) {
		log(ERROR, "MAX_SECTOR = 0\n");
		return FALSE;
	}

	/* L / (MS * MH) */
	p_chs->c = (ulong)(param[0] / (param[2] * param[1]));
	/* (L / MS) % MH */
	p_chs->h = (ulong)((param[0] / param[2]) % param[1]);
	if (p_chs->h > MAX_HEAD) {
		log(ERROR, "H > MAX_HEAD\n");
		return FALSE;
	}

	/* (L % MS) + 1 */
	p_chs->s = (ulong)((param[0] % param[2]) + 1);
	if (p_chs->s > MAX_SECTOR) {
		log(ERROR, "S > MAX_SECTOR\n");
		return FALSE;
	}

	printf("\033[1mLBA2CHS\033[0m\n  LBA:%s  ",
		getstr_u128(param[0], uint_buf));
	printf("MAX_HEAD:%s  ", getstr_u128(param[1], uint_buf));
	printf("MAX_SECTOR:%s\n", getstr_u128(param[2], uint_buf));

	return TRUE;
}

static void usage(void)
{
	printf("usage: bcal [-c N] [-f FORMAT] [-s bytes] [-d] [-h]\n\
	    [expression] [N unit] \n\n\
Perform storage conversions and calculations.\n\n\
positional arguments:\n\
  expression       evaluate storage arithmetic expression\n\
		   +, -, *, / with decimal inputs supported\n\
		   unit can be multiplied or divided by +ve integers\n\
		   units can be added or subtracted from each other\n\
		   Examples:\n\
		       bcal \"(5kb+2mb)/3\"\n\
		       bcal \"5 tb / 12\"\n\
		       bcal \"2.5mb*3\"\n\
  N [unit]         capacity in B/KiB/MiB/GiB/TiB/kB/MB/GB/TB\n\
		   see https://wiki.ubuntu.com/UnitsPolicy\n\
		   default unit is B (byte), case is ignored\n\
		   N can be decimal or '0x' prefixed hex value\n\n\
optional arguments:\n\
  -c N             show N in binary, decimal and hex\n\
  -f FORMAT        convert CHS to LBA or LBA to CHS\n\
		   formats are hyphen-separated\n\
		   LBA format:\n\
		       starts with 'l':\n\
		       lLBA-MAX_HEAD-MAX_SECTOR\n\
		   CHS format:\n\
		       starts with 'c':\n\
		       cC-H-S-MAX_HEAD-MAX_SECTOR\n\
		   omitted values are considered 0\n\
		   FORMAT 'c-50--0x12-' denotes:\n\
		     C = 0, H = 50, S = 0, MH = 0x12, MS = 0\n\
		   FORMAT 'l50-0x12' denotes:\n\
		     LBA = 50, MH = 0x12, MS = 0\n\
		   default MAX_HEAD: 16, default MAX_SECTOR: 63\n\
  -s bytes         sector size [default 512]\n\
  -m               show minimal output (e.g. decimal bytes)\n\
  -d               enable debug information and logs\n\
  -h               show this help and exit\n\n\
Version %s\n\
Copyright © 2016-2017 Arun Prakash Jana <engineerarun@gmail.com>\n\
License: GPLv3\n\
Webpage: https://github.com/jarun/bcal\n", VERSION);
}

static int xstricmp(const char *s1, const char *s2)
{
	while (*s1 && (tolower(*s1) == tolower(*s2))) {
		++s1;
		++s2;
	}
	return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

/* Convert any unit in bytes
 * Failure if out parameter holds -1
 */
static maxuint_t unitconv(Data bunit, char *isunit, int *out)
{
	/* Data is a C structure containing a string p and a char
	 * indicating if the string is a unit or a plain number
	 */
	char *numstr = bunit.p, *punit;
	int len, count;
	maxfloat_t byte_metric = 0;

	if (numstr == NULL || *numstr == '\0') {
		log(ERROR, "invalid token\n");
		*out = -1;
		return 0;
	} else
		*out = 0;

	len = strlen(numstr) - 1;
	if (isdigit(numstr[len])) {
		/* ensure this is not the result of a previous operation */
		if (*isunit != 1)
			*isunit = 0;
		return strtoull(numstr, NULL, 0);
	}

	while (isalpha(numstr[len]))
		--len;

	punit = numstr + len + 1;

	count = ARRAY_SIZE(units);
	while (--count >= 0)
		if (!xstricmp(units[count], punit))
			break;

	if (count == -1) {
		log(ERROR, "unknown unit\n");
		*out = -1;
		return 0;
	}

	*isunit = 1;
	*punit = '\0';

	switch (count) {
	case 0: {
		maxuint_t bytes;
		bytes = (maxuint_t)strtoflt128(numstr, &punit);
		if (*punit)
			goto error;
		return bytes;
		}
	case 1: /* Kibibyte */
		byte_metric = strtoflt128(numstr, &punit);
		if (*punit)
			goto error;
		return (maxuint_t)(byte_metric * 1024);
	case 2: /* Mebibyte */
		byte_metric = strtoflt128(numstr, &punit);
		if (*punit)
			goto error;
		return (maxuint_t)(byte_metric * (1 << 20));
	case 3: /* Gibibyte */
		byte_metric = strtoflt128(numstr, &punit);
		if (*punit)
			goto error;
		return (maxuint_t)(byte_metric * (1 << 30));
	case 4: /* Tebibyte */
		byte_metric = strtoflt128(numstr, &punit);
		if (*punit)
			goto error;
		return (maxuint_t)(byte_metric * ((maxuint_t)1 << 40));
	case 5: /* Kilobyte */
		byte_metric = strtoflt128(numstr, &punit);
		if (*punit)
			goto error;
		return (maxuint_t)(byte_metric * 1000);
	case 6: /* Megabyte */
		byte_metric = strtoflt128(numstr, &punit);
		if (*punit)
			goto error;
		return (maxuint_t)(byte_metric * 1000000);
	case 7: /* Gigabyte */
		byte_metric = strtoflt128(numstr, &punit);
		if (*punit)
			goto error;
		return (maxuint_t)(byte_metric * 1000000000);
	case 8: /* Terabyte */
		byte_metric = strtoflt128(numstr, &punit);
		if (*punit)
			goto error;
		return (maxuint_t)(byte_metric * 1000000000000);
	default:
		log(ERROR, "unknown unit\n");
		*out = -1;
		return 0;
	}

error:
	log(ERROR, "malformed input\n");
	*out = -1;
	return 0;
}

static int priority(char sign) /* Get the priority of operators */
{
	switch (sign) {
	case '-': return 1;
	case '+': return 2;
	case '*': return 3;
	case '/': return 4;
	}

	return 0;
}

/* Convert Infix mathematical expression to Postfix */
static int infix2postfix(char *exp, queue **resf, queue **resr)
{
	stack *op = NULL;  /* Operator Stack */
	char *token = strtok(exp, " ");
	Data tokenData = {"\0", 0};
	int balanced = 0;
	Data ct;

	while (token) {
		/* Copy argument to string part of the structure */
		strcpy(tokenData.p, token);

		switch (token[0]) {
		case '+':
		case '-':
		case '*':
		case '/':
			if (token[1] != '\0') {
				log(ERROR, "invalid token terminator\n");
				emptystack(&op);
				cleanqueue(resf);
				return -1;
			}

			while (!isempty(op) && top(op)[0] != '(' &&
			       priority(token[0]) <= priority(top(op)[0])) {
				/* Pop from operator stack */
				pop(&op, &ct);
				/* Insert to Queue */
				enqueue(resf, resr, ct);
			}

			push(&op, tokenData);
			break;
		case '(':
			++balanced;
			push(&op, tokenData);
			break;
		case ')':
			while (!isempty(op) && top(op)[0] != '(') {
				pop(&op, &ct);
				enqueue(resf, resr, ct);
			}

			pop(&op, &ct);
			--balanced;
			break;
		default:
			/* Enqueue operands */
			enqueue(resf, resr, tokenData);
		}

		token = strtok(NULL, " ");
	}

	while (!isempty(op)) {
		/* Put remaining elements into the queue */
		pop(&op, &ct);
		enqueue(resf, resr, ct);
	}

	if (balanced != 0) {
		log(ERROR, "unbalanced expression\n");
		cleanqueue(resf);
		return -1;
	}

	return 0;
}

/* Evaluates Postfix Expression
 * Failure if out parameter holds -1
 */
static maxuint_t eval(queue **front, queue **rear, int *out)
{
	stack *est = NULL;
	Data res, arg, raw_a, raw_b, raw_c;
	*out = 0;
	maxuint_t a, b, c;

	/* Check if queue is empty */
	if (*front == NULL)
		return 0;

	/* Check if only one element in the queue */
	if (*front == *rear) {
		char unit = 0;

		dequeue(front, rear, &res);
		return unitconv(res, &unit, out);
	}

	while (*front) {
		dequeue(front, rear, &arg);

		/* Check if arg is an operator */
		if (strlen(arg.p) == 1 && !isdigit(arg.p[0])) {
			pop(&est, &raw_b);
			pop(&est, &raw_a);

			b = unitconv(raw_b, &raw_b.unit, out);
			if (*out == -1)
				return 0;
			a = unitconv(raw_a, &raw_a.unit, out);
			if (*out == -1)
				return 0;

			log(DEBUG, "(%s, %d) %c (%s, %d)\n",
			    raw_a.p, raw_a.unit, arg.p[0], raw_b.p, raw_b.unit);

			c = 0;
			raw_c.unit = 0;

			switch (arg.p[0]) {
			case '+':
				/* Check if both are units */
				if (raw_a.unit == raw_b.unit) {
					c = a + b;
					if (raw_a.unit)
						raw_c.unit = 1;
					break;
				}

				log(ERROR, "unit mismatch in +\n");
				goto error;
			case '-':
				/* Check if both are units */
				if (raw_a.unit == raw_b.unit) {
					if (b > a) {
						log(ERROR, "negative result\n");
						goto error;
					}

					c = a - b;
					if (raw_a.unit)
						raw_c.unit = 1;
					break;
				}

				log(ERROR, "unit mismatch in -\n");
				goto error;
			case '*':
				/* Check if only one is unit */
				if (!(raw_a.unit && raw_b.unit)) {
					c = a * b;
					if (raw_a.unit || raw_b.unit)
						raw_c.unit = 1;
					break;
				}

				log(ERROR, "unit mismatch in *\n");
				goto error;
			case '/':
				/* Divisor must be a non-unit */
				if (!raw_b.unit) {
					c = a / b;
					if (raw_a.unit)
						raw_c.unit = 1;
					break;
				}

				log(ERROR, "unit mismatch in /\n");
				goto error;
			}

			/* Convert to string */
			strcpy(raw_c.p, getstr_u128(c, uint_buf));
			/* Push to stack */
			push(&est, raw_c);

		} else {
			log(DEBUG, "in else (%s)\n", arg.p);
			push(&est, arg);
		}
	}

	pop(&est, &res);

	/* Stack must be empty at this point */
	if (!isempty(est)) {
		log(ERROR, "invalid expression\n");
		goto error;
	}

	/* Convert string to integer */
	return strtoull(res.p, NULL, 0);

error:
	*out = -1;
	emptystack(&est);
	cleanqueue(front);
	return 0;
}

static int issign(char c)
{
	if (c == '+' || c == '-' || c == '*' || c == '/')
		return 1;

	return 0;
}

/* Check if a char is operator or not */
static int isoperator(char c)
{
	switch (c) {
	case '+':
	case '-':
	case '*':
	case '/':
	case '(':
	case ')': return 1;
	default: return 0;
	}
}

/* Check if valid storage arithmetic expression */
static int checkexp(char *exp)
{
	while (*exp) {
		if (*exp == 'b' || *exp == 'B')
			return 1;

		++exp;
	}

	return 0;
}

/* Trim all whitespace from both ends, in place */
static void strstrip(char *s)
{
	if (!s || !*s)
		return;

	int len = strlen(s) - 1;

	while (len >= 0 && isspace(s[len]))
		--len;
	s[len + 1] = '\0';

	len = 0;
	while (s[len] && isspace(s[len]))
		++len;

	if (len) {
		while (s[len]) {
			*s = s[len];
			++s;
		}

		*s = '\0';
	}
}

/* Replace consecutive inner whitespaces with a single space */
static void removeinnerspaces(char *s)
{
	char *p = s;

	while (*s != '\0') {
		if (!isspace(*s)) {
			*p = *s;
			++p;
		}

		++s;
	}

	*p = '\0';
}

/* Make the expression compatible with parsing by
 * inserting/removing space between arguments
 */
static char *fixexpr(char *exp)
{
	strstrip(exp);
	removeinnerspaces(exp);

	if (!checkexp(exp)) {
		unitless = 1;
		return NULL;
	}

	int i = 0, j = 0;
	char *parsed = (char *)malloc(2 * strlen(exp) * sizeof(char));
	char prev = '(';

	log(DEBUG, "exp (%s)\n", exp);

	while (exp[i] != '\0') {
		if (exp[i] == '-' && (issign(prev) || prev == '(')) {
			log(ERROR, "negative token\n");
			free(parsed);
			return NULL;
		}

		if (isoperator(exp[i]) && isalpha(exp[i + 1])) {
			log(ERROR, "invalid expression\n");
			free(parsed);
			return NULL;
		}

		if ((isdigit(exp[i]) && isoperator(exp[i + 1])) ||
		    (isoperator(exp[i]) && (isdigit(exp[i + 1]) ||
		     isoperator(exp[i + 1]))) ||
		    (isalpha(exp[i]) && isoperator(exp[i + 1]))) {
			parsed[j] = exp[i];
			++j;
			parsed[j] = ' ';
			++j;
			parsed[j] = exp[i + 1];
		} else {
			parsed[j] = exp[i];
			++j;
		}

		prev = exp[i];
		++i;
	}

	if (parsed[j])
		parsed[++j] = '\0';

	log(DEBUG, "parsed (%s)\n", parsed);

	/* If there's no space, this is either
	 * a number or malformed expression
	 */
	i = 0;
	while (parsed[i] && parsed[i] != ' ')
		++i;

	if (!parsed[i]) {
		log(DEBUG, "noop expression\n");
		free(parsed);
		unitless = 1;
		return NULL;
	}

	return parsed;
}

static int convertunit(char *value, char *unit, ulong sectorsz)
{
	int count = ARRAY_SIZE(units), ret;
	maxuint_t bytes = 0, lba = 0, offset = 0;

	strstrip(value);
	if (value[0] == '\0') {
		log(ERROR, "invalid value\n");
		return -1;
	}

	if (!unit) {
		int unitchars = 0, len = strlen(value);

		while (len) {
			if (!isalpha(value[len - 1]))
				break;

			++unitchars;
			--len;
		}

		if (unitchars) {
			while (--count >= 0)
				if (!xstricmp(units[count], value + len))
					break;

			if (count == -1) {
				log(ERROR, "unknown unit\n");
				return -1;
			}

			value[len] = '\0';
		} else
			count = 0;
	} else {
		strstrip(unit);

		while (--count >= 0)
			if (!xstricmp(units[count], unit))
				break;

		if (count == -1) {
			log(ERROR, "unknown unit\n");
			return -1;
		}
	}

	log(DEBUG, "%s %s\n", value, units[count]);

	if (!minimal)
		printf("\033[1mUNIT CONVERSION\033[0m\n");

	switch (count) {
	case 0:
		bytes = convertbyte(value, &ret);
		break;
	case 1:
		bytes = convertkib(value, &ret);
		break;
	case 2:
		bytes = convertmib(value, &ret);
		break;
	case 3:
		bytes = convertgib(value, &ret);
		break;
	case 4:
		bytes = converttib(value, &ret);
		break;
	case 5:
		bytes = convertkb(value, &ret);
		break;
	case 6:
		bytes = convertmb(value, &ret);
		break;
	case 7:
		bytes = convertgb(value, &ret);
		break;
	case 8:
		bytes = converttb(value, &ret);
		break;
	default:
		log(ERROR, "unknown unit\n");
		return -1;
	}

	if (ret == -1) {
		log(ERROR, "malformed input\n");
		return -1;
	}

	if (minimal)
		return 0;

	printf("\nADDRESS\n (d) %s\n (h) ",
		getstr_u128(bytes, uint_buf));
	printhex_u128(bytes);

	/* Calculate LBA and offset */
	lba = bytes / sectorsz;
	offset = bytes % sectorsz;

	printf("\n\nLBA:OFFSET (sector size: 0x%lx)\n", sectorsz);
	/* We use a global buffer, so print decimal lba first, then offset */
	printf(" (d) %s:", getstr_u128(lba, uint_buf));
	printf("%s\n (h) ", getstr_u128(offset, uint_buf));
	printhex_u128(lba);
	printf(":");
	printhex_u128(offset);
	printf("\n");

	return 0;
}

static int evaluate(char *exp)
{
	int ret = 0;
	maxuint_t bytes = 0;
	queue *front = NULL, *rear = NULL;
	char *expr = fixexpr(exp);	 /* Make parsing compatible */

	if (expr == NULL)
		return -1;

	ret = infix2postfix(expr, &front, &rear);
	free(expr);
	if (ret == -1)
		return -1;

	bytes = eval(&front, &rear, &ret);  /* Evaluate Expression */
	if (ret == -1)
		return -1;

	if (minimal) {
		convertbyte(getstr_u128(bytes, uint_buf), &ret);
		if (ret == -1) {
			log(ERROR, "malformed input\n");
			return -1;
		}

		return 0;
	}

	printf("\033[1mRESULT\033[0m\n");

	convertbyte(getstr_u128(bytes, uint_buf), &ret);
	if (ret == -1) {
		log(ERROR, "malformed input\n");
		return -1;
	}

	printf("\nADDRESS\n (d) %s\n (h) ",
		getstr_u128(bytes, uint_buf));
	printhex_u128(bytes);
	printf("\n");

	return 0;
}

int main(int argc, char **argv)
{
	int opt = 0;
	ulong sectorsz = SECTOR_SIZE;

	opterr = 0;

	while ((opt = getopt(argc, argv, "c:df:hms:")) != -1) {
		switch (opt) {
		case 'c':
			if (*optarg == '-') {
				log(ERROR, "N must be >= 0\n");
				return -1;
			}

			printf("\033[1mBASE CONVERSION\033[0m\n");
			maxuint_t val = strtoull_b(optarg);

			printf(" (b) ");
			binprint(val);
			printf("\n (d) %s\n (h) ",
				getstr_u128(val, uint_buf));
			printhex_u128(val);
			printf("\n\n");
			break;
		case 'f':
			if (tolower(*optarg) == 'c') {
				maxuint_t lba = 0;

				if (chs2lba(optarg + 1, &lba)) {
					printf("  LBA: (d) %s, (h) ",
						getstr_u128(lba, uint_buf));
					printhex_u128(lba);
					printf("\n\n");
				}
			} else if (tolower(*optarg) == 'l') {
				t_chs chs;

				if (lba2chs(optarg + 1, &chs)) {
					printf("  CHS: (d) %lu %lu %lu, ",
						chs.c, chs.h, chs.s);
					printf("(h) 0x%lx 0x%lx 0x%lx\n\n",
						chs.c, chs.h, chs.s);
				}
			} else
				log(ERROR, "invalid input\n");
			break;
		case 'm':
			minimal = 1;
			break;
		case 's':
			if (*optarg == '-') {
				log(ERROR, "sector size must be +ve\n");
				return -1;
			}
			sectorsz = strtoul_b(optarg);
			break;
		case 'd':
			cur_loglevel = DEBUG;
			log(DEBUG, "bcal v%s\n", VERSION);
			break;
		case 'h':
			usage();
			return 0;
		default:
			usage();
			return -1;
		}
	}

	if (argc == 1 && optind == 1) {
		usage();
		return -1;
	}

	/* Conversion */
	if (argc - optind == 2)
		if (convertunit(argv[optind], argv[optind + 1], sectorsz) == -1)
			return -1;

	/*Arithmetic operation*/
	if (argc - optind == 1)
		if ((evaluate(argv[optind]) == -1) && !unitless)
			return -1;

	/* Possibly a unitless byte */
	if (unitless)
		if (convertunit(argv[optind], NULL, sectorsz) == -1)
			return -1;

	return 0;
}
