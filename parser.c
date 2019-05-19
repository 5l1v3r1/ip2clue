/*
 * Author: Catalin(ux) M. BOIE
 * Description: parser for IP files
 */

#include <i_config.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <i_util.h>
#include <parser_core.h>
#include <parser_text.h>
#include <parser_ip2location.h>

/*
 * Parse file and store data in @db database
 */
int ip2clue_parse_file(struct ip2clue_db *db, const char *file_name,
	const char *format)
{
	struct timeval ts, te;
	int err;

	gettimeofday(&ts, NULL);

	db->ts_load = ts.tv_sec;
	db->ts = 0;
	snprintf(db->file, sizeof(db->file), "%s", file_name);

	err = -1;

	/* webhosting.info */
	if (!strcasecmp(format, "webhosting")) {
		db->format = IP2CLUE_FORMAT_WEBHOSTING;
		err = ip2clue_parse_text(db);
	} else if (!strcasecmp(format, "maxmind")) {
		db->format = IP2CLUE_FORMAT_MAXMIND;
		err = ip2clue_parse_text(db);
	} else if (!strcasecmp(format, "maxmind-v6")) {
		db->format = IP2CLUE_FORMAT_MAXMIND_V6;
		err = ip2clue_parse_text(db);
	} else if (!strcasecmp(format, "software77")) {
		db->format = IP2CLUE_FORMAT_SOFTWARE77;
		err = ip2clue_parse_text(db);
	} else if (!strcasecmp(format, "ip2location")) {
		db->format = IP2CLUE_FORMAT_IP2LOCATION;
		err = ip2clue_parse_ip2location(db);
	} else {
		snprintf(ip2clue_error, sizeof(ip2clue_error),
			"invalid file format [%s]", format);
		return -1;
	}

	if (err == -1)
		return -1;

	gettimeofday(&te, NULL);

	db->elap_load_ms = (te.tv_sec - ts.tv_sec) * 1000 +
		(te.tv_usec - ts.tv_usec) / 1000;

	db->usage_count = 0;
	db->lookup_ok = 0;
	db->lookup_notfound = 0;
	db->lookup_malformed = 0;

	return 0;
}

/*
 * Init a list of databases
 */
void ip2clue_list_init(struct ip2clue_list *list)
{
	list->number = 0;
	list->entries = NULL;
}

/*
 * Destroys a list of databases
 */
void ip2clue_list_destroy(struct ip2clue_list *list)
{
	unsigned int i;

	if (list->entries == NULL)
		return;

	for (i = 0; i < list->number; i++)
		ip2clue_destroy(list->entries[i]);
	free(list->entries);

	list->number = 0;
	list->entries = NULL;
}

/*
 * Output statistics about the databases
 * TODO: Add number of lookups (successful or not) and number of reloads.
 */
void ip2clue_list_stats(char *out, const size_t out_size, struct ip2clue_list *list)
{
	size_t rest, line_size;
	char line[512];
	struct ip2clue_db *db;
	unsigned int i;

	rest = out_size;

	strcpy(out, "");

	line_size = snprintf(line, sizeof(line), "%u database(s)",
		list->number);
	if (rest < line_size)
		return;

	strcat(out, line);
	rest -= line_size;

	for (i = 0; i < list->number; i++) {
		db = list->entries[i];
		line_size = snprintf(line, sizeof(line),
			"\n"
			"db %u: format [%s], %s, entries=%llu"
			", build_ts=%ld, load_ts=%ld, load=%ums"
			", file=[%s], mem=%lluB"
			" ok/notfound/malformed=%llu/%llu/%llu",
			i, ip2clue_format(db->format),
			db->v4_or_v6 == 4 ? "ipv4" : "ipv6",
			db->no_of_cells,
			db->ts, db->ts_load, db->elap_load_ms,
			db->file, db->mem,
			db->lookup_ok, db->lookup_notfound, db->lookup_malformed);
		if (rest < line_size)
			return;

		strcat(out, line);
		rest -= line_size;
	}
}

/*
 * Allocates a list structure
 */
int ip2clue_list_alloc(struct ip2clue_list *a, const unsigned int number)
{
	unsigned int mem;

	mem = number * sizeof(void *);
	a->entries = (struct ip2clue_db **) malloc(mem);
	if (a->entries == NULL) {
		snprintf(ip2clue_error, sizeof(ip2clue_error),
			"cannot alloc %u bytes", mem);
		return -1;
	}

	memset(a->entries, 0, mem);

	a->number = number;

	return 0;
}

/*
 * Clone a list
 */
int ip2clue_list_clone(struct ip2clue_list *dst, struct ip2clue_list *src)
{
	unsigned int i;

	ip2clue_list_destroy(dst);

	if (ip2clue_list_alloc(dst, src->number) != 0)
		return -1;

	for (i = 0; i < src->number; i++) {
		dst->entries[i] = src->entries[i];
		src->entries[i]->usage_count++;
	}

	return 0;
}

/*
 * Replaces a list with other
 */
int ip2clue_list_replace(struct ip2clue_list *dst, struct ip2clue_list *src)
{
	int ret;

	ret = ip2clue_list_clone(dst, src);
	if (ret != 0)
		return ret;
	ip2clue_list_destroy(src);

	return 0;
}

/*
 * Parse an option string
 * @option can be something like 'maxmind:file1, ip2location:file2'.
 */
static int ip2clue_list_load_one(struct ip2clue_db *db, const char *dir,
	const char *option)
{
	char *file;
	char format[32];
	unsigned int i, format_size;
	char final_file[1024];

	file = strchr(option, ':');
	if (file == NULL) {
		snprintf(ip2clue_error, sizeof(ip2clue_error),
			"invalid format [%s]", option);
		return -1;
	}
	file++;

	i = 0;
	format_size = sizeof(format);
	while ((*option != '\0') && (*option != ':') && (i < format_size - 1)) {
		format[i] = *option;
		i++;
		option++;
	}
	format[i] = '\0';

	snprintf(final_file, sizeof(final_file), "%s/%s", dir, file);
	return ip2clue_parse_file(db, final_file, format);
}

/*
 * Searches a db by file name
 */
static struct ip2clue_db *ip2clue_db_search(const struct ip2clue_list *l,
	const char *file)
{
	unsigned int i;
	struct ip2clue_db *db;

	if (l == NULL)
		return NULL;

	for (i = 0; i < l->number; i++) {
		db = l->entries[i];
		if (strcmp(db->file, file) == 0)
			return db;
	}

	return NULL;
}

/*
 * Refreshes a db list if files changed
 * @options can be something like 'maxmind:file1, ip2location:file2'.
 */
int ip2clue_list_refresh(struct ip2clue_list *dst, struct ip2clue_list *src,
	const char *dir, const char *options)
{
	struct ip2clue_db *db;
	struct ip2clue_split s;
	int files, err, force_load;
	unsigned int i;
	time_t mtime = 0;
	struct stat S;
	char *file;
	char path[1024];
	unsigned int mem;

	files = ip2clue_split(&s, options, ", ");
	if (files == -1)
		return -1;

	if (ip2clue_list_alloc(dst, files) != 0)
		return -1;

	for (i = 0; i < dst->number; i++) {
		/* find the last change of the file */
		file = strchr(s.fields[i], ':');
		if (file == NULL) {
			snprintf(ip2clue_error, sizeof(ip2clue_error),
				"invalid option [%s] (no ':')", s.fields[i]);
			goto out_free_dst; /* Invalid entry */
		}
		file++;

		force_load = 1;

		snprintf(path, sizeof(path), "%s/%s", dir, file);

		/* See if we can find the db in the old list */
		db = ip2clue_db_search(src, path);
		if (db != NULL) {
			/* is it still fresh? */
			err = stat(path, &S);
			if (err == 0)
				mtime = S.st_mtime;

			/* file changed? */
			if (db->ts_load >= mtime)
				force_load = 0;
		}

		if (force_load == 1) {
			mem = sizeof(struct ip2clue_db);
			db = (struct ip2clue_db *) malloc(mem);
			if (db == NULL) {
				snprintf(ip2clue_error, sizeof(ip2clue_error),
					"cannot alloc %u bytes for db", mem);
				goto out_free_dst;
			}

			/* TODO: s.fields[i] => get_value(&s, i) */
			err = ip2clue_list_load_one(db, dir, s.fields[i]);
			if (err != 0) {
				free(db);
				goto out_free_dst;
			}
		}
		db->usage_count++;

		dst->entries[i] = db;
	}

	return 0;

	out_free_dst:
	ip2clue_list_destroy(dst);

	return -1;
}

/*
 * Parse an option string
 * @options can be something like 'maxmind:file1, ip2location:file2'.
 */
int ip2clue_list_load(struct ip2clue_list *list, const char *dir,
	const char *options)
{
	return ip2clue_list_refresh(list, NULL, dir, options);
}
