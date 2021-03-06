amber : main.c http.c parser.c
	cc -o amber main.c http.c parser.c -lcurl -lcjson

clean :
	rm amber

