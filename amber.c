
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

struct buffer {
	char *data;
	size_t pos;
};

struct PRICES {
//	char * currentNEMtime;
//	char * postcode;
//	char * networkprovider;
	struct DAILY {
		double networkdailyprice;
		double basicmeterdailyprice;
		double additionalsmartmeterdailyprice;
		double amberdailyprice;
		double totaldailyprice;
	} daily;
	struct UNIT {
		double networkkwhprice;
		double marketkwhprice;
		double greenkwhprice;
		double carbonneutralkwhprice;
		double totalfixedkwhprice;
		double lossfactor;
	} unit;
	double currentwholesaleKWHPrice;
	double operationaldemand;
	double renewablespercentage;
};

void parse_amber_json(char *ptr, struct PRICES *prices);
double sapn_solar_sponge(bool display);

size_t header_callback(char *buffer, size_t size, size_t nitems, void *user_data)
{
	//printf("%.*s", (int)(nitems * size), buffer);
	return(size * nitems);
}

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *user_data)
{
	struct buffer *out_buf = (struct buffer *)user_data;
	size_t buffersize = size * nmemb;

	//printf("size %ld, nmemb %ld\r\n",size, nmemb);

	char *newptr = realloc(out_buf->data, (out_buf->pos + buffersize + 1));
	if (newptr == NULL) {
		printf("Failed to allocate buffer\r\n");
		return 0;
	}

	out_buf->data = newptr;
	memcpy((void *)(out_buf->data + out_buf->pos), ptr, buffersize);
	out_buf->pos += buffersize;

	return(size * nmemb);
}

int http_json_request(struct buffer *out_buf)
{
	CURL *curl;
	CURLcode res;

	curl_global_init(CURL_GLOBAL_DEFAULT);

	curl = curl_easy_init();

	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, "https://api.amberelectric.com.au/prices/listprices");
		//curl_easy_setopt(curl, CURLOPT_HTTPHEADER, "Content-Type: application/json");
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{\"postcode\":\"5000\"}");
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, out_buf);

#ifdef SKIP_PEER_VERIFICATION
		/*
		 * If you want to connect to a site who isn't using a certificate that is
		 * signed by one of the certs in the CA bundle you have, you can skip the
		 * verification of the server's certificate. This makes the connection
		 * A LOT LESS SECURE.
		 *
		 * If you have a CA cert for the server stored someplace else than in the
		 * default bundle, then the CURLOPT_CAPATH option might come handy for
		 * you.
		*/
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
#endif

#ifdef SKIP_HOSTNAME_VERIFICATION
		/*
		 * If the site you're connecting to uses a different host name that what
		 * they have mentioned in their server certificate's commonName (or
		 * subjectAltName) fields, libcurl will refuse to connect. You can skip
		 * this check, but this will make the connection less secure.
		 */
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
#endif

		/* Perform the request, res will get the return code */
		res = curl_easy_perform(curl);
		/* Check for errors */
		if(res != CURLE_OK) {
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		} else {
			//printf("Bytes read %ld\r\n",out_buf.pos);
			//printf("[%.*s]\r\n",(int)out_buf.pos, out_buf.data);
		}

		/* always cleanup */
		curl_easy_cleanup(curl);
	}

	curl_global_cleanup();

	return res;
}

int main(void)
{
	CURLcode res;
	char *data;

	time_t now;
	struct tm timeinfo;
	char strftime_buf[64];

	struct buffer out_buf = {
		.data = data,
		.pos = 0
	};

	struct PRICES prices;

	while (1) {

		/* Allocate a modest buffer now, we can realloc later if needed */
		out_buf.data = malloc(16384);
		out_buf.pos = 0;
		res = http_json_request(&out_buf);

		if(res == CURLE_OK) {
			parse_amber_json(out_buf.data, &prices);

			time(&now);
			localtime_r(&now, &timeinfo);
			strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
			printf("\r\nLocal Time: %s\n", strftime_buf);

			/* SA Power Networks provides a Residential Time of Use ('Solar Sponge') Network Tariff.
		 	 * Amber doesn't yet support this pricing structure, so we correct for it ourselves. */
			prices.unit.networkkwhprice = sapn_solar_sponge(true);

			//printf("Current MEN Time: %s\r\n",prices.currentNEMtime);
			//printf("Postcode: %s\r\n",prices.postcode);
			//printf("Network Provider: %s\r\n",prices.networkprovider);

			printf("\r\nDaily Charges (Including GST):\r\n");
			printf("Fixed Network Charge		= %3.2lf c/day\r\n", prices.daily.networkdailyprice);
			printf("Basic Metering Charge		= %3.2lf c/day\r\n", prices.daily.basicmeterdailyprice);
			printf("Additional Smart Meter Charge	= %3.2lf c/day\r\n", prices.daily.additionalsmartmeterdailyprice);
			printf("Amber Price			= %3.2lf c/day\r\n", prices.daily.amberdailyprice);
			printf("Total Daily Price		= %3.2lf c/day\r\n", prices.daily.totaldailyprice);

			printf("\r\nPer kWh unit Charges (Including GST):\r\n");
			printf("Network Charges			= %3.3lf c/kWh\r\n", prices.unit.networkkwhprice);
			printf("Market Charges			= %3.3lf c/kWh\r\n", prices.unit.marketkwhprice);
			printf("100percent Green Power Offset	= %3.3lf c/kWh\r\n", prices.unit.greenkwhprice);
			printf("Carbon Neutral Offset		= %3.3lf c/kWh\r\n", prices.unit.carbonneutralkwhprice);
			printf("Total 				= %3.3lf c/kWh\r\n", prices.unit.totalfixedkwhprice);

			printf("\r\n");
			printf("Current Demand			= %.0f MW\r\n", prices.operationaldemand);
			printf("Renewables			= %.0f percent\r\n", prices.renewablespercentage * 100);
			printf("Loss Factor			= %3.3lf\r\n", prices.unit.lossfactor);

			printf("\r\n");
			printf("Spot Price			= %.02f c/kWh\r\n", prices.currentwholesaleKWHPrice);
			printf("Total Price			= %.01f c/kWh\r\n",
				(prices.currentwholesaleKWHPrice * prices.unit.lossfactor)
				+ prices.unit.totalfixedkwhprice);

		}
		free(out_buf.data);

		sleep(10);
	}
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

void parse_amber_json(char *ptr, struct PRICES *prices)
{
	double price_kwh;
	double totalfixed_kwh;

	cJSON *NEM;
	cJSON *data;
	cJSON *parameter;

	NEM = cJSON_Parse(ptr);

	data = cJSON_GetObjectItemCaseSensitive(NEM, "data");

	cJSON *currentNEMtime = cJSON_GetObjectItemCaseSensitive(data, "currentNEMtime");
	cJSON *postcode = cJSON_GetObjectItemCaseSensitive(data, "postcode");
	cJSON *networkProvider = cJSON_GetObjectItemCaseSensitive(data, "networkProvider");

	/* Amber has four staticPrices arrays labelled E1, E2, B1 and B1PFIT.
	 * Array 0 (E1) has the data we are after. */
	cJSON *staticPrices = cJSON_GetObjectItemCaseSensitive(data, "staticPrices");
	parameter = cJSON_GetArrayItem(staticPrices, 0);
	if (parameter != NULL) {

		/* Network Daily Price - Fixed daily charge payable to electricity distributor. */
		cJSON *networkdailyprice = cJSON_GetObjectItemCaseSensitive(parameter, "networkDailyPrice");
		if (networkdailyprice != NULL) prices->daily.networkdailyprice = strtod(networkdailyprice->valuestring, NULL);
		else printf("Can't find %s\r\n", networkdailyprice->string);

		/* Metering Charges - Fixed daily charge payable to metering provider. */
		cJSON *basicmeterdailyprice = cJSON_GetObjectItemCaseSensitive(parameter, "basicMeterDailyPrice");
		if (basicmeterdailyprice != NULL) prices->daily.basicmeterdailyprice = strtod(basicmeterdailyprice->valuestring, NULL);
		else printf("Can't find %s\r\n", basicmeterdailyprice->string);

		cJSON *additionalsmartmeterdailyprice = cJSON_GetObjectItemCaseSensitive(parameter, "additionalSmartMeterDailyPrice");
		if (additionalsmartmeterdailyprice != NULL) prices->daily.additionalsmartmeterdailyprice = strtod(additionalsmartmeterdailyprice->valuestring, NULL);
		else printf("Can't find %s\r\n", additionalsmartmeterdailyprice->string);

		/* Amber Electric's Fee - Fixed daily charge payable to Amber Electric. */
		cJSON *amberdailyprice = cJSON_GetObjectItemCaseSensitive(parameter, "amberDailyPrice");
		if (amberdailyprice != NULL) prices->daily.amberdailyprice = strtod(amberdailyprice->valuestring, NULL);
		else printf("Can't find %s\r\n", amberdailyprice->string);

		/* Should be total of above daily charges. */
		cJSON *totaldailyprice = cJSON_GetObjectItemCaseSensitive(parameter, "totalDailyPrice");
		if (totaldailyprice != NULL) prices->daily.totaldailyprice = strtod(totaldailyprice->valuestring, NULL);
		else printf("Can't find %s\r\n", totaldailyprice->string);

		/* Network Charge - Payable to electricity distributor. May not accurately reflect
		 * Time of Use (ToU) network tariffs. */
		cJSON *networkkwhprice = cJSON_GetObjectItemCaseSensitive(parameter, "networkKWHPrice");
		if (networkkwhprice != NULL) prices->unit.networkkwhprice = strtod(networkkwhprice->valuestring, NULL);
		else printf("Can't find %s\r\n", networkkwhprice->string);

		/* Market charges - Includes charges to AEMO and Environmental Certificate Costs. */
		cJSON *marketkwhprice = cJSON_GetObjectItemCaseSensitive(parameter, "marketKWHPrice");
		if (marketkwhprice != NULL) prices->unit.marketkwhprice = strtod(marketkwhprice->valuestring, NULL);
		else printf("Can't find %s\r\n", marketkwhprice->string);

		/* Carbon offset charges specific to your Amber plan. You will be on either on the default
		 * Carbon Neutral or 100% GreenPower Plan - Cost to purchase carbon credits. */
		cJSON *greenkwhprice = cJSON_GetObjectItemCaseSensitive(parameter, "greenKWHPrice");
		if (greenkwhprice != NULL) prices->unit.greenkwhprice = strtod(greenkwhprice->valuestring, NULL);
		else printf("Can't find %s\r\n", greenkwhprice->string);

		cJSON *carbonneutralkwhprice = cJSON_GetObjectItemCaseSensitive(parameter, "carbonNeutralKWHPrice");
		if (carbonneutralkwhprice != NULL) prices->unit.carbonneutralkwhprice = strtod(carbonneutralkwhprice->valuestring, NULL);
		else printf("Can't find %s\r\n", carbonneutralkwhprice->string);

		/* Should be total of above kWh charges. */
		cJSON *totalfixedkwhprice = cJSON_GetObjectItemCaseSensitive(parameter, "totalfixedKWHPrice");
		if (totalfixedkwhprice != NULL) prices->unit.totalfixedkwhprice = strtod(totalfixedkwhprice->valuestring, NULL);
		else printf("Can't find %s\r\n", totalfixedkwhprice->string);

		/* Loss Factor */
		cJSON *lossfactor = cJSON_GetObjectItemCaseSensitive(parameter, "lossFactor");
		if (lossfactor != NULL) prices->unit.lossfactor = strtod(lossfactor->valuestring, NULL);
		else printf("Can't find %s\r\n", lossfactor->string);

	}

	cJSON *variablePrices = cJSON_GetObjectItemCaseSensitive(data, "variablePricesAndRenewables");
	//cJSON_ArrayForEach(parameter, variablePrices)
	//for (int i = 45; i <= 50; i++) {
	if (variablePrices != NULL) {

		parameter = cJSON_GetArrayItem(variablePrices, 48);
		if (parameter != NULL) {
			//cJSON *period = cJSON_GetObjectItemCaseSensitive(parameter, "period");
			//printf("Period Ending: %s ",period->valuestring);

			cJSON *price = cJSON_GetObjectItemCaseSensitive(parameter, "wholesaleKWHPrice");
			if (price != NULL) prices->currentwholesaleKWHPrice = strtod(price->valuestring, NULL);

			cJSON *renewables = cJSON_GetObjectItemCaseSensitive(parameter, "renewablesPercentage");
			if (renewables != NULL) prices->renewablespercentage = strtod(renewables->valuestring, NULL);

			cJSON *operationaldemand = cJSON_GetObjectItemCaseSensitive(parameter, "operationalDemand");
			if (operationaldemand!= NULL ) prices->operationaldemand = strtod(operationaldemand->valuestring, NULL);
		}
	}

	cJSON_Delete(NEM);
}
