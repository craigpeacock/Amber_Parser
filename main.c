
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <time.h>
#include <libgen.h>
#include "http.h"
#include "parser.h"

static void print_usage(char *prg);
double sapn_solar_sponge(bool display);
int log_prices(FILE *fhandle, struct PRICES *prices);
int display_prices(struct PRICES *prices);
void display_extended_pricing(char *postcode);

#define IDLE 	0
#define FETCH 	1

int main(int argc, char **argv)
{
	CURLcode res;
	char *data;
	char postcode[4] = {"5000"};

	int modulo;

	time_t now;
	struct tm timeinfo;

	struct buffer out_buf = {
		.data = data,
		.pos = 0
	};

	int previous_period = -1;
	double previous_price = 0;

	struct PRICES prices;
	prices.networkprovider = NULL;

	unsigned int state = IDLE;

	FILE *fhandle;

	fhandle = fopen("Amberdata.csv","a+");
	if (fhandle == NULL) {
		printf("Unable to open Amberdata.csv for writing\r\n");
		exit(1);
	}

	printf("JSON File Parser\r\nAmber Electric\r\n");

	int opt;

	while ((opt = getopt(argc, argv, "p:i?")) != -1) {
		switch (opt) {
		case 'p':
			printf("Postcode = %s\r\n",optarg);
			strncpy(postcode, optarg, 4);
			break;

		case 'i':
			display_extended_pricing(postcode);
			exit(1);
			break;

		default:
			print_usage(basename(argv[0]));
			exit(1);
			break;
		}
	}

	state = FETCH;

	while (1) {

		/* Get Time */
		time(&now);
		localtime_r(&now, &timeinfo);
		//printf("[%02d:%02d] %d\r\n",timeinfo.tm_min, timeinfo.tm_sec, state);

		switch (state) {

			case IDLE:
				if (timeinfo.tm_sec == 0) {
					modulo = timeinfo.tm_min % 10;
					//printf("[mod] = %d\r\n",modulo);
					if ((modulo == 2) | (modulo == 7)) {
						state = FETCH;
					}
				}
				break;

			case FETCH:
				/* Start fetching a new JSON file. We keep trying every 5 seconds until */
				/* the settlement time is different from the previous period */

				/* Allocate a modest buffer now, we can realloc later if needed */
				if (out_buf.data != NULL) free(out_buf.data);
				out_buf.data = malloc(65536);
				if (out_buf.data == NULL) {
					printf("Error allocating memory\r\n");
				}
				out_buf.pos = 0;

				/* Fetch JSON file */
				res = http_json_request(&out_buf, postcode);
				if(res == CURLE_OK) {
					/* If HTTP request was successful, parse request */
					parse_amber_json(out_buf.data, &prices);

					/* Print a dot each time we make a HTTP request */
					//printf(".");
					//fflush(stdout);

					if ((prices.latestperiod.tm_min != previous_period) ||
					    (prices.currentwholesaleKWHPrice != previous_price)) {
						/* Change in settlement time, log new period */
						previous_period = prices.latestperiod.tm_min;
						previous_price = prices.currentwholesaleKWHPrice;

						display_prices(&prices);
						log_prices(fhandle, &prices);

						/* Success, go back to IDLE */
						state = IDLE;
						break;
					}

				}
				/* No luck, we will try again in ten */
				sleep(10);
				break;

			}
			sleep(1);
	} // end while - we should never get here
	//free(out_buf.data);
}

static void print_usage(char *prg)
{
	fprintf(stderr, "Usage: %s [options]\n",prg);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "	-p <postcode>\n");
	fprintf(stderr, "	-i 		Show daily and unit charges\n");
	fprintf(stderr, "\n");
}

double sapn_solar_sponge(bool display)
{
	double networkwhprice_tod;
	time_t now;
	struct tm timeinfo;
	char strftime_buf[64];

	time(&now);
	localtime_r(&now, &timeinfo);
	if (display) printf("Network ToD Tariff: ");

	if ((timeinfo.tm_hour >= 10) && (timeinfo.tm_hour < 15)) {
		if (display) printf("Solar Sponge\r\n");
		/* Prices are GST ex, multipled by 1.1 to correct for GST */
		networkwhprice_tod = 3.45 * 1.1;
	}
	else if ((timeinfo.tm_hour >= 1)  && (timeinfo.tm_hour <= 5)) {
		if (display) printf("Off-Peak\r\n");
		networkwhprice_tod = 6.9 * 1.1;
	}
	else {
		if (display) printf("Peak\r\n");
		networkwhprice_tod = 17.23 * 1.1;
	}

	return(networkwhprice_tod);
}

int log_prices(FILE *fhandle, struct PRICES *prices)
{

	time_t now;
	struct tm timeinfo;

	time(&now);
	localtime_r(&now, &timeinfo);

	fprintf(fhandle,"%04d-%02d-%02d %02d:%02d:%02d,",
		timeinfo.tm_year + 1900,
		timeinfo.tm_mon + 1,
		timeinfo.tm_mday,
		timeinfo.tm_hour,
		timeinfo.tm_min,
		timeinfo.tm_sec);

	fprintf(fhandle,"%04d-%02d-%02d %02d:%02d:%02d,",
		prices->latestperiod.tm_year + 1900,
		prices->latestperiod.tm_mon + 1,
		prices->latestperiod.tm_mday,
		prices->latestperiod.tm_hour,
		prices->latestperiod.tm_min,
		prices->latestperiod.tm_sec);

	if (prices->periodsource == SOURCE_5MIN)
		fprintf(fhandle,"(5MIN),");
	else if (prices->periodsource == SOURCE_30MIN)
		fprintf(fhandle,"(30MIN),");

	fprintf(fhandle,"%.04f,%.01f,",
		prices->currentwholesaleKWHPrice,
		(prices->currentwholesaleKWHPrice * prices->unit.lossfactor) + prices->unit.totalfixedkwhprice);

	fprintf(fhandle,"%.0f\r\n", prices->renewablespercentage * 100);

	fflush(fhandle);
}


int display_prices(struct PRICES *prices)
{
	time_t now;
	struct tm timeinfo;

	time(&now);
	localtime_r(&now, &timeinfo);

	printf("Local time %04d-%02d-%02d %02d:%02d:%02d, ",
		timeinfo.tm_year + 1900,
		timeinfo.tm_mon + 1,
		timeinfo.tm_mday,
		timeinfo.tm_hour,
		timeinfo.tm_min,
		timeinfo.tm_sec);

	printf("Period ending %04d-%02d-%02d %02d:%02d:%02d ",
		prices->latestperiod.tm_year + 1900,
		prices->latestperiod.tm_mon + 1,
		prices->latestperiod.tm_mday,
		prices->latestperiod.tm_hour,
		prices->latestperiod.tm_min,
		prices->latestperiod.tm_sec);

	if (prices->periodsource == SOURCE_5MIN) printf("(5MIN), ");
	else if (prices->periodsource == SOURCE_30MIN) printf("(30MIN), ");

	printf("Spot Price = %.04f c/kWh, Total Price = %.01f c/kWh\r\n",
		prices->currentwholesaleKWHPrice,
		(prices->currentwholesaleKWHPrice * prices->unit.lossfactor) + prices->unit.totalfixedkwhprice);
}

void display_extended_pricing(char *postcode)
{
	char *data;
	char networkprovider[30] = {0};

	struct buffer out_buf = {
		.data = data,
		.pos = 0
	};

	struct PRICES prices;
	prices.networkprovider = (char *)&networkprovider;

	/* Allocate a modest buffer now, we can realloc later if needed */
	out_buf.data = malloc(16384);
	out_buf.pos = 0;
	CURLcode res = http_json_request(&out_buf, postcode);

	if(res == CURLE_OK) {

		parse_amber_json(out_buf.data, &prices);

		/* SA Power Networks provides a Residential Time of Use ('Solar Sponge') Network Tariff.
		 * Amber doesn't yet support this pricing structure, so we correct for it ourselves. */
		//prices.unit.networkkwhprice = sapn_solar_sponge(true);

		prices.unit.totalfixedkwhprice = 	prices.unit.networkkwhprice +
							prices.unit.marketkwhprice +
							prices.unit.carbonneutralkwhprice;

		printf("Current NEM Time 		%04d-%02d-%02d %02d:%02d:%02d\r\n",
			prices.currentNEMtime.tm_year + 1900,
			prices.currentNEMtime.tm_mon + 1,
			prices.currentNEMtime.tm_mday,
			prices.currentNEMtime.tm_hour,
			prices.currentNEMtime.tm_min,
			prices.currentNEMtime.tm_sec);

		printf("Postcode 			%d\r\n",prices.postcode);
		printf("Network Provider 		%s\r\n",prices.networkprovider);

		printf("\r\nDaily Charges:\r\n");
		printf("Fixed Network Charge		%3.2lf c/day\r\n", prices.daily.networkdailyprice);
		printf("Basic Metering Charge		%3.2lf c/day\r\n", prices.daily.basicmeterdailyprice);
		printf("Additional Smart Meter Charge	%3.2lf c/day\r\n", prices.daily.additionalsmartmeterdailyprice);
		printf("Amber Price			%3.2lf c/day\r\n", prices.daily.amberdailyprice);
		printf("Total Daily Price		%3.2lf c/day\r\n", prices.daily.totaldailyprice);

		printf("\r\nPer kWh unit Charges:\r\n");
		printf("Network Charges			%3.3lf c/kWh\r\n", prices.unit.networkkwhprice);
		printf("Market Charges			%3.3lf c/kWh\r\n", prices.unit.marketkwhprice);
		printf("100 percent Green Power Offset  %3.3lf c/kWh\r\n", prices.unit.greenkwhprice);
		printf("Carbon Neutral Offset		%3.3lf c/kWh\r\n", prices.unit.carbonneutralkwhprice);
		printf("Total 				%3.3lf c/kWh\r\n", prices.unit.totalfixedkwhprice);

		printf("\r\n");
		//printf("Current Demand			%.0f MW\r\n", prices.operationaldemand);
		printf("Renewables			%.0f percent\r\n", prices.renewablespercentage * 100);
		printf("Loss Factor			%3.5lf\r\n", prices.unit.lossfactor);

		printf("\r\n");
		printf("Wholesale Spot Market Price	%.02f c/kWh\r\n", prices.currentwholesaleKWHPrice);
		printf("Total Price			%.01f c/kWh\r\n",
			(prices.currentwholesaleKWHPrice * prices.unit.lossfactor)
			+ prices.unit.totalfixedkwhprice);
		printf("All prices include GST\r\n");
	}
	free(out_buf.data);
}
