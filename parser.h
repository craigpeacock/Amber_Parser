
#define SOURCE_5MIN 	1
#define SOURCE_30MIN 	2

struct PRICES {
	struct tm currentNEMtime;
	char * networkprovider;
	unsigned int postcode;
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
	struct tm latestperiod;
	struct tm periodending;
	unsigned int periodsource;
	double currentwholesaleKWHPrice;
	double operationaldemand;
	double renewablespercentage;
};

void parse_amber_json(char *ptr, struct PRICES *prices);

