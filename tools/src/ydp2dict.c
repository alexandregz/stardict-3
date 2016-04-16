/*
 * This file is part of StarDict.
 *
 * StarDict is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * StarDict is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with StarDict.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "ydpdict.h"

long off, siz, n, i, cr, j;
FILE *F, *F2, *F3, *F4;
char *p, *p2, *p3, *ext;

char words[20000];
time_t t0;
struct tm *t;
char *s;
char path[250];
int cyc, m;

int dict_count = 4;
char * dict_dat[] = {"dict100.dat", "dict101.dat", "dict200.dat", "dict201.dat"};
char * dict_idx[] = {"dict100.idx", "dict101.idx", "dict200.idx", "dict201.idx"};
char * dict_label[] = {"English - Polish", "Polish - English", "Deutsch - Polnisch", "Polnisch - Deutsch"};


typedef struct {
	char *words;
	char *trans;
} PAIR;

PAIR *arr;


int stardict_strcmp(const char *s1, const char *s2)
{
	int a;
	a = strcasecmp(s1, s2);
	if (a == 0) {
		return strcmp(s1, s2);
	}
	return a;
}

int cmp(const void *s1, const void *s2)
{
	PAIR *a, *b;
	a= (PAIR *)s1;
	b= (PAIR *)s2;
	return stardict_strcmp(a->words, b->words);
}

int main (int argc, char *argv[])
{

	ydpdict_t *dict;
	arr=(PAIR *)malloc(sizeof(PAIR)*80000);
	setbuf(stdout, 0);

	if (argc > 1) {
		printf("Just run ydp2dict in a folder containing dict*.dat files\n");
		exit(0);
	}


	for (cyc = 0; cyc < dict_count; cyc++) {

		dict = ydpdict_open(dict_dat[cyc], dict_idx[cyc], YDPDICT_ENCODING_UTF8);
		if (!dict) {
			printf("\nCound't open file: %s.\n", dict_dat[cyc]);
			continue;
		}
		n = ydpdict_get_count(dict);

		for (i = 0; i < n; i++) {
			arr[i].words = ydpdict_get_word(dict, i);
			arr[i].trans = ydpdict_read_xhtml(dict, i);
		}

		// Creating the dictonary
		s = dict_label[cyc];

		sprintf(path, "ydp_%s.idx", s);
		ext=strstr(path, ".idx")+1;
		F3=fopen(path, "wb");
		strcpy(ext, "dict");
		F4=fopen(path, "wb");

		qsort(arr, n, sizeof(PAIR), cmp);
		for (off=i=0; i<n; i++) {

			fwrite(arr[i].trans, strlen(arr[i].trans), 1, F4);
			fwrite(arr[i].words, strlen(arr[i].words)+1, 1, F3);

			for (j=3; j>=0; j--) {
				fwrite(((char*)&off)+j, 1, 1, F3);
			}

			siz=strlen(arr[i].trans);

			for (j=3; j>=0; j--) {
				fwrite(((char*)&siz)+j, 1, 1, F3);
			}

			off+=siz;

			free(arr[i].words);
			free(arr[i].trans);
		}

		printf("cccc\n");

		siz=ftell(F3);
		fclose(F3);

		strcpy(ext, "ifo");
		F3=fopen(path, "wt");

		time(&t0);
		t=gmtime(&t0);

		fprintf(F3, "StarDict's dict ifo file\nversion=2.4.2\nwordcount=%li\nidxfilesize=%li\nbookname=", n, siz);
		fprintf(F3, "YDP %s dictionary", s);
		fprintf(F3, "\ndate=%i.%02i.%02i\nsametypesequence=h\n",
		t->tm_year+1900, t->tm_mon+1, t->tm_mday);

		fclose(F3);

		printf("\nTotal %li entries written: ydp_%s.*\n", n, s);
	}

	printf("\nRestart StarDict now!\n\n");
	return 0;
}
