#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <ncurses.h>

#include <bits/stdc++.h>

///TODO: migliorare la qualità del codice, piccolo "text editor" che permette muovere il cursore sul messaggio e cancellare il testo liberamente, login temporaneo con username

using namespace std;

#define MAXBUFLEN 140                                                      // massima lunghezza di un messaggio da inviare/ricevere

const char INIT_BUF = ' ';                                                 // buffer di "sblocco chat" per il 1° client che si connette alla stanza

#define WRITE_BUF_POSITION 0.95F                                           // percentuale della finestra alla quale si colloca il campo di scrittura del messaggio

#define MESSAGE_BAR_INFO "scrivi qualcosa (<ctrl+c> per uscire): "         // messaggio di informazione prima dello spazio per il messaggio (informativo) 

struct recieverPacket {
  
  int* terminalWindowOffset;     // offset incrementale dei messaggi, asse y (in numero di colonne)
  int sockfd;                    // descrittore del socket da controllare
  int termRows;                  // no. di righe del terminale
  int xCarry;                    // offset sull'asse x dove riposizionare il cursore per permettere la scrittura dei messaggi
};


struct textEditorPacket {

  const int cursorStartPos;      // posizione iniziale del cursore
  int termRows;                  // righe della finestra
  bool* terminator;              // puntatore ad un flag nel main, che indica al thread di uscire
  //int cursorLimit;               // fine del messaggio, non ci si può spostare oltre
};


/*
 * @desc:   riconosce il messaggio di sblocco mandato dal server
 * @param:  il messaggio
 * @return: true se riconosciuto, false altrimenti
 */
bool detectAdviseMessage(const char* buf) {
  
  return !strcmp(buf, "^^&/£$%?^^");
}

inline bool isTrashMessage(const char* buf) {

  return !(*buf);
}

/*
 * @desc:   pulisce una riga del terminale
 * @param:  coordinate (x, y), lunghezza del messaggio
 */
void clearMessageBar(int xPos, int yPos, int messageLength) {
  
  char* mask = (char *)malloc(messageLength * sizeof(char));
  mask[messageLength] = 0;
  for(int i=0; i < messageLength; i++)
    mask[i] = ' ';
  
  mvprintw(/*(WRITE_BUF_POSITION * termRows)*/ yPos, xPos, mask);
}

/*
 * @desc:   si occupa di ricevere i messaggi inviati dal server
 * @param:  puntatore generico, convertito in <struct recieverPacket*>
 * @return: controllo al SO (pthread_exit())
 * TODO: - dispMessages e offset locali invece che globali/passati da main()
 */
void* reciever (void* arg) {  // sottopgm usato dal thread ricettore

  deque<string> dispMessages;         /* contiene la "finestra" di messaggi attualmente in vista sullo schermo, per permettere lo scrolling
                                         dato che agisce come una "sliding window", uso una deque che permette push e pop da entrambe le estremità in O(1) */
                                       
  
  int nbytes;
  
  recieverPacket* tmp = (recieverPacket *) arg;

  int thisSocket = tmp->sockfd;

  // per riferirsi all'offset : *(tmp->terminalWindowOffset);
  
  char incomingBuf[MAXBUFLEN];
  char username[MAXBUFLEN] = {"testEnvironment"};

  const int maxDisplayedMessages = (tmp->termRows / 2) - 2;

  int tmpDim;   // lunghezza del messaggio eliminato a causa scorrimento, ma da cancellare dallo schermo
  
  while (1) {

    refresh();

    /*
    nbytes = recv(thisSocket, username, MAXBUFLEN, 0);

    if (nbytes == -1)
      fprintf(stderr, "errore in ricezione username \n");
    */

    nbytes = recv (thisSocket, incomingBuf, MAXBUFLEN, 0); 
    //printf("ricevuto messaggio\n");
    if(nbytes == -1)
      fprintf(stderr, "errore in ricezione messaggio \n");

    incomingBuf[nbytes] = 0;

    if(isTrashMessage(incomingBuf))
      continue;
    
    if(nbytes != -1) {
      
      if (!detectAdviseMessage(incomingBuf)) {

        *(tmp->terminalWindowOffset) = 0;
        
        //mvprintw(*(tmp->terminalWindowOffset), 2, "\nmessaggio: %s \n", incomingBuf);

        if(dispMessages.size() == maxDisplayedMessages) {
	  
          tmpDim = dispMessages[0].length() + 11;  // aggiungo la dimensione di "messaggio : "
          dispMessages.pop_front();
        }

        dispMessages.push_back(string(incomingBuf));
        
        for(int i=0; i < dispMessages.size(); i++) {

	  if(!i)
	    clearMessageBar(2, tmpDim, tmp->termRows);
	  else
	    clearMessageBar(2, *(tmp->terminalWindowOffset), strlen(dispMessages[i].c_str()));

	  mvprintw(*(tmp->terminalWindowOffset), 2, "%s\n", dispMessages[i].c_str());
	    
          *(tmp->terminalWindowOffset) += 2;
        }
      }
      else {

        //printf("[thread: e' arrivato il messaggio di sblocco] \n");
        send(thisSocket, &INIT_BUF, sizeof(INIT_BUF), 0);  // "sblocco" la chat
      }
    }
    else {

      fprintf(stderr, "errore in ricezione %s \n\n", gai_strerror(nbytes));
      
      return (void *) 1;
    }

    refresh();
    
    /*  stampa singola 
     *(tmp->terminalWindowOffset) += 2;
     */
     move(WRITE_BUF_POSITION * (tmp->termRows), tmp->xCarry);
  }
  
  return (void *) 0;
}


/*
 * @desc: permette di shiftare i caratteri indietro di un posto, partendo da x
 * @param: coordinate di inizio shift, no. di caratteri da shiftare
 * @return: none
 */
void slideChars(int y, int x, int dim) {

  char nextCar;

  for(int i=0; i < dim-1; i++) {
    
    nextCar = mvwgetch(stdscr, y, x + 1);
    mvaddch(y, x, nextCar);

    x++;
  }

  return;
}

int main() {

  int termRows, termCols;
  
  char username[MAXBUFLEN];

  WINDOW* term;
  
  initscr();
  getmaxyx(stdscr, termRows, termCols);
  clear();
  
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
    exit(EXIT_FAILURE);
  }
  
  int clientSocket = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  
  if( clientSocket == -1 ) {    
    
    fprintf(stderr, "errore nella creazione del socket \n");
    endwin();
    return 1;
  }
  
  status = connect(clientSocket, servinfo->ai_addr, servinfo->ai_addrlen);
  
  if(status == -1) {
    
    fprintf(stderr, "errore nella connessione al server %s ...\n\n", gai_strerror(status));
    endwin();
    return 1;
  }
  
  mvprintw((int)termRows/2, (int)termCols/2, "\nsei connesso!\n\n");
  
  clear();
  
  /* ricevo i dati sulla situazione attuale */
  
  char infoBuf[MAXBUFLEN];
  
  nbytes = recv (clientSocket, infoBuf, MAXBUFLEN ,0);
  
  if(nbytes == -1) {
    fprintf(stderr, "errore in ricezione : %s \n\n", gai_strerror(nbytes));
    endwin();
    return 1;
  }
  
  attron(A_BOLD);
  infoBuf[nbytes] = 0;
  mvprintw((int)(0.05*termRows), 2, "%s", infoBuf);
  
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
  
  mvprintw((int)(0.4 * termRows), 2, "inserisci username: ");
  mvscanw((int)(0.4 * termRows), strlen("inserisci username: ") + 3, "%s", username);
  mvprintw((int)(0.5 * termRows), 2, "digita l'id della stanza a cui connetterti: ");
  mvscanw((int)(0.5 * termRows), strlen("digita l'id della stanza a cui connetterti: ")+3, "%d", &choice);
  
  string tmp = to_string(choice);
  
  // invio id della stanza scelta 
  nbytes = send(clientSocket, tmp.c_str(), tmp.size(), 0);
  
  if(nbytes == -1) {
    
    fprintf(stderr, "errore in ricezione...\n\n");
    endwin();
    return EXIT_FAILURE;
  }

  // invio nome utente
  /*nbytes = send(clientSocket, username, strlen(username), 0);

  if(nbytes == -1) {
    
    fprintf(stderr, "errore in ricezione...\n\n");
    endwin();
    return EXIT_FAILURE;
    }*/
  
  // ricevo il numero di porta a cui connettermi
  
  char roomPort[10];
  
  nbytes = recv(clientSocket, roomPort, 10, 0);
  roomPort[nbytes] = 0;
  
  //mvprintw((int)(0.5*termRows), 2, "devi connetterti sulla porta %s", roomPort);
  
  close(clientSocket);
  
  // CONNESSIONE ALLA STANZA (nel server, l'entryThread ha appena finito il suo lavoro):
  
  if((status = getaddrinfo("localhost", roomPort, &hints, &servinfo)) != 0) {
    
    fprintf(stderr, "errore in getaddrinfo: %s ...\n\n", gai_strerror(status));
    endwin();
    return 1;
  }
  
  clientSocket = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  
  if( clientSocket == -1 ) {
    
    fprintf(stderr, "errore nella creazione del socket \n");
    endwin();
    return EXIT_FAILURE;
  }

  // il client attende qui, nel frattempo il server chiude l'entryThread e apre il roomAccepter
  status = connect(clientSocket, (sockaddr *)servinfo->ai_addr, servinfo->ai_addrlen);
  
  if(status == -1) {
    
    fprintf(stderr, "errore nella connessione al server %s ...\n\n", gai_strerror(status));
    
    return EXIT_FAILURE;
  }

  // invio nome utente
  nbytes = send(clientSocket, username, strlen(username), 0);
  
  if(nbytes == -1) {
    fprintf(stderr, "error in sending user data : %s \n\n", gai_strerror(nbytes));
    return EXIT_FAILURE;
  }
		
  mvprintw ((int)(0.6*termRows), 2, "sei connesso alla stanza %d!", choice);
  mvprintw((int)(WRITE_BUF_POSITION * termRows), 2, MESSAGE_BAR_INFO);
  refresh();
  attroff(A_BOLD);
  
  int recOffset = 0;       // offset per la stampa dei messaggi ricevuti (usato da recThread)
  
  recieverPacket pkg = {&recOffset, clientSocket, termRows, strlen("scrivi qualcosa (<ctrl+c> per uscire): ")+3};
  
  thread recThread (reciever, &pkg);  // thread ricettore di messaggi;
  
  char sendBuf[MAXBUFLEN];
  char incoming[MAXBUFLEN];
  
  clear();
  
  // === MAIN LOOP ===
  
  int messageBarInfoLength = strlen(MESSAGE_BAR_INFO);     // lunghezza del messaggio informativo
  int startCursorPos = messageBarInfoLength + 3;           // posizione di partenza del cursore

  char c;
  textEditorPacket tePkg = {startCursorPos, termRows};
  int mouseX, mouseY, tempMessageEnd = 0;
  
  do {
    
    refresh();
        
    mvprintw((int)(WRITE_BUF_POSITION * termRows), 2, MESSAGE_BAR_INFO);
    mvgetstr((int)(WRITE_BUF_POSITION * termRows), startCursorPos, sendBuf);

    //lavoro di textEditor() nel main, è sperimentale
    ///TODO: debug dello snippet, fino a FINE
    //while(c != 13) {
	
      //getyx(stdscr, mouseY, mouseX);
      
      //c = getch();
      
      //if(c == '\033') {     // pressione di un tasto freccia
				
				//getch();
				
				//switch(getch()) {
						
				//case 'C':
					////if(mouseX < tempMessageEnd) {TODO: risolvere questo caso, non entra nell'if() 
					//move(termRows * WRITE_BUF_POSITION, ++mouseX);
						////}
					//break;
					
				//case 'D':
					//if(mouseX > startCursorPos) {
						//move(termRows * WRITE_BUF_POSITION, --mouseX);
					//}
					//break;
					
				//default:
					//break;
				//}

      //} else if(c == 127) {   // pressione del tasto delete (!= canc)

				//if(mouseX >= startCursorPos) {
					
					////sendBuf.erase(mouseX - startCursorPos, 1);
					//// mvaddch(mouseY, mouseX-1, ' ');
					//slideChars((int)(termRows * WRITE_BUF_POSITION), mouseX-1, tempMessageEnd - mouseX);
					//move((int)(termRows * WRITE_BUF_POSITION), --mouseX);
					//tempMessageEnd--;
				//}

      //} else {            // digitazione del messaggio

	
				//// if(sendBuf.empty()) 
				////   sendBuf.push_back( c );
				//// else 
				////   sendBuf.insert(mouseX - startCursorPos, &c);

				//mvaddch(mouseY, mouseX, c);
				//tempMessageEnd++;
      //}

      //// clearMessageBar(startCursorPos, termRows * WRITE_BUF_POSITION, sendBuf.length());
      //// mvprintw((int)( termRows * WRITE_BUF_POSITION ), startCursorPos, sendBuf);

      /*mvaddch(5, 5, (char)tempMessageEnd);
				//move (WRITE_BUF_POSITION * termRows, mouseX);*/
    //}
    /* FINE */
    
    //mvaddch(10, 10, 'v');
    //cout<< sendBuf.c_str() <<"\n";
    
    nbytes = send(clientSocket, sendBuf, MAXBUFLEN, 0);
    
    clearMessageBar(startCursorPos, termRows * WRITE_BUF_POSITION, strlen(sendBuf));
    
    if(nbytes == -1) {
      
      fprintf(stderr, "errore in invio...\n\n");
      
      return EXIT_FAILURE;
    }
    
    refresh();
    
  } while(!check);
  
  recThread.join();

  endwin();  // uscita dalla GUI
  
  return 0;
}
