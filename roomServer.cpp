/*
 * programma che mette a disposizione delle stanze di chat, ognuna su una porta diversa
 * il funzionamento del sistema  è il seguente:
 *
 * viene scelta un porta per la connessione iniziale, usata da ogni client
 * il client inserisce l'id della stanza a cui connettersi.
 * l'handshake tra le 2 parti consiste nell'invio dell'id scelto al server, questo risponderà poi al client
 * indicandogli la porta a cui connettersi (si immagini la ds usata per mantenere traccia delle porte a cui sono create le stanze)
 * ogni stanza non è altro che un thread creato dal server che ascolta su una porta stabilita
 * il thread intercetta i messaggi inviati e li inoltra a tutti i partecipanti, presi da una lista dinamica <ip, sockfd>
 *
 * IP PUBBLICO SERVER : 93.45.81.67
 *
 * Nicolas Benatti @[06, 07] / 2017
 */

/* TODO: passaggio da variabili globali ad accurato passaggio di parametri */

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/time.h>

#include <bits/stdc++.h>

using namespace std;

#define MAXBUFLEN 200           // lunghezza massima di un messaggio
#define HANDSHAKE_PORT 5000	// porta "d'ingresso"

struct threadPacket {           // pacchetto per gli entryThreads

  int roomId;
  int listeningPort;
  
};


struct roomIOhandlerPacket {
  
  int roomId;        /* id della stanza da gestire */
  int* maxFd;        /* file descriptor più recentemente aggiunto, per listen() */
  fd_set* fdSet;     /* puntatore al fd_set dei client della stanza */
  bool* syncPtr;     /* puntatore al flag che sincronizza la creazione/distruzione del thread che si attiva all'uscita dell'ultimo client */
};

struct timeval tv;

map<int, int> rooms;				    // <roomId, port>, ricerca delle porte delle stanze, in base al loro id

map<int, string>  usernames;                        // memorizza i nomi degli utenti (che il pgm identifica dal file descriptor del socket)

map<int, vector<int>> roomAttachedClients;	    // <roomId, clientFdList>, per ogni stanza, la lista dei fd dei client connessi

vector<thread> roomThreads;			    //  questi thread gestiscono l'IO delle stanze (uno e uno solo per stanza)

vector<thread> entryThreads;			   /* questi thread gestiscono l'handshake che il client richiede inizialmente.
                                                    * Eseguono un look-up dalla tabella delle stanze ed informano il client su dove connettersi
                                                    * ed aggiornano la lista dei client per quella stanza
                                                    * mi servo dei thread per permettere ovviamente connessioni simultanee
                                                    */

vector<thread> roomAccepters; 			    // svolgono il lavoro di accettazione dei client nella stanza

mutex m1;					    // sincronizza il contatore delle stanze (OBSOLETO)

//bool check = false;                               // permette la distruzione/creazione del thread all'uscita di tutti i client/connessione del 1° client


/*
 * @desc: testa un messaggio da inviare/ricevere
 * @param: il messaggio
 * @return: false se il messaggio è inutile (solo spazi)
 *          true altrimenti
 */
bool verifyMessageContent(const char* buf) {
  
  /*while(*buf++)
    if(*buf != ' ') return true;*/

  for(int i=0; buf[i]; i++)
    if ( *(buf + i) != ' ' ) return true;

  return false;
}


void* handleRoomIO (void* arg) {						 // sottopgm che verrà usato dai roomThreads
  
  fd_set listeningSockets;
  
  FD_ZERO(&listeningSockets);
  
  printf("inizio handleRoomIO \n");

  roomIOhandlerPacket* tmp = (roomIOhandlerPacket *) arg;
  int id = tmp->roomId;
  
  // per riferrirsi a maxfd usare: *(tmp->maxFd)
  
  // ciclo principale
  
  char incomingBuf[MAXBUFLEN];
  int nbytes;
  
  //printf("***[%d, %d]*** \n\n", roomSocket, connectedSocket);
  
  do {
    
    printf("attendo messaggi [%d]...\n", *(tmp->maxFd));

    // salvo l'elenco originale
    listeningSockets = *(tmp->fdSet);
    
    select (*(tmp->maxFd) + 1, &listeningSockets, NULL, NULL, NULL);
    
    for(unsigned i=0; i < roomAttachedClients[id].size(); i++) {

      printf("testo il socket %d: \n", roomAttachedClients[id][i]);
      
      if(FD_ISSET(roomAttachedClients[id][i], &listeningSockets)) {  // c'è un client con dei dati da mandare

        nbytes = recv(roomAttachedClients[id][i], incomingBuf, MAXBUFLEN, 0);

        incomingBuf[nbytes] = 0;

	string toSend(usernames[roomAttachedClients[id][i]] + ": " + incomingBuf);
	
        if(nbytes > 0) {									
          
          printf("stanza, ricevuto messaggio da %d (%s)\n\n", roomAttachedClients[id][i], usernames[roomAttachedClients[id][i]].c_str());

          if( verifyMessageContent(incomingBuf) ) {
            
            for(unsigned i=0; i < roomAttachedClients[id].size(); i++) { // mando il messaggio a tutti

              nbytes = send(roomAttachedClients[id][i], toSend.c_str(), MAXBUFLEN, 0);
              printf("MANDATO\n");
              if(nbytes == -1)
                fprintf(stderr, "errore nell'invio \n");

              /*
              nbytes = send(roomAttachedClients[id][i], usernames[id].c_str(), MAXBUFLEN, 0);
                
              if(nbytes == -1)
                  fprintf(stderr, "errore nell'invio \n");
              */
            }
          }
          else printf("trash message \n");  //DEBUG MESSAGE
        }
        else if (!nbytes) {
          
          printf("** utente %d disconnesso ** \n\n", roomAttachedClients[id][i]);

          FD_CLR (roomAttachedClients[id][i], tmp->fdSet);
          
          roomAttachedClients[id].erase(roomAttachedClients[id].begin() + i);

	  printf("client connessi alla stanza %d : %d \n", id, roomAttachedClients[id].size());

          /* se è andato via anche l'ultimo client, esco, avvertendo il roomHandler */ 
          if( ! roomAttachedClients[id].size() ) { 
            *(tmp->syncPtr) = false; 
            return (void *)1; 
          }
          
          break;
        }
        else
          fprintf(stderr, "stanza, errore in ricezione %s \n", gai_strerror(nbytes));
      }
      else  // DEBUG MESSAGE
	      printf("il socket %d non ha nulla da inviare \n", roomAttachedClients[id][i]);
    }
    
  } while(true);
  
  printf("fine handleRoomIO\n");
  
  return (void*) 0;
}


void * handleHandshake(void* info) {			     //	sottopgm che verrà usato dagli entryThreads
  
  printf("inizio handleHandshake \n");
  
  int* pc = (int *) info;
  
  int pendingClient = *pc;
  
  int choice;
  
  char buf;
  
  // ricevo id della stanza scelta 
  int nbytes = recv(pendingClient, &buf, sizeof(char), 0); 
  
  if(nbytes == -1) {
    
    fprintf(stderr, "errore nella ricezione del roomId... %s\n\n", gai_strerror(nbytes));
    close(pendingClient);
    return (void *) 1;
  }
  
  printf("ho ricevuto la richiesta di connessione alla stanza %c \n\n", buf);
  
  choice = atoi(&buf);
  
  // ricevo nome utente e lo inserisco in tabella

  /*char* userNameReceived = (char*) malloc(MAXBUFLEN * sizeof(char));
  
  nbytes = recv(pendingClient, userNameReceived, MAXBUFLEN, 0);

  usernames.insert({pendingClient, string(userNameReceived)});

  printf("ho ricevuto nome utente: (%d, %s)\n", pendingClient, usernames[pendingClient].c_str());

  free(userNameReceived);*/
  
  int portToConnect = rooms[choice];	 // look-up dalla tabella delle stanze
  
  string tmp = to_string(portToConnect);
  

  // invio al client la porta della stanza scelta
  send(pendingClient, tmp.c_str(), tmp.length(), 0);
  
  if(nbytes == -1) {
    
    fprintf(stderr, "errore nella ricezione del roomId...\n\n");
    close(pendingClient);
    return (void *) 1;
  }
  
  printf("ho inviato il client la porta a cui connettersi [%s]\n\n", tmp.c_str());
  
  printf("fine handleHandshake \n");
  
  return (void *) 0;
}


void* handleRoomConnections(void* arg) {    //sottopgm usato dai roomAccepters

  bool check = false;  // indica quando terminare (potrebbe essere sostituito dai segnali)
  
  fd_set masterSet;  // set di file descriptors che ogni stanza tiene per permettere l'inoltro a tutti i client connessi
  
  printf("inizio handleRoomConnections \n");
  
  FD_ZERO(&masterSet);
  
  struct addrinfo hints, *servinfo;
  
  threadPacket* packet = (threadPacket *) arg;
  
  int id = packet->roomId;
  int port = packet->listeningPort;
  
  memset(&hints, 0, sizeof(hints));
  
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  
  
  rooms.insert({id, port});	    // mi inserisco nella tabella globale delle stanze
  
  string t = to_string(port);
  
  //const char* ts = t.c_str();
  
  int status = getaddrinfo("localhost", t.c_str(), &hints, &servinfo);
  
  if(status) {
    
    fprintf(stderr, "errore in getaddrinfo %s \n\n", gai_strerror(status));
    
    return (void*)1;
  }
  
  int thisSocket = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  
  if(thisSocket == -1) { 

    fprintf(stderr, "errore nella creazione del socket...\n\n");
    
    return (void*)1;
  }
  
  status = bind(thisSocket, servinfo->ai_addr, servinfo->ai_addrlen);
  
  if(status == -1) {
    
    fprintf(stderr, " handleRoomConnections(): errore nel binding del socket: %s \n\n", gai_strerror(status));
    
    return (void*)1;
  }
  
  
  status = listen(thisSocket, port);   //	ascolto su quella porta
  
  if(status == -1) {
    
    fprintf(stderr, "handleRoomConnections(%d): errore in ascolto connessioni", id);
    return (void *) 1;
  }
  
  //inizio l'IO con i clients
  int connected, nbytes;
  struct sockaddr_storage connectedAddr;
  int connectedAddrLen = sizeof(connectedAddr);

  // FD_SET (thisSocket, &listeningSockets);
  
  int maxfd = thisSocket + 1;
  
  while(true) {
    
    printf("handleRoomConnections(%d): attendo connessioni... \n\n", id);

    // a questo punto il client, che aspetta da tempo, si può connettere
    connected = accept(thisSocket, (struct sockaddr *)&connectedAddr, (unsigned *)&connectedAddrLen);

    char* userNameReceived = (char*) malloc(MAXBUFLEN * sizeof(char));

    // ricevo il nome utente
    nbytes = recv(connected, userNameReceived, MAXBUFLEN, 0);

    userNameReceived[nbytes] = 0;
    
    usernames.insert({connected, string(userNameReceived)});
    
    printf("handleRoomConnections() : ho ricevuto nome utente: (%d, %s)\n", connected, usernames[connected].c_str());
    
    free(userNameReceived);
    
    FD_SET(connected, &masterSet);  // inserimento nuovo sockfd
    
    maxfd = max(connected + 1, maxfd);
    
    printf("handleRoomConnections(%d): accettata richiesta \n", id);
    
    printf("***[%d, %d]***\n\n", thisSocket, connected);
    
    roomAttachedClients[id].push_back(connected);
    
    printf("**inserisco il socket %d nella tabella** n\n", connected);

    // preparo il pacchetto da passare all' IOhandler
    roomIOhandlerPacket pkg;
    
    pkg.roomId = id;
    pkg.maxFd = &maxfd; 
    pkg.fdSet = &masterSet;
    pkg.syncPtr = &check;
    
    /* quando si connette un nuovo client, o un client si disconnette, mando un avviso al 1o client, che manderà un messaggio 
     * inutile per "sbloccare" la chat e permettere all'ultimo di scrivere fin da subito
     * lato client, intanto, il recThread stà già girando ed intercetterà tale messaggio
     */
    char adviseBuf[] = {"^^&/£$%?^^"};
    send(roomAttachedClients[id][0], adviseBuf, strlen(adviseBuf), 0);
    
    /* quando entra il 1° client  (e poi non più), inizio a gestire l'I/O */
    if(!check && roomAttachedClients[id].size() == 1) {
      check = true;
      try {
	roomThreads.push_back(thread(handleRoomIO, &pkg));
      }
      catch (exception& exc) {

	cerr << "ERRORE: "<< exc.what() <<"\n";
      }
    }
  }
  
  printf("fine handleRoomConnections \n");
  
  return (void *) 0;
}


int main() {
  
  struct addrinfo hints, *servinfo;
  
  memset(&hints, 0, sizeof(hints));
  
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  
  // creo le stanze (di prova)
  threadPacket tp = {1, 4000};
  rooms.insert({tp.roomId, tp.listeningPort});

  try {
    roomAccepters.push_back(thread(handleRoomConnections, &tp));
  }
  catch(exception& exc) {

    cerr<< "ERRORE: "<< exc.what() <<"\n";
  }
  
  threadPacket tp1 = {4, 6000};
  rooms.insert({tp.roomId, tp.listeningPort});

  try {
    roomAccepters.push_back(thread(handleRoomConnections, &tp1));
  }
  catch(exception& exc) {

    cerr<< "ERRORE: "<< exc.what() <<"\n";
  }
    
  
  int status = getaddrinfo("localhost", "5000", &hints, &servinfo);
  
  if(status) {
    
    fprintf(stderr, "errore in getaddrinfo %s \n\n", gai_strerror(status));
    
    return 1;
  }
  
  int serverSocket = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  
  if(serverSocket == -1) {
    
    fprintf(stderr, "errore nella creazione del main socket ...\n\n");
    
    return 1;
  }
  
  status = bind(serverSocket, servinfo->ai_addr, servinfo->ai_addrlen);
  
  if(status == -1) {
    
    fprintf(stderr, "errore nel binding del main socket...\n\n");
    
    return 1;
  }
  
  // ascolto il canale iniziale
  
  printf("attendo connessioni...\n\n");
  
  status = listen(serverSocket, HANDSHAKE_PORT);
  
  if(status == -1) {
    
    fprintf(stderr, "errore nel mettermi in ascolto..\n\n");
    
    return 1;
  }
  
  
  struct sockaddr_storage tmpClientInfo;
  int tmpAddrLen = sizeof(tmpClientInfo);
  
  int tmpSocket;
  int yes = 1;   // per permettere il riutilizzo della porta anche subito dopo la chiusura del socket

  if(setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))) {
    fprintf(stderr, "errore nel liberare la porta \n\n");
    return 1;
  }
  
  do {
    
    tmpSocket = accept(serverSocket, (struct sockaddr* )&tmpClientInfo, (unsigned *)&tmpAddrLen);
    printf("utente connesso al main channel\n\n");
    // illustro all'utente le stanze disponibili
    
    int dim = rooms.size();

    /* mostro al client la situazione attuale */

    string info ("ci sono ");

    ((info += to_string(dim)) += " stanze: \n");

    //send(tmpSocket, info.c_str(), strlen(info.c_str()), 0);

    //info.clear();

    //send(tmpSocket, (to_string(dim)).c_str(), strlen((to_string(dim)).c_str()), 0);
    
    for(map<int, int>::iterator it = rooms.begin(); it != rooms.end(); it++) {
      
      (((((info += "\n    stanza #") += to_string(it->first)) += " : utenti connessi ") += to_string(roomAttachedClients[it->first].size())) += "\n\n");
      //send(tmpSocket, info.c_str(), strlen(info.c_str()), 0);
      //printf("ho mandato \n");
      //info.clear();
    }

    send(tmpSocket, info.c_str(), strlen(info.c_str()), 0);
    printf("ho mandato \n");
    
    // il thread gestirà la scelta dell'utente, mentre questo ciclo accoglierà i nuovi utenti
    try {
      entryThreads.push_back(thread(handleHandshake, &tmpSocket));
    }
    catch(exception& exc) {

      cerr<< "ERRORE: "<< exc.what() << "\n";
    }
  } while(true);

  close(serverSocket);
  
  return 0;
}
