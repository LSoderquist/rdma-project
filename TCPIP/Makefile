client:
	g++ bank_client.cpp -o bank_client

mvcc:
	g++ -c bank_common.cpp -o bank_common.o -I$(shell pg_config --includedir)
	g++ -c bank_MVCC.cpp -o bank_MVCC.o -I$(shell pwd)
	g++ bank_MVCC.o bank_common.o -o bank_MVCC -lpthread -lpq

tl2:
	g++ -c bank_common.cpp -o bank_common.o -I$(shell pg_config --includedir)
	g++ -c bank_TL2.cpp -o bank_TL2.o -I$(shell pwd)
	g++ bank_TL2.o bank_common.o -o bank_TL2 -lpthread -lpq

2pl:
	g++ -c bank_common.cpp -o bank_common.o -I$(shell pg_config --includedir)
	g++ -c bank_2PL.cpp -o bank_2PL.o -I$(shell pwd)
	g++ bank_2PL.o bank_common.o -o bank_2PL -lpthread -lpq

sgl:
	g++ -c bank_common.cpp -o bank_common.o -I$(shell pg_config --includedir)
	g++ -c bank_SGL.cpp -o bank_SGL.o -I$(shell pwd)
	g++ bank_SGL.o bank_common.o -o bank_SGL -lpthread -lpq