#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>

#include <bits/stdc++.h>

using namespace std;

#define MAXBUFLEN 140    // massima lunghezza di un messaggio da inviare/ricevere
char INIT_BUF = ' ';     // buffer di "sblocco chat" per il 1° client che si connette alla stanza

/*
 * @desc: riconosce il messaggio di sblocc mandato dal server
 * @param: il messaggio
 * @return true se riconosciuto, false altrimenti
 * @adv: è difatto una chiamata ad una funzione built-in, ma per leggibilità cambio nome
 */
bool detectAdviseMessage(const char* buf) {

  return strcmp(buf, "^^&/£$%?^^");
}

void* reciever (void* arg) {  // sottopgm usato dal thread ricettore
  
  int nbytes;
  
  int* tmp = (int *) arg;

  int thisSocket = *tmp;

  // printf("thread: %d \n\n", thisSocket);
  
  char incomingBuf[MAXBUFLEN];
  
  while (true) {

    nbytes = recv (thisSocket, incomingBuf, MAXBUFLEN, 0); 

    incomingBuf[nbytes] = 0;
    
    if(nbytes != -1) {
      if (detectAdviseMessage(incomingBuf))  
	printf("\nmessaggio: %s \n", incomingBuf);
      else {
	//printf("[thread: e' arrivato il messaggio di sblocco] \n");
	send(thisSocket, &INIT_BUF, sizeof(INIT_BUF), 0);  // "sblocco" la chat
      }
    }
    else {
      fprintf(stderr, "errore in ricezione %s \n\n", gai_strerror(nbytes));
      
      return (void *) 1;
    }
  }

  return (void *) 0;
}

int main() {

	int status, nbytes;
	struct addrinfo hints;
	struct addrinfo* servinfo;
	bool check = false;
	
	int numRooms;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if((status = getaddrinfo("localhost", "5000", &hints, &servinfo)) != 0) {

		fprintf(stderr, "errore in getaddrinfo: %s ...\n\n", gai_strerror(status));
		exit(1);
	}

	int clientSocket = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

	if( clientSocket == -1 ) {	

		fprintf(stderr, "errore nella creazione del socket \n");

		return 1;
	}

	status = connect(clientSocket, servinfo->ai_addr, servinfo->ai_addrlen);

	if(status == -1) {

		fprintf(stderr, "errore nella connessione al server %s ...\n\n", gai_strerror(status));

		return 1;
	}

	printf("\nsei connesso!\n\n");
	
	/* ricevo i dati sulla situazione attuale */
	
	char infoBuf[MAXBUFLEN];
	
	nbytes = recv (clientSocket, infoBuf, MAXBUFLEN ,0);

	if(nbytes == -1) {
	  fprintf(stderr, "errore in ricezione : %s \n\n", gai_strerror(nbytes));
	  return 1;
	}

	infoBuf[nbytes] = 0;
	
	printf("%s", infoBuf);
	
	/*nbytes = recv(clientSocket, infoBuf, MAXBUFLEN, 0);

	printf("ricevuto numRooms \n");
	
	if(nbytes == -1) {
	  fprintf(stderr, "errore in ricezione : %s \n\n", gai_strerror(nbytes));
	  return 1;
	}

	infoBuf[nbytes] = 0;
	
	numRooms = atoi(infoBuf);
	
	printf("numRooms : %d \n", numRooms);*/
	
	/*for(int i = 0; i < numRooms; i++) {
	  
	  printf("sono nel for \n");
	  
	  nbytes = recv(clientSocket, infoBuf, MAXBUFLEN, 0);
	  
	  if(nbytes == -1) {
	  fprintf(stderr, "errore in ricezione : %s \n\n", gai_strerror(nbytes));
	  return 1;
	  }
	  
	  infoBuf[nbytes] = 0;
	  
	  //printf("%s", infoBuf);
	  }*/
	
	//dico al server dove voglio andare
	int choice;
	
	printf("digita l'id della stanza a cui connetterti: ");
	scanf("%d", &choice);
	
	string tmp = to_string(choice);
	
	nbytes = send(clientSocket, tmp.c_str(), tmp.size(), 0);
	
	if(nbytes == -1) {
	  
	  fprintf(stderr, "errore in ricezione...\n\n");
	  
	  return EXIT_FAILURE;
	}

	// ricevo a che porta connettermi

	char roomPort[10];
	
	nbytes = recv(clientSocket, roomPort, 10, 0);
	roomPort[nbytes] = 0;
	
	printf("devi connetterti sulla porta %s \n\n", roomPort);

	close(clientSocket);
	
	// CONNESSIONE ALLA STANZA (nel server, l'entryThread ha appena finito il suo lavoro):

	if((status = getaddrinfo("localhost", roomPort, &hints, &servinfo)) != 0) {
	  
	  fprintf(stderr, "errore in getaddrinfo: %s ...\n\n", gai_strerror(status));
	  exit(1);
	}
	
	clientSocket = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	if( clientSocket == -1 ) {
	  
	  fprintf(stderr, "errore nella creazione del socket \n");
	  
	  return 1;
	}
	
	status = connect(clientSocket, (sockaddr *)servinfo->ai_addr, servinfo->ai_addrlen);
	if(status == -1) {
	  
	  fprintf(stderr, "errore nella connessione al server %s ...\n\n", gai_strerror(status));
	  
	  return 1;
	}
	
	// ciclo principale
	
	printf ("sei connesso alla stanza %d ! \n\n", choice);
	
	thread recThread (reciever, &clientSocket);  // thread ricettore di messaggi;
	
	char sendBuf[MAXBUFLEN];
	char incoming[MAXBUFLEN];
	
	do {
	  
	  printf("scrivi qualcosa (<ctrl+c> per uscire): ");
	  //fgets(sendBuf, MAXBUFLEN, stdin);
	  //scanf("%s", sendBuf);
	  cin.getline(sendBuf, MAXBUFLEN);
	  
	  nbytes = send(clientSocket, sendBuf, MAXBUFLEN, 0);
	  
	  if(nbytes == -1) {
	    
	    fprintf(stderr, "errore in invio...\n\n");
	    
	    return EXIT_FAILURE;
	  }
	  
	  if(strlen(sendBuf) == 1 && *sendBuf == 'q') check = 1;
	  
	} while(!check);
	
	recThread.join();
	
	return 0;
}
