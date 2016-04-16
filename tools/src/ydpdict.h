/*
 *  ydpdict support library
 *  (C) Copyright 1998-2010 Wojtek Kaniewski <wojtekka@toxygen.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,
 *  USA.
 *
 *  $Id: ydpdict.h 52 2007-12-16 09:49:42Z wojtekka $
 */

#ifndef _YDPDICT_YDPDICT_H
#define _YDPDICT_YDPDICT_H

/**
 * Macro creating version identifier. Useful for comparison of current
 * library version version expected.
 */
#define YDPDICT_MAKE_VERSION(major,minor,release) ((major) << 16 | (minor) << 8 | (release))

/**
 * Library version.
 */
#define YDPDICT_VERSION YDPDICT_MAKE_VERSION(1,0,4)

/**
 * Output encoding type.
 */
typedef enum {
	YDPDICT_ENCODING_WINDOWS1250,
	YDPDICT_ENCODING_UTF8
} ydpdict_encoding_t;

/**
 * Opaque dictionary structure.
 */
typedef void ydpdict_t;

ydpdict_t *ydpdict_open(const char *dat, const char *idx, ydpdict_encoding_t encoding);
int ydpdict_get_count(const ydpdict_t *dict);
int ydpdict_find_word(const ydpdict_t *dict, const char *word);
char *ydpdict_get_word(const ydpdict_t *dict, int def);
char *ydpdict_read_rtf(const ydpdict_t *dict, int def);
char *ydpdict_read_xhtml(const ydpdict_t *dict, int def);
int ydpdict_set_xhtml_header(ydpdict_t *dict, int header);
int ydpdict_set_xhtml_style(ydpdict_t *dict, const char *style);
int ydpdict_set_xhtml_use_style(ydpdict_t *dict, int use_style);
int ydpdict_set_xhtml_title(ydpdict_t *dict, const char *title);
int ydpdict_close(ydpdict_t *dict);

char *ydpdict_phonetic_to_utf8(const char *input);
char *ydpdict_windows1250_to_utf8(const char *input);
char *ydpdict_windows1250_super_to_utf8(const char *input);

#endif /* _YDPDICT_YDPDICT_H */

