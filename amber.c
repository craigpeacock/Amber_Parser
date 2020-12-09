
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

	printf("Current Time: %s\r\n",currentNEMtime->valuestring);
	printf("Postcode: %s\r\n",postcode->valuestring);
	printf("Network Provider: %s\r\n",networkProvider->valuestring);

	cJSON *staticPrices = cJSON_GetObjectItemCaseSensitive(data, "staticPrices");
	//cJSON_ArrayForEach(parameter, staticPrices)
	//{
	//	cJSON *totaldailyprice = cJSON_GetObjectItemCaseSensitive(parameter, "totalDailyPrice");
	//	printf("Total Daily Price = %.02f c/kWh\r\n", strtod(totaldailyprice->valuestring, NULL));
	//}

	cJSON *variablePrices = cJSON_GetObjectItemCaseSensitive(data, "variablePricesAndRenewables");
	cJSON_ArrayForEach(parameter, variablePrices)
	{
		cJSON *period = cJSON_GetObjectItemCaseSensitive(parameter, "period");
		printf("Period: %s ",period->valuestring);

		cJSON *price = cJSON_GetObjectItemCaseSensitive(parameter, "wholesaleKWHPrice");
		//printf("Price: %s \r\n",price->valuestring);

		price_kwh = strtod(price->valuestring, NULL);
		printf("Price = %.02f c/kWh\r\n",price_kwh);
	}

	cJSON_Delete(NEM);
}
