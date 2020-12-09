amber : amber.o 
	cc -o amber amber.o -lcurl -lcjson

amber.o : amber.c
	cc -c amber.c

clean :
	rm amber amber.o 
        
        
