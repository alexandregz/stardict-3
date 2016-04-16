/*
 *  ydpdict support library
 *  (C) Copyright 1998-2012 Wojtek Kaniewski <wojtekka@toxygen.net>
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
 *  $Id$
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <ctype.h>
#include "ydpdict.h"

/**
 * Index file magic.
 */
#define YDPDICT_IDX_MAGIC 0x8d4e11d5

/**
 * Word array entry.
 */
typedef struct {
	char *word;			/** Word in output encoding */
	uint32_t index;			/** Word definition index in database */
} ydpdict_word_t;

/**
 * Dictionary structure.
 */
typedef struct {
	FILE *dat;			/** Database file handle */
	FILE *idx;			/** Index file handle */

	int count;			/** Word count */
	ydpdict_word_t *words;		/** Word array */

	ydpdict_encoding_t encoding;	/** Output encoding */
 
	int xhtml_header;		/** XHTML header output flag */
	char *xhtml_title;		/** XHTML title */
	char *xhtml_style;		/** XHTML custom style */
	int xhtml_use_style;		/** XHTML style usage flag */
} ydpdict_priv_t;

/*
 * Character attributes.
 */
typedef enum {
	ATTR_B = (1 << 0),		/** Bold */
	ATTR_CF0 = (1 << 1),		/** Color #0 */
	ATTR_CF1 = (1 << 2),		/** Color #1 */
	ATTR_CF2 = (1 << 3),		/** Color #2 */
	ATTR_QC = (1 << 4),		/** Comment */
	ATTR_SUPER = (1 << 5),		/** Superscript */
	ATTR_F = (1 << 6),		/** Font */
	ATTR_F1 = (1 << 7),		/** Phonetic font */
	ATTR_I = (1 << 8),		/** Italic */
	ATTR_CF5 = (1 << 9),		/** Color #5 */
	ATTR_SA = (1 << 10),		/** Margin? */
	ATTR_B0 = (1 << 11),		/** No bold */
	ATTR_I0 = (1 << 12),		/** No italic */
} ydpdict_attr_t;

/**
 * \brief Conversion table from phonetic characters to UTF-8.
 * \note It covers characters in range 128..159.
 */
static char *ydpdict_phonetic_to_utf8_table[32] =
{
	"?", "?", "ɔ", "ʒ", "?", "ʃ", "ɛ", "ʌ",
	"ə", "θ", "ɪ", "ɑ", "?", "ː", "ˈ", "?",
	"ŋ", "?", "?", "?", "?", "?", "?", "ð",
	"æ", "?", "?", "?", "?", "?", "?", "?"
};

/**
 * \brief Table of superscript digits in UTF-8.
 * \note It covers characters in range 0..9.
 */
static char *ydpdict_superscript_to_utf8_table[10] =
{
	"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"
};

/**
 * \brief Conversion table from windows-1250 to UTF-8.
 * \note It covers characters in range 128..255.
 */
static char *ydpdict_windows1250_to_utf8_table[128] =
{
	"€", "?", "‚", "?", "„", "…", "†", "‡", 
	"?", "‰", "Š", "‹", "Ś", "Ť", "Ž", "Ź", 
	"?", "‘", "’", "“", "”", "•", "–", "—", 
	"?", "™", "š", "›", "ś", "ť", "ž", "ź", 
	" ", "ˇ", "˘", "Ł", "¤", "Ą", "¦", "§", 
	"¨", "©", "Ş", "«", "¬", "­", "®", "Ż", 
	"°", "±", "˛", "ł", "´", "µ", "¶", "·", 
	"¸", "ą", "ş", "»", "Ľ", "˝", "ľ", "ż", 
	"Ŕ", "Á", "Â", "Ă", "Ä", "Ĺ", "Ć", "Ç", 
	"Č", "É", "Ę", "Ë", "Ě", "Í", "Î", "Ď", 
	"Đ", "Ń", "Ň", "Ó", "Ô", "Ő", "Ö", "×", 
	"Ř", "Ů", "Ú", "Ű", "Ü", "Ý", "Ţ", "ß", 
	"à", "á", "â", "ă", "ä", "ĺ", "ć", "ç", 
	"č", "é", "ę", "ë", "ě", "í", "î", "ï", 
	"đ", "ń", "ň", "ó", "ô", "ő", "ö", "÷", 
	"ř", "ů", "ú", "ű", "ü", "ý", "ţ", "˙", 
};

/**
 * \brief Convert 32-bit value from little-endian to machine-endian
 *
 * \param value little-endian value
 * 
 * \return machine-endian value
 */
static inline uint32_t ydpdict_fix32(uint32_t value)
{
#ifndef WORDS_BIGENDIAN
	return value;
#else
	return (uint32_t) (((value & (uint32_t) 0x000000ffU) << 24) |
		((value & (uint32_t) 0x0000ff00U) << 8) |
		((value & (uint32_t) 0x00ff0000U) >> 8) |
		((value & (uint32_t) 0xff000000U) >> 24));
#endif
}

/**
 * \brief Convert 16-bit value from little-endian to machine-endian
 *
 * \param value little-endian value
 * 
 * \return machine-endian value
 */
static inline uint16_t ydpdict_fix16(uint16_t value)
{
#ifndef WORDS_BIGENDIAN
	return value;
#else
	return (uint16_t) (((value & (uint16_t) 0x00ffU) << 8) |
		((value & (uint16_t) 0xff00U) >> 8));
#endif
}

/**
 * \brief Open dictionary and read index
 * 
 * The common mistake is to supply lowercase names, while the files have
 * uppercase names.
 * 
 * \param dat data file path
 * \param idx index file path
 * \param encoding output encoding for XHTML
 * 
 * \return Pointer to allocated structure or NULL on error
 */
ydpdict_t *ydpdict_open(const char *dat, const char *idx, ydpdict_encoding_t encoding)
{
	ydpdict_priv_t *dict = NULL;
	uint32_t index;
	uint32_t magic;
	uint16_t count;
	int i, j;

	dict = calloc(1, sizeof(ydpdict_priv_t));

	if (dict == NULL)
		goto failure;

	/* Set defaults */

	dict->xhtml_header = 0;
	dict->encoding = encoding;

	/* Open files */

	dict->idx = fopen(idx, "rb");

	if (dict->idx == NULL)
		goto failure;

	dict->dat = fopen(dat, "rb");

	if (dict->dat == NULL)
		goto failure;

	/* Read and verify magic cookie */

	if (fread(&magic, sizeof(magic), 1, dict->idx) != 1)
		goto failure;

	if (ydpdict_fix32(magic) != YDPDICT_IDX_MAGIC)
		goto failure;

	/* Read word count */

	if (fseek(dict->idx, 8, SEEK_SET) == (off_t) -1)
		goto failure;

	if (fread(&count, sizeof(count), 1, dict->idx) != 1)
		goto failure;

	dict->count = ydpdict_fix16(count);

	/* Allocate memory */

	dict->words = calloc(dict->count, sizeof(ydpdict_word_t));

	if (dict->words == NULL)
		goto failure;

	/* Read index table offset */

	if (fseek(dict->idx, 16, SEEK_SET) == (off_t) -1)
		goto failure;

	index = 0;

	if (fread(&index, sizeof(index), 1, dict->idx) != 1)
		goto failure;

	index = ydpdict_fix32(index);

	if (fseek(dict->idx, index, SEEK_SET) == (off_t) -1)
		goto failure;

	/* Read index table */

	i = 0;

	do {
		char buf[256];

		if (fseek(dict->idx, 4, SEEK_CUR) == (off_t) -1)
			goto failure;

		if (fread(&index, sizeof(index), 1, dict->idx) != 1)
			goto failure;

		dict->words[i].index = ydpdict_fix32(index);

		j = 0;

		do {
			unsigned char ch;

			if (fread(&ch, 1, 1, dict->idx) != 1)
				goto failure;

			if (dict->encoding == YDPDICT_ENCODING_WINDOWS1250)
				buf[j] = ch;
			else {
				if (ch > 127) {
					const char *str;
					int k;

					str = ydpdict_windows1250_to_utf8_table[ch - 128];

					for (k = 0; str[k] && j < sizeof(buf); k++)
						buf[j++] = str[k];

					j--;
				} else
					buf[j] = ch;
			}
		} while (j < sizeof(buf) && buf[j++]);

		dict->words[i].word = strdup(buf);

		if (dict->words[i].word == NULL)
			goto failure;

	} while (++i < dict->count);

	return dict;

failure:
	if (dict != NULL)
		ydpdict_close(dict);

	return NULL;
}

/**
 * \brief Read word definition in original RTF format, without charset 
 * convertion
 *
 * \param dict dictionary description
 * \param def definition index
 *
 * \return allocated buffer with definition on success, NULL on error
 */
char *ydpdict_read_rtf(const ydpdict_t *pdict, int def)
{
	const ydpdict_priv_t *dict = pdict;
	char *text = NULL;
	uint32_t len = 0;

	if (dict == NULL || def >= dict->count) {
		errno = EINVAL;
		return NULL;
	}

	if (fseek(dict->dat, dict->words[def].index, SEEK_SET) == (off_t) -1)
		goto failure;

	if (fread(&len, sizeof(len), 1, dict->dat) != 1)
		goto failure;

	len = ydpdict_fix32(len);

	text = malloc(len + 1);

	if (text == NULL)
		goto failure;

	if (fread(text, 1, len, dict->dat) != len)
		goto failure;

	text[len] = 0;

	return text;

failure:
	if (text != NULL)
		free(text);

	return NULL;
}

/**
 * \brief Find specified word's index in dictionary
 *
 * \param dict dictionary description
 * \param word complete or partial word
 * 
 * \return definition index on success, -1 on error
 */
int ydpdict_find_word(const ydpdict_t *pdict, const char *word)
{
	const ydpdict_priv_t *dict = pdict;
	int i = 0;

	if (dict == NULL)
		return -1;

	for (; i < dict->count; i++) {
		if (strncasecmp(dict->words[i].word, word, strlen(word)) == 0)
			return i;
	}

	return -1;
}

/**
 * \brief Returns number of words in dictionary
 *
 * \param dict dictionary description
 * 
 * \return word count on success, -1 on error
 */
int ydpdict_get_count(const ydpdict_t *pdict)
{
	const ydpdict_priv_t *dict = pdict;

	if (dict == NULL)
		return -1;

	return dict->count;
}

/**
 * \brief Read word from dictionary
 *
 * \param dict dictionary description
 * \param def word index
 *
 * \return constant buffer with word on success, NULL on error
 */
char *ydpdict_get_word(const ydpdict_t *pdict, int def)
{
	const ydpdict_priv_t *dict = pdict;

	if (dict == NULL || def >= dict->count)
		return NULL;

	return dict->words[def].word;
}

/**
 * \brief Close dictionary
 *
 * \param dict dictionary definition
 *
 * \return 0 on success, -1 on error
 */
int ydpdict_close(ydpdict_t *pdict)
{
	ydpdict_priv_t *dict = pdict;

	if (dict == NULL)
		return -1;

	if (dict->words != NULL) {
		int i;

		for (i = 0; i < dict->count; i++)
			free(dict->words[i].word);

		free(dict->words);
		dict->words = NULL;
	}

	if (dict->dat != NULL) {
		fclose(dict->dat);
		dict->dat = NULL;
	}

	if (dict->idx != NULL) {
		fclose(dict->idx);
		dict->idx = NULL;
	}

	free(dict->xhtml_title);
	dict->xhtml_title = NULL;

	free(dict->xhtml_style);
	dict->xhtml_style = NULL;

	free(dict);

	return 0;
}

/**
 * \brief Append text to a string
 *
 * \param buf pointer to char*, freed on error
 * \param len pointer to buffer length
 * \param str string to be appended
 *
 * \return 0 on success, -1 on error
 */
static int ydpdict_append(char **buf, int *len, const char *str)
{
	int len1, len2;
	char *tmp;

	len1 = strlen(*buf);
	len2 = strlen(str);

	if (len1 + len2 > *len - 1) {
		while (len1 + len2 > *len - 1)
			*len <<= 1;

		if (!(tmp = realloc(*buf, *len))) {
			free(*buf);
			return -1;
		}

		*buf = tmp;
	}

	strcpy(*buf + len1, str);

	return 0;
}

/**
 * \brief Read word definition in XHTML format
 *
 * \param dict dictionary description
 * \param def definition index
 *
 * \return allocated buffer with definition on success, NULL on error
 */
char *ydpdict_read_xhtml(const ydpdict_t *pdict, int def)
{
	const ydpdict_priv_t *dict = pdict;
	char *buf = NULL;
	ydpdict_attr_t attr = 0, attr_pending = 0, attr_stack[32];
	int level = 0;
	int paragraph = 0, margin = 0, buf_len;
	unsigned char *rtf, *rtf_orig;

#undef APPEND
#define APPEND(x) \
	do { \
		if (ydpdict_append(&buf, &buf_len, x) == -1) \
			goto failure; \
	} while (0)

#undef APPEND_SPAN
#define APPEND_SPAN(class, color) \
	do { \
		if (dict->xhtml_use_style) { \
			APPEND("<font class=\""); \
			APPEND(class); \
			APPEND("\">"); \
		} else { \
			APPEND("<font color=\""); \
			APPEND(color); \
			APPEND("\">"); \
		} \
	} while (0)

#undef CLOSE_TAGS
#define CLOSE_TAGS() \
	do { \
		if ((attr & ATTR_SUPER) != 0) \
			APPEND("</sup>"); \
		\
		if ((attr & (ATTR_CF0 | ATTR_CF1 | ATTR_CF2 | ATTR_CF5)) != 0) \
			APPEND("</font>"); \
		\
		if ((attr & ATTR_I) != 0) \
			APPEND("</i>"); \
		\
		if ((attr & ATTR_B) != 0) \
			APPEND("</b>"); \
	} while (0)


	rtf = (unsigned char*) ydpdict_read_rtf(dict, def);

	if (rtf == NULL)
		return NULL;

	rtf_orig = rtf;

	buf_len = 256;

	buf = malloc(buf_len);

	if (buf == NULL)
		goto failure;

	buf[0] = 0;

	if (dict->xhtml_header) {
		const char *charset;

		switch (dict->encoding) {
			case YDPDICT_ENCODING_UTF8:
				charset = "utf-8";
				break;
			case YDPDICT_ENCODING_WINDOWS1250:
				charset = "windows-1250";
				break;
			default:
				charset = NULL;
		}

		APPEND("<?xml version=\"1.0\"?>\n<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n<html xmlns=\"http://www.w3.org/1999/xhtml\"><head>");

		APPEND("<title>");
		APPEND((dict->xhtml_title != NULL) ? dict->xhtml_title : "");
		APPEND("</title>");

		if (charset != NULL) {
			APPEND("<meta http-equiv=\"Content-type\" content=\"text/html; charset=");
			APPEND(charset);
			APPEND("\" />");
		}

		if (dict->xhtml_style != NULL) {
			APPEND("<style>");
			APPEND(dict->xhtml_style);
			APPEND("</style>");
		}

		APPEND("</head><body>");
	}

	// APPEND("<p>");

	while (*rtf != 0) {
		switch (*rtf) {
			case '{':
			{
				if (level < 16) {
					attr_stack[level] = attr_pending;
					level++;
				}

				if (margin && !(attr_stack[level - 1] & ATTR_SA))
					attr |= ATTR_SA;

				rtf++;

				break;
			}

			case '}':
			{
				if (level < 1)
					break;

				if (margin && (attr & ATTR_SA) != 0) {
					CLOSE_TAGS();
					// APPEND("</div><p>");
					// APPEND("<br>");
					attr = 0;
					margin = 0;
				}

				level--;
				attr_pending = attr_stack[level];

				rtf++;
				break;
			}

			case '\\':
			{
				int len = 0;
				char token[16];

				rtf++;

				while (isalnum(*rtf)) {
					if (len < sizeof(token) - 1)
						token[len++] = *rtf;

					rtf++;
				}

				token[len] = 0;

				if (*rtf == ' ')
					rtf++;

				if (strncmp(token, "par", 3) == 0 && (attr & ATTR_QC) == 0) {
					paragraph = 1;

					if (strcmp(token, "pard") == 0)
						attr_pending = 0;
				}

				if (strcmp(token, "line") == 0 && !paragraph)
					APPEND("<br>");

				if (strcmp(token, "b") == 0)
					attr_pending |= ATTR_B;

				if (strcmp(token, "b0") == 0)
					attr_pending &= ~ATTR_B;

				if (strcmp(token, "i") == 0)
					attr_pending |= ATTR_I;

				if (strcmp(token, "i0") == 0)
					attr_pending &= ~ATTR_I;

				if (strcmp(token, "cf0") == 0) {
					attr_pending &= ~(ATTR_CF0 | ATTR_CF1 | ATTR_CF2 | ATTR_CF5);
					attr_pending |= ATTR_CF0;
				}

				if (strcmp(token, "cf1") == 0) {
					attr_pending &= ~(ATTR_CF0 | ATTR_CF1 | ATTR_CF2 | ATTR_CF5);
					attr_pending |= ATTR_CF1;
				}

				if (strcmp(token, "cf2") == 0) {
					attr_pending &= ~(ATTR_CF0 | ATTR_CF1 | ATTR_CF2 | ATTR_CF5);
					attr_pending |= ATTR_CF2;
				}

				if (strcmp(token, "cf5") == 0) {
					attr_pending &= ~(ATTR_CF0 | ATTR_CF1 | ATTR_CF2 | ATTR_CF5);
					attr_pending |= ATTR_CF5;
				}

				if (strcmp(token, "super") == 0)
					attr_pending |= ATTR_SUPER;

				if (strcmp(token, "qc") == 0) {
					attr_pending |= ATTR_QC;
					attr |= ATTR_QC;
				}

				if (strncmp(token, "f", 1) == 0 && strcmp(token, "f1") != 0) {
					attr_pending |= ATTR_F;
					attr |= ATTR_F;
				}

				if (strcmp(token, "f1") == 0) {
					attr_pending |= ATTR_F1;
					attr |= ATTR_F1;
				}

				if (strncmp(token, "sa", 2) == 0) {
					if (!margin) {
						paragraph = 2;
					} else {
						// APPEND("<br>");
					}
				}

				break;
			}

			default:
			{
				// workaround for square bracket like in
				// {[\f1\cf5 pronunciation]}
				//
				if (*rtf == ']' && (attr & ATTR_F1) != 0 && (attr & ATTR_CF5) != 0)
					attr_pending &= ~ATTR_CF5;

				if (!isspace(*rtf) && paragraph != 0) {
					CLOSE_TAGS();

					if (margin) {
						// APPEND("</div>");
						attr &= ~ATTR_SA;
						margin = 0;
					} else {
						// APPEND("</p>");
					}

					if (paragraph == 2) {
						if (dict->xhtml_use_style)
							APPEND("<div class=\"example\">");
						else
							// APPEND("<div style=\"margin: -1em 2em auto 2em;\">");
							APPEND("<br>");
						margin = 1;
					} else {
						// APPEND("<p>");
						APPEND("<br><br>");
					}

					attr = 0;
					paragraph = 0;
				}

				if (!isspace(*rtf) && attr != attr_pending) {
					CLOSE_TAGS();

					if ((attr_pending & ATTR_B) != 0)
						APPEND("<b>");

					if ((attr_pending & ATTR_I) != 0)
						APPEND("<i>");

					if ((attr_pending & ATTR_CF0) != 0)
						APPEND_SPAN("cf0", "blue");

					if ((attr_pending & ATTR_CF1) != 0)
						APPEND_SPAN("cf1", "green");

					if ((attr_pending & ATTR_CF2) != 0)
						APPEND_SPAN("cf2", "red");

					if ((attr_pending & ATTR_CF5) != 0)
						APPEND_SPAN("cf5", "magenta");

					if ((attr_pending & ATTR_SUPER) != 0 && (attr & ATTR_SUPER) == 0)
						APPEND("<sup>");

					attr = attr_pending;
				}

				if ((attr & ATTR_QC) == 0 && (dict->encoding == YDPDICT_ENCODING_UTF8)) {
					if ((attr & ATTR_F1) != 0 && *rtf > 127 && *rtf < 160)
						APPEND(ydpdict_phonetic_to_utf8_table[*rtf - 128]);
					else if (((attr & ATTR_F1) != 0 && *rtf > 160) || *rtf > 127)
						APPEND(ydpdict_windows1250_to_utf8_table[*rtf - 128]);
					else if (*rtf == 127)
						APPEND("~");
					else if (*rtf == '&')
						APPEND("&amp;");
					else if (*rtf == '<')
						APPEND("&lt;");
					else if (*rtf == '>')
						APPEND("&gt;");
					else {
						char tmp[2] = { *rtf, 0 };
						APPEND(tmp);
					}
				}

				if ((attr & ATTR_QC) == 0 && (dict->encoding == YDPDICT_ENCODING_WINDOWS1250)) {
					if (*rtf == 127)
						APPEND("~");
					else {
						char tmp[2] = { *rtf, 0 };
						APPEND(tmp);
					}
				}

				rtf++;

				break;
			}
		}
	}

	CLOSE_TAGS();

	//if (!margin)
	//	APPEND("</p>");
	//else
	//	APPEND("</div>");

	free(rtf_orig);

	if (dict->xhtml_header)
		APPEND("</body></html>");

#undef APPEND

	return buf;

failure:
	free(rtf_orig);

	return NULL;
}

/**
 * \brief Set XHTML style
 *
 * \param dict dictionary description
 * \param style style
 *
 * \result 0 on success, -1 on error
 */
int ydpdict_set_xhtml_style(ydpdict_t *pdict, const char *style)
{
	ydpdict_priv_t *dict = pdict;

	if (dict == NULL) {
		errno = EINVAL;
		return -1;
	}

	free(dict->xhtml_style);
	dict->xhtml_style = NULL;

	if (style != NULL) {
		dict->xhtml_style = strdup(style);

		if (dict->xhtml_style == NULL)
			return -1;
	}

	return 0;
}

/**
 * \brief Set XHTML title
 *
 * \param dict dictionary description
 * \param title title
 *
 * \result 0 on success, -1 on error
 */
int ydpdict_set_xhtml_title(ydpdict_t *pdict, const char *title)
{
	ydpdict_priv_t *dict = pdict;

	if (dict == NULL) {
		errno = EINVAL;
		return -1;
	}

	free(dict->xhtml_title);
	dict->xhtml_title = NULL;

	if (title != NULL) {
		dict->xhtml_title = strdup(title);

		if (dict->xhtml_title == NULL)
			return -1;
	}

	return 0;
}

/**
 * \brief Toggle XHTML header output
 *
 * \param dict dictionary description
 * \param header header output flag
 *
 * \result 0 on success, -1 on error
 */
int ydpdict_set_xhtml_header(ydpdict_t *pdict, int header)
{
	ydpdict_priv_t *dict = pdict;

	if (dict == NULL) {
		errno = EINVAL;
		return -1;
	}

	dict->xhtml_header = header;

	return 0;
}

/**
 * \brief Toggle CSS style in XHTML
 *
 * If CSS style is enabled, all text attributes are described by CSS classes.
 * Otherwise all style information is embedded directly in style tag
 * atttributes.
 *
 * \param dict dictionary description
 * \param use_style style usage flag
 *
 * \result 0 on success, -1 on error
 */
int ydpdict_set_xhtml_use_style(ydpdict_t *pdict, int use_style)
{
	ydpdict_priv_t *dict = pdict;

	if (dict == NULL) {
		errno = EINVAL;
		return -1;
	}

	dict->xhtml_use_style = use_style;

	return 0;
}

/**
 * \brief Converts phonetic string to UTF-8
 *
 * \param input input string
 *
 * \return allocated buffer with converted string on success, NULL on error
 */
char *ydpdict_phonetic_to_utf8(const char *input)
{
	int i, len = 0;
	char *result;

	for (i = 0; input[i] != 0; i++) {
		if (((unsigned char*) input)[i] >= 128 && ((unsigned char*) input)[i] < 160)
			len += strlen(ydpdict_phonetic_to_utf8_table[((unsigned char*) input)[i] - 128]);
		else
			len++;
	}

	result = malloc(len + 1);

	if (result == NULL)
		return NULL;

	result[0] = 0;

	for (i = 0; input[i]; i++) {
		if (((unsigned char*) input)[i] >= 128 && ((unsigned char*) input)[i] < 160)
			strcat(result, ydpdict_phonetic_to_utf8_table[((unsigned char*) input)[i] - 128]);
		else {
			char tmp[2] = { input[i], 0 };

			strcat(result, tmp);
		}
	}

	return result;
}

/**
 * \brief Converts windows1250 string to UTF-8
 *
 * \param input input string
 *
 * \return allocated buffer with converted string on success, NULL on error
 */
char *ydpdict_windows1250_to_utf8(const char *input)
{
	int i, len = 0;
	char *result;

	for (i = 0; input[i]; i++) {
		if (((unsigned char*) input)[i] >= 128)
			len += strlen(ydpdict_windows1250_to_utf8_table[((unsigned char*) input)[i] - 128]);
		else
			len++;
	}

	result = malloc(len + 1);

	if (result == NULL)
		return NULL;

	result[0] = 0;

	for (i = 0; input[i]; i++) {
		if (((unsigned char*) input)[i] >= 128)
			strcat(result, ydpdict_windows1250_to_utf8_table[((unsigned char*) input)[i] - 128]);
		else {
			char tmp[2] = { input[i], 0 };

			strcat(result, tmp);
		}
	}

	return result;
}

/**
 * \brief Converts windows1250 string to UTF-8
 *
 * \note Codes 1..9 are converted to respective superscript digits and code
 *       10 is converted to superscript 0.
 *
 * \param input input string
 *
 * \return allocated buffer with converted string on success, NULL on error
 */
char *ydpdict_windows1250_super_to_utf8(const char *input)
{
	int i, len = 0;
	char *result;

	for (i = 0; input[i] != 0; i++) {
		if (((unsigned char*) input)[i] < 10)
			len += strlen(ydpdict_superscript_to_utf8_table[((unsigned char*) input)[i]]);
		else if (((unsigned char*) input)[i] >= 128)
			len += strlen(ydpdict_windows1250_to_utf8_table[((unsigned char*) input)[i] - 128]);
		else
			len++;
	}

	result = malloc(len + 1);

	if (result == NULL)
		return NULL;

	result[0] = 0;

	for (i = 0; input[i]; i++) {
		if (((unsigned char*) input)[i] < 10)
			strcat(result, ydpdict_superscript_to_utf8_table[((unsigned char*) input)[i]]);
		else if (((unsigned char*) input)[i] >= 128)
			strcat(result, ydpdict_windows1250_to_utf8_table[((unsigned char*) input)[i] - 128]);
		else {
			char tmp[2] = { input[i], 0 };

			strcat(result, tmp);
		}
	}

	return result;
}
