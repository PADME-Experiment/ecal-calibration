/*!
 * Simple socket program client.cpp
 * Version - 1.0.0
 * Based on: Simple chat program (client side).cpp - http://github.com/hassanyf
 *
 * Copyleft (c) 2017 Rodrigo Tufino <rtufino@ups.edu.ec, r.tufino@alumnos.upm.es>
 */
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <sstream>
#include <stdio.h>      /* printf, fgets */
#include <stdlib.h>     /* atoi */
#include <math.h>       /* round, floor, ceil, trunc */
#include <time.h>
#include <algorithm>
#include <vector>
#include <cstring>  // for wcscpy_s, wcscat_s
#include <cstdlib>  // for _countof
#include <errno.h>  // for return values

//#include <algorithm> 
#include <chrono> 
#include <ctime>

using namespace std; 
//using namespace std::chrono; 


int num_crystals_read;
const int ncrystals = 650;
int buflen = 0;
int buflenr = 0;


struct ecal {
	int inumber;
	int slot;
	int nch;
	int x;
	int y;
	double realx;
	double realy;
	int ipos;
	int slotdaq;
	int nchdaq;
	double hv;
} crystals[ncrystals];

bool sortbypos(const ecal &lhs, const ecal &rhs) {
	return lhs.ipos < rhs.ipos; 
}

char* mystrsep(char** stringp, const char* delim)
{
	char* start = *stringp, * p = start ? strpbrk(start, delim) : NULL;

	if (!p) {
		*stringp = NULL;
	}
	else {
		*p = 0;
		*stringp = p + 1;
	}

	return start;
}

int readconfig() {

	std::string line;
	char* linec;

	char* tok;

	bool debug = false;

	int line_number = 0;
	std::ifstream input;

	// dimension structure
	// ecal crystals[ncrystals];



	input.open("./CalorimeterMap15pC.txt");

	while (std::getline(input, line)) {
		if (line_number > 0) {
			int ntoken = 0;
			// cast string to char
			linec = strdup(line.c_str());

			//std::cout << "linec = " << linec << std::endl;
			int tokcount = 0;
			int ik = line_number - 1;
			while ((tok = mystrsep(&linec, ";")) != NULL) {
				if (tokcount == 0) crystals[ik].inumber = atoi(tok);
				if (tokcount == 1) {
					crystals[ik].x = atoi(tok);
					crystals[ik].realx= (crystals[ik].x * 2.12) + 1.56;
				}
				if (tokcount == 2) {
					crystals[ik].y = atoi(tok);
					crystals[ik].realy = (crystals[ik].y * 2.12) + 1.56;
					// reverse position in pair rows
					int moduley = crystals[ik].y%2;
					if(moduley!=0) {
						crystals[ik].ipos = crystals[ik].y * 100 + (28-crystals[ik].x);
					} else {
						crystals[ik].ipos = crystals[ik].y * 100 + crystals[ik].x;	
					}
				}
				if (tokcount == 3) crystals[ik].slot = atoi(tok);
				if (tokcount == 4) {
					// strip 0 if present
					//if(tok[0]=='0') tok[0]=' ';
					crystals[ik].nch = atoi(tok);
				}
				if (tokcount == 5) crystals[ik].slotdaq = atoi(tok);
				if (tokcount == 6) crystals[ik].nchdaq = atoi(tok);
				if (tokcount == 7) crystals[ik].hv = atof(tok);
				tokcount++;
				//printf(" line %d - token %d = %s \n",line_number,tokcount,tok);
			}
			//printf(" line %d , inum=%d , x=%d , y=%d, slot=%d, nch=%d \n",
			//	     ik,inumber[ik],x[ik],y[ik],slot[ik],nch[ik]);
		}
		line_number++;

		// std::cout<<line<<'\n';
	}

	printf(" Map file  is long %d lines \n", line_number);
	printf(" Ecal values read =  %d \n", line_number - 1);

	num_crystals_read = line_number - 1;

	// and close the input file
	input.close();


	if (debug) {
		printf(" ecal before sorting \n");
		printf("\n");
		// and now print resulting matrix
		for (int ij = 0; ij < line_number - 1; ij++) {
				printf(" %3d %8d %6d  %3d %3d %f %f %f \n", ij,crystals[ij].inumber,crystals[ij].ipos,crystals[ij].x,crystals[ij].y,crystals[ij].realx,crystals[ij].realy,crystals[ij].hv);
				//if (ij%10 ==0 ) printf("\n");
		}

	}
	
	//now sort vs y and x - serpentone-like and print array again
	sort(crystals, crystals + line_number - 1, sortbypos);

	if (debug) {
		printf(" ecal after sorting \n");
		printf("\n");
		// and now print resulting matrix
		for (int ij = 0; ij < line_number - 1; ij++) {
			printf(" %3d %8d %6d  %3d %3d %f %f  %f \n", ij, crystals[ij].inumber, crystals[ij].ipos, crystals[ij].x, crystals[ij].y, crystals[ij].realx, crystals[ij].realy,crystals[ij].hv);
			//if (ij%10 ==0 ) printf("\n"
		}

	}

	
	return 0;
}


int main() {
  
 /* -------------- INITIALIZING VARIABLES -------------- */
 int client; // socket file descriptors
 int portNum = 27015; // port number (same that server)
 int bufsize = 1024; // buffer size
 char buffer[bufsize]; // buffer to transmit
 // padmelab1
 //char ip[] = "192.84.130.189";
 // padmelab2
 char ip[] = "192.84.130.189";
 //char ip[] = "192.168.110.128";
 bool isExit = false; // var fo continue infinitly
 //char address[14]="141.108.45.4";


std::chrono::time_point<std::chrono::system_clock> start, end;

 // start by reading crystals config
 int ireturn = readconfig();
 printf(" return code from readconfig = %d \n", ireturn);
 printf(" read crystals = %d \n", num_crystals_read);


 int lastcrystal=0;
 double lastx =0.0;
 double lasty =0.0;
 
 // then check the last position file
 if(std::fstream{"last_position.txt"}) {
   printf("last_position.txt file exist \n");
   
   int input1_lines=0;
   std::string line;
   char* linec;
   char* tok;
   std::ifstream input1;
  
   // open file and check the content
   input1.open("./last_position.txt");
   while (std::getline(input1, line)) {
     input1_lines++;
     
     int ntoken = 0;
     // cast string to char
     linec = strdup(line.c_str());
     
     //std::cout << "linec = " << linec << std::endl;
     int tokcount = 0;
      while ((tok = mystrsep(&linec, ";")) != NULL) {
       if (tokcount == 0) lastcrystal = atoi(tok);
       if (tokcount == 1) lastx= atof(tok);
       if (tokcount == 2) lasty= atof(tok);
       tokcount++;
       //printf(" line %d - token %d = %s \n",line_number,tokcount,tok);
      }
   }
   printf(" read from file lastcrystal = %8i , lastx = %f , lasty = %f  \n",lastcrystal,lastx,lasty);
   input1.close();
 } else {
   printf("last_position.txt file couldn\'t be opened (not existing or failed to open) \n");
 }


 // then start opening socket
 // Structure describing an Internet socket address.
 struct sockaddr_in server_addr;

 cout << "\n- Starting client..." << endl;

 /* ---------- ESTABLISHING SOCKET CONNECTION ----------*/

 client = socket(AF_INET, SOCK_STREAM, 0);

 /*
 * The socket() function creates a new socket.
 * It takes 3 arguments:
 * 1) AF_INET: address domain of the socket.
 * 2) SOCK_STREAM: Type of socket. a stream socket in
 * which characters are read in a continuous stream (TCP)
 * 3) Third is a protocol argument: should always be 0.
 * If the socket call fails, it returns -1.
 */

 if (client < 0) {
   cout << "\n-Error establishing socket..." << endl;
 exit(-1);
 }

 cout << "\n- Socket client has been created..." << endl;

 /*
 * The variable serv_addr is a structure of sockaddr_in.
 * sin_family contains a code for the address family.
 * It should always be set to AF_INET.
 * INADDR_ANY contains the IP address of the host. For
 * server code, this will always be the IP address of
 * the machine on which the server is running.
 * htons() converts the port number from host byte order
 * to a port number in network byte order.
 */

 server_addr.sin_family = AF_INET;
 server_addr.sin_port = htons(portNum);

 //printf("inet_pton address %s \n",ip);
 
 /*
 * This function converts an Internet address (either IPv4 or IPv6)
 * from presentation (textual) to network (binary) format.
 * If the comunication is on the same machine, you can comment this line.
 */
 inet_pton(AF_INET, ip, &server_addr.sin_addr);
 //inet_pton(AF_INET, address, &server_addr.sin_addr);

 /* ---------- CONNECTING THE SOCKET ---------- */

 if (connect(client, (struct sockaddr *) &server_addr, sizeof(server_addr))
 < 0)
   cout << "- Connection to the server " << ip << " port number: " << portNum << endl;

 /*
 * The connect function is called by the client to
 * establish a connection to the server. It takes
 * three arguments, the socket file descriptor, the
 * address of the host to which it wants to connect
 * (including the port number), and the size of this
 * address.
 * This function returns 0 on success and -1
 * if it fails.
 * Note that the client needs to know the port number of
 * the server but not its own port number.
 */

 //int imax= num_crystals_read;
 int imax=50; 
 //int imax=num_crystals_read;
 int iloop =0;
 // loop to send messages to server
 do {

   if(iloop>=imax) {
     // declare exit
     printf(" arrived at max num of crystals - exit \n");
     isExit=true;
   }
   
   if(iloop==0) {
     int ihome=-1;
     // first of all send machien to home position
     printf("sending server to home position \n");
     double positionx=0.;
     double positiony=0.;
     int icrystalzero=0;
     double hvzero=0.;
     double realx = 0.;
     double realy = 0.;
     //sprintf(buffer,"%3d; %8d; %6d; %3d; %3d; %6.2f; %6.2f; %6.2f \n", ihome, icrystalzero,icrystalzero, icrystalzero, icrystalzero,positionx, positiony,hvzero);
     sprintf(buffer,"gohome ; %6.2f ; %6.2f \n", realy,realx);
     // send to the server
     buflen=strlen(buffer);
     printf(" iloop= %d - client buflen = %d - %s \n",iloop,buflen,buffer);
     send(client, buffer, buflen, 0);

     // and wait for the machine to say ready
     // wait the response from the server
     printf(" waiting for server to answer.. \n");
   
     recv(client, buffer , bufsize, 0);
     buflenr=strlen(buffer);
     buffer[buflenr] = '\0';
     
     // check if end of transmission received
     printf(" received buffer length = %d  \n",buflenr);
     printf(" received buffer = %.*s \n",buflenr,buffer);
     int icompare=strcmp(buffer,"#");
     if(icompare==0) {
       printf(" exit buffer received \n");
       isExit=true;
     }

     // wait 5 secs
     //sleep(5);
     
   }

   int ij=iloop;
   int iok=1;
   if(lastcrystal!=0) {
     // now check if we have arrived at last crystal
     if(crystals[ij].inumber == lastcrystal) {
       printf(" resume data taking from last crystal %8i \n",lastcrystal);
       iok=1;
       // and reset lastcrystal
       lastcrystal=0;
     } else {
       iok=0;
     }
   } 
   
   if(iok>0) {
     // then continue loop

     if(iloop==200) {
       // and reset realx,realy
       double realx= +1.0;
       double realy = +2.0;

       // here create the buffer to send
       printf("sending movrel for crystal %d \n", ij);
       printf("%3d; %8d; %6d; %3d; %3d; %6.2f; %6.2f; %6.2f \n", ij, crystals[ij].inumber, crystals[ij].ipos, crystals[ij].x, crystals[ij].y, realx, realy,crystals[ij].hv);
       
       
       // sprintf(buffer,"%3d; %8d; %6d; %3d; %3d; %6.2f; %6.2f; %6.2f \n", ij, crystals[ij].inumber, crystals[ij].ipos, crystals[ij].x, crystals[ij].y,crystals[ij].realx, crystals[ij].realy,crystals[ij].hv);
       sprintf(buffer,"gorel ; %6.2f ; %6.2f \n", realx, realy);
       
       //cin >> buffer;
     
     } else { 
       // here create the buffer to send
       printf("sending movabs for crystal %d \n", ij);
       printf("%3d; %8d; %6d; %3d; %3d; %6.2f; %6.2f; %6.2f \n", ij, crystals[ij].inumber, crystals[ij].ipos, crystals[ij].x, crystals[ij].y, crystals[ij].realx, crystals[ij].realy,crystals[ij].hv);
       
       
       // sprintf(buffer,"%3d; %8d; %6d; %3d; %3d; %6.2f; %6.2f; %6.2f \n", ij, crystals[ij].inumber, crystals[ij].ipos, crystals[ij].x, crystals[ij].y,crystals[ij].realx, crystals[ij].realy,crystals[ij].hv);
       sprintf(buffer,"goabs ; %6.2f ; %6.2f \n", (float) crystals[ij].x, (float) crystals[ij].y);
       
       //cin >> buffer;
     }
     
     // Get starting timepoint 
     start = std::chrono::high_resolution_clock::now(); 

     // send to the server
     buflen=strlen(buffer);
     printf(" iloop= %d - client buflen = %d - %s \n",iloop,buflen,buffer);
     send(client, buffer, buflen, 0);

     // and increment the counter
     iloop++;
   
     // wait the response from the server
     printf(" waiting for server to answer.. \n");

     
     recv(client, buffer, bufsize , 0);

     // Get ending timepoint 
     end = std::chrono::high_resolution_clock::now(); 

     int buflenr1=strlen(buffer);
     buffer[buflenr1] = '\0';;
     printf(" received buffer length = %d  \n",buflenr1);
     printf(" received buffer = %.*s \n",buflenr1,buffer);

     int elapsed_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>
                             (end-start).count();

     double millisec = elapsed_milliseconds/1000.;

     cout << " Time taken from send command to receive answer : " << millisec << " seconds " << endl;

     printf("\n\n");

     FILE * pFile;
     // and write last position buffer
     pFile=fopen("last_position.txt","w");
     fprintf(pFile," %8d ; %6.2f ; %6.2f \n",crystals[ij].inumber,crystals[ij].realx,crystals[ij].realy);
     fclose(pFile);


     // check if end of transmission received
    int icompare=strcmp(buffer,"#");
     if(icompare==0) {
       printf(" exit buffer received \n");
       isExit=true;
     }
   

     // wait 5 secs
     //sleep(5);
   
     // print the server message
     //cout << buffer << endl;
     
   } else {
     iloop++;
   }  // endif iok
 } while (!isExit);
 
 /* ---------------- CLOSE CALL ------------- */
 printf("Asking where are motors \n");
 printf(" sending where to server \n");
 double realx=0.;
 double realy=0.;
 sprintf(buffer,"whereis ; %6.2f ; %6.2f \n", realx, realy);

 // send to the server
 buflen=strlen(buffer);
 printf(" iloop= %d - client buflen = %d - %s \n",iloop,buflen,buffer);
 send(client, buffer, buflen, 0);
 // ans wait the answer

 // wait the response from the server
 printf(" waiting for server to answer.. \n");
 
 recv(client, buffer, bufsize, 0);
 int buflenr2=strlen(buffer);
 buffer[buflenr2] = '\0';;
 printf(" received buffer length = %d \n",buflenr2);
 printf(" received buffer = %.*s \n",buflenr2,buffer);
 
 
 cout << "\nConnection terminated.\n";

 printf(" sending close to server \n");
 realx=0.;
 realy=0.;
 sprintf(buffer,"close ; %6.2f ; %6.2f \n", realx, realy);

 // send to the server
 buflen=strlen(buffer);
 printf(" iloop= %d - client buflen = %d - %s \n",iloop,buflen,buffer);
 send(client, buffer, buflen, 0);

 /*
  * Once the server presses # to end the connection,
  * the loop will break and it will close the server
  * socket connection and the client connection.
  */
 close(client);
 return 0;
}



