#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#include <fstream>     
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <vector>
#include <sstream>
#include <pthread.h>
#include <signal.h>

#define RETURN_ERROR (-1)
#define SERVER_PORT  (8080)
#define SERVER_MAX_CONNECTIONS  (10)
#define SERVER_INBUFFER_LENGTH  (2048)
#define SERVER_RX_TIMEOUT	(2)
using namespace std;
int socket_set_timeout(int socket_fd, int seconds) {
	struct timeval timeout;
	timeout.tv_sec = seconds;
	timeout.tv_usec = 0;	 
	return setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO,  (const void *) &timeout, (socklen_t) sizeof(struct timeval));}

int server_get_file(std::string page, std::string &filename){ //Mikä tiedosto haetaan (get pyynnön perusteella)
	filename = "/home/zuryuk/Desktop/web-server/";	//Kansio josta tietoja haetaan
	if (page == "/"){
	filename += "index.html";} // tyhjällä get pyynnöllä palautetaan index.html
	else {
	filename += page;
	}}
int server_read_file(std::string filename, char * &memblock) //Tiedoston luku
{
  streampos size;
  ifstream file (filename.c_str(), ios::in|ios::binary|ios::ate); //Filestream auki määriteltyyn tiedostoon
  if (file.is_open())
  {
    size = file.tellg();
    memblock = new char [size + (streampos)1]; 
    file.seekg (0, ios::beg);
    file.read (memblock, size); //Luetaan tiedosto memblockkiin
    file.close();
    return size;
  }
  else {
	return -1;}}

void server_handle_query(vector <std::string> &v, std::string query) { //Queryn pätkintä
	char delimiter = ' ';
	string::size_type i = 0; // Aloitus indeksi
	string::size_type j = query.find(delimiter); // Lopetus indeksi
	
	while(j!= string::npos) {
		v.push_back(query.substr(i, j-i));
		i = ++j;
		j = query.find(delimiter, j);
	}
	v.push_back(query.substr(i, query.size()));
}
void *handle_new_connection(void *arg) { //Threadin suorittama ohjelma
	int client_fd = *((int *) arg); // muunnetaan file descriptori kunnolliseen muotoon
	char in_buffer[SERVER_INBUFFER_LENGTH]; //bufferi
	memset(in_buffer, 0, SERVER_INBUFFER_LENGTH); //nollataan bufferi
	socket_set_timeout(client_fd, SERVER_RX_TIMEOUT);	

	int bytes_read = 0;
	int total_bytes_read = 0;
	do {
		bytes_read = read(client_fd, &in_buffer[total_bytes_read], SERVER_INBUFFER_LENGTH-1-total_bytes_read);  //luetaan pyyntö
		if (bytes_read > 0) {
			total_bytes_read += bytes_read;
		}

	} while (bytes_read > 0); 

	vector <std::string> query_vector;
	server_handle_query(query_vector,  std::string(in_buffer));

	if (query_vector.size() >= 3 && query_vector.at(0) == "GET") {

		std::string filename;
		server_get_file(query_vector.at(1), filename);
		char *  membuffer;
		int bytes_read = server_read_file(filename, membuffer); // Handle data length (int -> unsinged long etc...)
		std::string bytes_read_string;

		ostringstream ss;
		ss << bytes_read;
		bytes_read_string = ss.str();		
				
		if (bytes_read >= 0) 
			{
			std::string response_header = "HTTP/1.1 200 OK\r\n";
			response_header += "Content-Length: " + bytes_read_string + "\r\n\r\n";
			write(client_fd, response_header.c_str(),  response_header.size());
			write(client_fd, membuffer, bytes_read); 
			delete[] membuffer;} 
		else 
			{
			std::string response_header = "HTTP/1.1 200 OK\r\n";
			std::string E404 = "<html><body><h1>ERROR 404 PAGE NOT FOUND</h1></body</html>";
			response_header += "Content-Length: 56\r\n\r\n";
			write(client_fd, response_header.c_str(),  response_header.size());
			write(client_fd, E404.c_str(), E404.size());
			// Handle page not found
			;}		
	}
	sleep(1);
	close(client_fd);
}
void kill_sig(){
	for(int i = 0; i< 32; i++){
	close(i);}
	exit(EXIT_SUCCESS);
}

int main(void) {
	//DAEMON SETUP
        pid_t pid, sid;
        pid = fork(); //Irrottaudutaan prosesseista
        if (pid < 0) {
                exit(EXIT_FAILURE); // Epäonnistunut exit
        }
        if (pid > 0) {
		printf("Daemon started\n");
                exit(EXIT_SUCCESS); // Pääohjelma voidaan sulkea onnistuneella forkilla
        }
        umask(0); //Kaikki käyttöoikeudet luotuihin tiedostoihin
        sid = setsid(); //Uusi signature ID
        if (sid < 0) {
                exit(EXIT_FAILURE);
        }
        if ((chdir("/server")) < 0) { //Työskentely kansion vaihto 
                exit(EXIT_FAILURE);
        }
	//SIGNAALIN KÄSITTELYT
	(void)signal(SIGCLD, SIG_IGN); /* ignore child death */
	(void)signal(SIGHUP, SIG_IGN); /* ignore terminal hangups */
	signal(SIGTERM, kill_sig);
	for(int i=0;i<32;i++)
		(void)close(i);		/* close open filedescriptors */
	(void)setpgrp();		/* break away from process group */	
	//DAEMON SETUP END
	// WEB SERVER Setup
	int server_fd;
	int client_fd;
	struct sockaddr_in server_addr; 
	struct sockaddr_in client_addr; 
	server_fd = socket(AF_INET, SOCK_STREAM, 0);  //TCP socket
	if (server_fd == -1)
	{return RETURN_ERROR;}
	// Alustaa tietueen arvoon 0
	memset(&server_addr, 0, sizeof(struct sockaddr_in));	
	memset(&client_addr, 0, sizeof(struct sockaddr_in));	

	server_addr.sin_family = AF_INET; 		//TCP portista 8080 localhost
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(server_fd, (const struct sockaddr*)&server_addr, sizeof(struct sockaddr_in)) < 0) { //Bindataan socketti filedescriptoriin
		close(server_fd); 
		return RETURN_ERROR;		}
	if (listen(server_fd, SERVER_MAX_CONNECTIONS) < 0) //Socketin kuuntelu päälle
	{
		close(server_fd); 
		return RETURN_ERROR;		}
	// Tallennetaan tietueen pituus muuttujaan
	int addr_length = sizeof(struct sockaddr_in);
	//Threadi clienttien käsittelyyn
	pthread_t cli_thread;
	//MAIN LOOP
        while (1) {
	client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *) &addr_length); //Hyväksytään clientti
	if (client_fd >= 0){
			pthread_create(&cli_thread, NULL, handle_new_connection, &client_fd); // Threadataan clientin käsittely, argumenttina file descriptori
			continue; 
		}
        }
	 // siivotaan
	close(client_fd);
	close(server_fd);
   return 0;
}