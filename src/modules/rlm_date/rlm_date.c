/*
 *   This program is is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License, version 2 if the
 *   License as published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/**
 * @file rlm_date.c
 * @brief Translates timestrings between formats.
 *
 * @author Artur Malinowski <artur@wow.com>
 *
 * @copyright 2013 Artur Malinowski <artur@wow.com>
 * @copyright 1999-2013 The FreeRADIUS Server Project.
 */

#include <freeradius-devel/radiusd.h>
#include <freeradius-devel/modules.h>
#include <ctype.h>
#include <time.h>

typedef struct rlm_date_t {
	char const *xlat_name;
	char const *fmt;
} rlm_date_t;

static const CONF_PARSER module_config[] = {
	{"format", PW_TYPE_STRING_PTR, offsetof(rlm_date_t, fmt), NULL, "%b %e %Y %H:%M:%S %Z"},
	{NULL, -1, 0, NULL, NULL}
};

DIAG_OFF(format-nonliteral)
static ssize_t xlat_date_convert(void *instance, UNUSED REQUEST *request, char const *fmt, char *out, size_t outlen)
{
	rlm_date_t *inst = instance;
	time_t date = 0;
	struct tm tminfo;
	VALUE_PAIR *vp;

	if ((radius_get_vp(&vp, request, fmt) < 0) || !vp) {
		*out = '\0';
		return 0;
	}

	switch (vp->da->type) {
	/*
	 *	These are 'to' types, i.e. we'll convert the integers
	 *	to a time structure, and then output it in the specified
	 *	format as a string.
	 */
	case PW_TYPE_DATE:
		date = vp->vp_date;
		goto encode;

	case PW_TYPE_INTEGER:
	case PW_TYPE_INTEGER64:
		date = (time_t) vp->vp_integer;

	encode:
		if (localtime_r(&date, &tminfo) == NULL) {
			REDEBUG("Failed converting time string to localtime");
			goto error;
		}
		return strftime(out, outlen, inst->fmt, &tminfo);

	/*
	 *	These are 'from' types, i.e. we'll convert the input string
	 *	into a time structure, and then output it as an integer
	 *	unix timestamp.
	 */
	case PW_TYPE_STRING:
		if (strptime(vp->vp_strvalue, inst->fmt, &tminfo) == NULL) {
			REDEBUG("Failed to parse time string \"%s\" as format '%s'", vp->vp_strvalue, inst->fmt);
			goto error;
		}

		date = mktime(&tminfo);
		if (date < 0) {
			REDEBUG("Failed converting parsed time into unix time");

		}
		return snprintf(out, outlen, "%" PRIu64, (uint64_t) date);

	default:
		REDEBUG("Can't convert type %s into date", fr_int2str(dict_attr_types, vp->da->type, "<INVALID>"));
	}

	error:
	*out = '\0';
	return -1;
}
DIAG_ON(format-nonliteral)

static int mod_instantiate(CONF_SECTION *conf, void *instance)
{
	rlm_date_t *inst = instance;

	inst->xlat_name = cf_section_name2(conf);
	if (!inst->xlat_name) {
		inst->xlat_name = cf_section_name1(conf);
	}

	xlat_register(inst->xlat_name, xlat_date_convert, NULL, inst);

	return 0;
}

module_t rlm_date = {
	RLM_MODULE_INIT,
	"date",				/* Name */
	0,   	/* type */
	sizeof(rlm_date_t),
	module_config,
	mod_instantiate,		/* instantiation */
	NULL,				/* detach */
	{
		NULL,			/* authentication */
		NULL,			/* authorization */
		NULL,			/* pre-accounting */
		NULL			/* accounting */
	},
};

