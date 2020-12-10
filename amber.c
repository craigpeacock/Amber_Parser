
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

void parse_amber_json(char *ptr);

struct buffer {
	char *data;
	size_t pos;
};

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *user_data)
{
	struct buffer *out_buf = (struct buffer *)user_data;

	//printf("size %ld, nmemb %ld\r\n",size, nmemb);

	memcpy((void *)(out_buf->data + out_buf->pos), ptr, size * nmemb);
	out_buf->pos += size * nmemb;

	return(size * nmemb);
}

int main(void)
{
	CURL *curl;
	CURLcode res;

	char *data;
	data = malloc(131072);

	struct buffer out_buf = {
		.data = data,
		.pos = 0
	};

	curl_global_init(CURL_GLOBAL_DEFAULT);

	curl = curl_easy_init();

	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, "https://api.amberelectric.com.au/prices/listprices");
		//curl_easy_setopt(curl, CURLOPT_HTTPHEADER, "Content-Type: application/json");
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{\"postcode\":\"5000\"}");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out_buf);

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
			parse_amber_json(out_buf.data);
		}

		/* always cleanup */
		curl_easy_cleanup(curl);
	}

	curl_global_cleanup();

	return 0;
}

void parse_amber_json(char *ptr)
{
	double price_kwh;

	const cJSON *data;
	const cJSON *parameter;

	cJSON *NEM = cJSON_Parse(ptr);

	data = cJSON_GetObjectItemCaseSensitive(NEM, "data");

	cJSON *currentNEMtime = cJSON_GetObjectItemCaseSensitive(data, "currentNEMtime");
	cJSON *postcode = cJSON_GetObjectItemCaseSensitive(data, "postcode");
	cJSON *networkProvider = cJSON_GetObjectItemCaseSensitive(data, "networkProvider");

	printf("Current MEN Time: %s\r\n",currentNEMtime->valuestring);
	printf("Postcode: %s\r\n",postcode->valuestring);
	printf("Network Provider: %s\r\n",networkProvider->valuestring);

	cJSON *staticPrices = cJSON_GetObjectItemCaseSensitive(data, "staticPrices");
	/* Amber has four staticPrices arrays labelled E1, E2, B1 and B1PFIT.
	 * Array 0 (E1) has the data we are after. */
	parameter = cJSON_GetArrayItem(staticPrices, 0);

	/* Network Daily Price - Fixed daily charge payable to electricity distributor. */
	cJSON *networkdailyprice = cJSON_GetObjectItemCaseSensitive(parameter, "networkDailyPrice");
	/* Metering Charges - Fixed daily charge payable to metering provider. */
	cJSON *basicmeterdailyprice = cJSON_GetObjectItemCaseSensitive(parameter, "basicMeterDailyPrice");
	cJSON *additionalsmartmeterdailyprice = cJSON_GetObjectItemCaseSensitive(parameter, "additionalSmartMeterDailyPrice");
	/* Amber Electric's Fee - Fixed daily charge payable to Amber Electric. */
	cJSON *amberdailyprice = cJSON_GetObjectItemCaseSensitive(parameter, "amberDailyPrice");
	/* Should be total of above daily charges. */
	cJSON *totaldailyprice = cJSON_GetObjectItemCaseSensitive(parameter, "totalDailyPrice");

	/* Amber stores values as text, so cJSON's valuedouble field is empty.
	 * We manually populate it, once the object has been created */
	networkdailyprice->valuedouble = strtod(networkdailyprice->valuestring, NULL);
	basicmeterdailyprice->valuedouble = strtod(basicmeterdailyprice->valuestring, NULL);
	additionalsmartmeterdailyprice->valuedouble = strtod(additionalsmartmeterdailyprice->valuestring, NULL);
	amberdailyprice->valuedouble = strtod(amberdailyprice->valuestring, NULL);
	totaldailyprice->valuedouble = strtod(totaldailyprice->valuestring, NULL);

	printf("\r\nDaily Charges (Including GST):\r\n");
	printf("Fixed Network Charge          = %3.2lf c/day\r\n", networkdailyprice->valuedouble);
	printf("Basic Metering Charge         = %3.2lf c/day\r\n", basicmeterdailyprice->valuedouble);
	printf("Additional Smart Meter Charge = %3.2lf c/day\r\n", additionalsmartmeterdailyprice->valuedouble);
	printf("Amber Price                   = %3.2lf c/day\r\n", amberdailyprice->valuedouble);
	printf("Total Daily Price             = %3.2lf c/day\r\n", totaldailyprice->valuedouble);

	/* Network Charge - Payable to electricity distributor. May not accurately reflect
	 * Time of Day (ToD) network tariffs. */
	cJSON *networkwhprice = cJSON_GetObjectItemCaseSensitive(parameter, "networkKWHPrice");
	/* Market charges - Includes charges to AEMO and Environmental Certificate Costs. */
	cJSON *marketkwhprice = cJSON_GetObjectItemCaseSensitive(parameter, "marketKWHPrice");
	/* Carbon offset charges specific to your Amber plan. You will be on either on the default
	 * Carbon Neutral or 100% GreenPower Plan - Cost to purchase carbon credits. */
	cJSON *greenkwhprice = cJSON_GetObjectItemCaseSensitive(parameter, "greenKWHPrice");
	cJSON *carbonneutralkwhprice = cJSON_GetObjectItemCaseSensitive(parameter, "carbonNeutralKWHPrice");
	/* Should be total of above kWh charges. */
	cJSON *totalfixedkwhprice = cJSON_GetObjectItemCaseSensitive(parameter, "totalfixedKWHPrice");

	/* Amber Electric stores values as text, so cJSON's valuedouble field is empty.
	 * We manually populate it, once the object has been created. */
	networkwhprice->valuedouble = strtod(networkwhprice->valuestring, NULL);
	marketkwhprice->valuedouble = strtod(marketkwhprice->valuestring, NULL);
	greenkwhprice->valuedouble = strtod(greenkwhprice->valuestring, NULL);
	carbonneutralkwhprice->valuedouble = strtod(carbonneutralkwhprice->valuestring, NULL);
	totalfixedkwhprice->valuedouble = strtod(totalfixedkwhprice->valuestring, NULL);

	printf("\r\nPer kWh unit Charges (Including GST):\r\n");
	printf("Network Charge			= %3.3lf c/kWh\r\n", networkwhprice->valuedouble);
	printf("Market Charge			= %3.3lf c/kWh\r\n", marketkwhprice->valuedouble);
	printf("100%% Green Power Offset		= %3.3lf c/kWh\r\n", greenkwhprice->valuedouble);
	printf("Carbon Neutral Offset		= %3.3lf c/kWh\r\n", carbonneutralkwhprice->valuedouble);
	printf("Total				= %3.3lf c/kWh\r\n", totalfixedkwhprice->valuedouble);

	/* Loss Factor */
	cJSON *lossfactor = cJSON_GetObjectItemCaseSensitive(parameter, "lossFactor");
	lossfactor->valuedouble = strtod(lossfactor->valuestring, NULL);
	printf("\r\nLoss Factor = %3.3lf\r\n", lossfactor->valuedouble);

	cJSON *variablePrices = cJSON_GetObjectItemCaseSensitive(data, "variablePricesAndRenewables");

	printf("\r\n");
	//cJSON_ArrayForEach(parameter, variablePrices)
	for (int i = 45; i <= 50; i++) {

		parameter = cJSON_GetArrayItem(variablePrices, i);

		cJSON *period = cJSON_GetObjectItemCaseSensitive(parameter, "period");
		printf("Period Ending: %s ",period->valuestring);

		cJSON *price = cJSON_GetObjectItemCaseSensitive(parameter, "wholesaleKWHPrice");
		price_kwh = strtod(price->valuestring, NULL);
		printf("Spot Price = %.02f c/kWh, Total Price %.02f c/kWh\r\n",price_kwh,
			(price_kwh * lossfactor->valuedouble) + totalfixedkwhprice->valuedouble);
	}

	cJSON_Delete(NEM);
}
