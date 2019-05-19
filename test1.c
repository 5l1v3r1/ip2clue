#include <i_config.h>

#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <i_types.h>
#include <i_util.h>
#include <parser.h>

int main(void)
{
	int ret, loops;
	struct ip2clue_db db;
	struct ip2clue_cell_v4 *cell4;
	struct ip2clue_cell_v6 *cell6;
	struct timeval s, e;
	long diff, total_loops;
	char *test_ips[500000];
	char sip[64];
	char *file, *type;
	char dump[256];
	char *key;

	/* v4 */
	//file = "samples/maxmind1.csv"; type = "maxmind";
	//file = "samples/ip-to-country.csv"; type = "webhosting";
	//file = "samples/IpToCountry.csv"; type = "software77";
	file = "samples/IP-COUNTRY-REGION-CITY-LATITUDE-LONGITUDE-ZIPCODE-TIMEZONE-ISP-DOMAIN-NETSPEED-AREACODE-WEATHER-SAMPLE.BIN"; type = "ip2location";
	//file = "samples/IP-COUNTRY-SAMPLE.BIN"; type = "ip2location";

	/* v6 */
	file = "samples/IPV6-COUNTRY.SAMPLE.BIN"; type = "ip2location";

	ret = ip2clue_parse_file(&db, file, type);
	if (ret != 0)
		return 1;

	printf("Loading took %ums.\n", db.elap_load_ms);
	if (db.ts_load > 0)
		printf("DB was loaded at %s", ctime(&db.ts_load));
	if (db.ts > 0)
		printf("DB was built by provider at %s", ctime(&db.ts));
	printf("DB takes %llu bytes.\n", db.mem);

	if (db.v4_or_v6 == IP2CLUE_TYPE_V4) {
		total_loops = 500000;
		for (loops = 0; loops < total_loops; loops++) {
			snprintf(sip, sizeof(sip), "%ld.%ld.%ld.%ld",
				random() % 255, random() % 255, random() % 255, random() % 255);
			test_ips[loops] = strdup(sip);
		}

		gettimeofday(&s, NULL);
		loops = 0;
		while (loops < total_loops) {
			cell4 = ip2clue_search_v4(&db, test_ips[loops]);
			loops++;
		}
		gettimeofday(&e, NULL);
		diff = (e.tv_sec - s.tv_sec) * 1000 + (e.tv_usec - s.tv_usec) / 1000;
		printf("Took %ldms for %d loops (%ld reqs/ms).\n",
			diff, loops, loops / (diff == 0 ? 1 : diff));

		for (loops = 0; loops < total_loops; loops++)
			free(test_ips[loops]);

		cell4 = ip2clue_search_v4(&db, "2.6.190.56");
		if (cell4 == NULL) {
			printf("Error in lookup!\n");
			return 2;
		}
		if (strcmp(cell4->country_short, "GB") != 0) {
			printf("Invalid answer. Expected GB, got %s!\n", cell4->country_short);
			abort();
		}
		printf("ip_start=%u, ip_end=%u, country_short=[%s]\n",
			cell4->ip_start, cell4->ip_end, cell4->country_short);

		cell4 = ip2clue_search_v4(&db, "222.252.0.1");
		if (cell4 == NULL) {
			printf("Error in lookup!\n");
			return 2;
		}
		if (strcmp(cell4->country_short, "VN") != 0) {
			printf("Invalid answer. Expected VN, got %s!\n", cell4->country_short);
			abort();
		}
		printf("ip_start=%u, ip_end=%u, country_short=[%s]\n",
			cell4->ip_start, cell4->ip_end, cell4->country_short);

		/* Out of bounds */
		cell4 = ip2clue_search_v4(&db, "0.0.0.0");
		if (cell4 != NULL)
			printf("ERROR: 0.0.0.0 din not returned NULL but [%s]!\n", cell4->country_short);

		/* Out of bounds */
		cell4 = ip2clue_search_v4(&db, "255.255.255.255");
		if (cell4 != NULL)
			printf("ERROR: 255.255.255.255 din not returned NULL but [%s]!\n", cell4->country_short);
	} else if (db.v4_or_v6 == IP2CLUE_TYPE_V6) {
		/* test 1 */
		key = "2001:0960:0002:04D2:0000:0000:0000:0000";
		printf("Looking for %s...\n", key);
		cell6 = ip2clue_search_v6(&db, key);
		if (cell6 == NULL) {
			printf("Error in lookup!\n");
			return 2;
		}
		ip2clue_dump_cell_v6(dump, sizeof(dump), cell6);
		printf("Got %s.\n", dump);
		if (strcmp(cell6->country_short, "NL") != 0) {
			printf("Invalid answer. Expected 'NL'!\n");
			abort();
		}

		/* test 2 */
		key = "2001:4830:00EA::";
		printf("Looking for %s...\n", key);
		cell6 = ip2clue_search_v6(&db, key);
		if (cell6 == NULL) {
			printf("Error in lookup!\n");
			return 2;
		}
		ip2clue_dump_cell_v6(dump, sizeof(dump), cell6);
		printf("Got %s.\n", dump);
		if (strcmp(cell6->country_short, "-") != 0) {
			printf("Invalid answer. Expected '-'!\n");
			abort();
		}

		/* test 3 */
		key = "2001:ff8:1:0:0:0:0:0";
		printf("Looking for %s...\n", key);
		cell6 = ip2clue_search_v6(&db, key);
		if (cell6 == NULL) {
			printf("Error in lookup!\n");
			return 2;
		}
		ip2clue_dump_cell_v6(dump, sizeof(dump), cell6);
		printf("Got %s.\n", dump);
		if (strcmp(cell6->country_short, "MO") != 0) {
			printf("Invalid answer. Expected 'MO'!\n");
			abort();
		}
	} else {
		printf("v4_or_v6 is wrong!\n");
		abort();
	}

	ip2clue_destroy(&db);

	return 0;
}
