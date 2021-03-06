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

void parse_amber_json(char *ptr, struct PRICES *prices)
{
	double price_kwh;
	double totalfixed_kwh;

	cJSON *NEM;
	cJSON *parameter;

	NEM = cJSON_Parse(ptr);
	if (NEM == NULL) {
		printf("Unable to parse JSON file\r\n");
		return;
	}

	cJSON *data = cJSON_GetObjectItemCaseSensitive(NEM, "data");
	if (data == NULL) {
		printf("Cannot find data object\r\n");
		cJSON_Delete(NEM);
		return;
	}

	cJSON *currentNEMtime = cJSON_GetObjectItemCaseSensitive(data, "currentNEMtime");
	if (currentNEMtime != NULL) {
		/* String in the format of 2020-12-19T15:10:00 */
		if (strptime((char *)currentNEMtime->valuestring, "%Y-%m-%dT%H:%M:%S", &prices->currentNEMtime) == NULL)
			printf("Unable to parse NEM time\r\n");
		//printf("Current NEM time : %s ", currentNEMtime->valuestring);
	}

	cJSON *postcode = cJSON_GetObjectItemCaseSensitive(data, "postcode");
	if (postcode != NULL) prices->postcode = atoi(postcode->valuestring);

	cJSON *networkProvider = cJSON_GetObjectItemCaseSensitive(data, "networkProvider");
	if (networkProvider != NULL)
		if (prices->networkprovider != NULL)
			strcpy(prices->networkprovider, networkProvider->valuestring);

	/* Amber has four staticPrices arrays labelled E1, E2, B1 and B1PFIT.
	 * Array 0 (E1) has the data we are after. */
	cJSON *staticPrices = cJSON_GetObjectItemCaseSensitive(data, "staticPrices");
	if (staticPrices != NULL) {
		parameter = cJSON_GetArrayItem(staticPrices, 0);
		if (parameter != NULL) {
			/* Network Daily Price - Fixed daily charge payable to electricity distributor. */
			cJSON *networkdailyprice = cJSON_GetObjectItemCaseSensitive(parameter, "networkDailyPrice");
			if (networkdailyprice != NULL)
				prices->daily.networkdailyprice = strtod(networkdailyprice->valuestring, NULL);
			else
				printf("Can't find %s\r\n", networkdailyprice->string);

			/* Metering Charges - Fixed daily charge payable to metering provider. */
			cJSON *basicmeterdailyprice = cJSON_GetObjectItemCaseSensitive(parameter, "basicMeterDailyPrice");
			if (basicmeterdailyprice != NULL)
				prices->daily.basicmeterdailyprice = strtod(basicmeterdailyprice->valuestring, NULL);
			else
				printf("Can't find %s\r\n", basicmeterdailyprice->string);

			cJSON *additionalsmartmeterdailyprice = cJSON_GetObjectItemCaseSensitive(parameter, "additionalSmartMeterDailyPrice");
			if (additionalsmartmeterdailyprice != NULL)
				prices->daily.additionalsmartmeterdailyprice = strtod(additionalsmartmeterdailyprice->valuestring, NULL);
			else
				printf("Can't find %s\r\n", additionalsmartmeterdailyprice->string);

			/* Amber Electric's Fee - Fixed daily charge payable to Amber Electric. */
			cJSON *amberdailyprice = cJSON_GetObjectItemCaseSensitive(parameter, "amberDailyPrice");
			if (amberdailyprice != NULL)
				prices->daily.amberdailyprice = strtod(amberdailyprice->valuestring, NULL);
			else
				printf("Can't find %s\r\n", amberdailyprice->string);

			/* Should be total of above daily charges. */
			cJSON *totaldailyprice = cJSON_GetObjectItemCaseSensitive(parameter, "totalDailyPrice");
			if (totaldailyprice != NULL)
				prices->daily.totaldailyprice = strtod(totaldailyprice->valuestring, NULL);
			else
				printf("Can't find %s\r\n", totaldailyprice->string);

			/* Network Charge - Payable to electricity distributor. May not accurately reflect
			 * Time of Use (ToU) network tariffs. */
			cJSON *networkkwhprice = cJSON_GetObjectItemCaseSensitive(parameter, "networkKWHPrice");
			if (networkkwhprice != NULL)
				prices->unit.networkkwhprice = strtod(networkkwhprice->valuestring, NULL);
			else
				printf("Can't find %s\r\n", networkkwhprice->string);

			/* Market charges - Includes charges to AEMO and Environmental Certificate Costs. */
			cJSON *marketkwhprice = cJSON_GetObjectItemCaseSensitive(parameter, "marketKWHPrice");
			if (marketkwhprice != NULL)
				prices->unit.marketkwhprice = strtod(marketkwhprice->valuestring, NULL);
			else
				printf("Can't find %s\r\n", marketkwhprice->string);

			/* Carbon offset charges specific to your Amber plan. You will be on either on the default
			 * Carbon Neutral or 100% GreenPower Plan - Cost to purchase carbon credits. */
			cJSON *greenkwhprice = cJSON_GetObjectItemCaseSensitive(parameter, "greenKWHPrice");
			if (greenkwhprice != NULL)
				prices->unit.greenkwhprice = strtod(greenkwhprice->valuestring, NULL);
			else
				printf("Can't find %s\r\n", greenkwhprice->string);

			cJSON *carbonneutralkwhprice = cJSON_GetObjectItemCaseSensitive(parameter, "carbonNeutralKWHPrice");
			if (carbonneutralkwhprice != NULL)
				prices->unit.carbonneutralkwhprice = strtod(carbonneutralkwhprice->valuestring, NULL);
			else
				printf("Can't find %s\r\n", carbonneutralkwhprice->string);

			/* Should be total of above kWh charges. */
			cJSON *totalfixedkwhprice = cJSON_GetObjectItemCaseSensitive(parameter, "totalfixedKWHPrice");
			if (totalfixedkwhprice != NULL)
				prices->unit.totalfixedkwhprice = strtod(totalfixedkwhprice->valuestring, NULL);
			else
				printf("Can't find %s\r\n", totalfixedkwhprice->string);

			/* Loss Factor */
			cJSON *lossfactor = cJSON_GetObjectItemCaseSensitive(parameter, "lossFactor");
			if (lossfactor != NULL)
				prices->unit.lossfactor = strtod(lossfactor->valuestring, NULL);
			else
				printf("Can't find %s\r\n", lossfactor->string);

		}
	}

	cJSON *variablePrices = cJSON_GetObjectItemCaseSensitive(data, "variablePricesAndRenewables");
	if (cJSON_IsArray(variablePrices))
	{
		//printf("Items in array %d\n",cJSON_GetArraySize(variablePrices));

		cJSON_ArrayForEach(parameter, variablePrices)
		{
			cJSON *periodType = cJSON_GetObjectItemCaseSensitive(parameter, "periodType");
			//cJSON *periodSource = cJSON_GetObjectItemCaseSensitive(parameter, "periodSource");

			// Keep iterating array until we pass the last of the ACTUAL results.
			if (strcmp(periodType->valuestring, "ACTUAL") != 0) break;
		}
	}

	/* Period type should always be actual. We don't do anything with forcasts */
	//cJSON *periodType = cJSON_GetObjectItemCaseSensitive(parameter->prev, "periodType");
	//printf("Period Type: %s ",periodType->valuestring);

	/* Get period ending. This is the 30 minute settlement period */
	cJSON *period = cJSON_GetObjectItemCaseSensitive(parameter->prev, "period");
	if (period != NULL) {
		/* String in the format of 2020-12-19T15:10:00 */
		if (strptime((char *)period->valuestring, "%Y-%m-%dT%H:%M:%S", &prices->periodending) == NULL)
			printf("Unable to parse settlement time\r\n");
		//printf("Period Ending    : %s ",period->valuestring);
	}

	/* Get latest period. This is the 5 minute settlement period. It is not present when Period Source = 30MIN */
	cJSON *latestPeriod = cJSON_GetObjectItemCaseSensitive(parameter->prev, "latestPeriod");
	if (latestPeriod != NULL) {
		/* String in the format of 2020-12-19T15:10:00 */
		if (strptime((char *)latestPeriod->valuestring, "%Y-%m-%dT%H:%M:%S", &prices->latestperiod) == NULL)
			printf("Unable to parse settlement time\r\n");
		//printf("Latest Period : %s ",latestPeriod->valuestring);
	}

	cJSON *periodSource = cJSON_GetObjectItemCaseSensitive(parameter->prev, "periodSource");
	if (periodSource != NULL) {
		//printf("Period Source: %s \r\n",periodSource->valuestring);
		if (strcmp(periodSource->valuestring, "30MIN") == 0) {
			prices->periodsource = SOURCE_30MIN;
			memcpy(&prices->latestperiod, &prices->periodending, sizeof(prices->periodending));
		} else if (strcmp(periodSource->valuestring, "5MIN") == 0) {
			prices->periodsource = SOURCE_5MIN;
		}
	}

	/* Get Wholesale price (cents per kWh) */
	cJSON *price = cJSON_GetObjectItemCaseSensitive(parameter->prev, "wholesaleKWHPrice");
	if (price != NULL) {
		prices->currentwholesaleKWHPrice = strtod(price->valuestring, NULL);
		//printf("Wholesale kWh Price %s\r\n", price->valuestring);
	}

	cJSON *renewables = cJSON_GetObjectItemCaseSensitive(parameter->prev, "renewablesPercentage");
	if (renewables != NULL)
		prices->renewablespercentage = strtod(renewables->valuestring, NULL);

	cJSON *operationaldemand = cJSON_GetObjectItemCaseSensitive(parameter->prev, "operationalDemand");
	if (operationaldemand!= NULL )
		prices->operationaldemand = strtod(operationaldemand->valuestring, NULL);

	cJSON_Delete(NEM);
}

