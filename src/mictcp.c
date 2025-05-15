#include <mictcp.h>
#include <api/mictcp_core.h>


#define MAX_PORTS 32
static int active_ports[MAX_PORTS];
static int nb_active_ports = 0;
/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm)
{
   int result = -1;
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   result = initialize_components(sm); /* Appel obligatoire */
   set_loss_rate(0);

   return result;
}

// Ajoute un port à la liste des ports actifs
static void add_active_port(int port) {
    if (nb_active_ports < MAX_PORTS) {
        active_ports[nb_active_ports++] = port;
    }
}

// Vérifie si un port est actif
static int is_port_active(int port) {
    for (int i = 0; i < nb_active_ports; ++i) {
        if (active_ports[i] == port) return 1;
    }
    return 0;
}

/*
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr)
{
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   //add_active_port(addr.port);
   //return 0;
}

/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    return -1;
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    return -1;
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send (int mic_sock, char* mesg, int mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");

	// Créer un mic_tcp_pdu
	mic_tcp_pdu pdu;
	pdu.payload.data = mesg;
	pdu.payload.size = mesg_size;

	pdu.header.source_port = // aller chercher dans la structure mictcp_socket correspondant au socket identifié par mic_sock passé en paramètre.
	pdu.header.dest_port =  // Port auquel on veut envoyer le message qui a été donné par l’application lors du mic_tcp_connect et qu’on a stocké dans la structure mictcp_socket correspondant au socket identifié par mic_sock passé en paramètre.

//Envoyer un message (dont la taille le contenu sont passés en paramètres).
	Int sent_size = IP_send(pdu, structure mictcp_socket_addr contenue dans la structure mictcp_socket correspondant au socket identifié par mic_sock passé en paramètre).

             
    return sent_size;
}

/*
 * Permet à l’application réceptrice de réclamer la récupération d’une donnée
 * stockée dans les buffers de réception du socket
 * Retourne le nombre d’octets lu ou bien -1 en cas d’erreur
 * NB : cette fonction fait appel à la fonction app_buffer_get()
 */
int mic_tcp_recv (int socket, char* mesg, int max_mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    mic_tcp_payload payload;
    payload.data = mesg;
    if ((payload.size = max_mesg_size) <0){
        return -1;
    }
    int taille = app_buffer_get(payload);
    return taille;// ou -1 si erreur;
    
}

/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close (int socket)
{
    printf("[MIC-TCP] Appel de la fonction :  "); printf(__FUNCTION__); printf("\n");
    return -1;
}

/*
 * Traitement d’un PDU MIC-TCP reçu (mise à jour des numéros de séquence
 * et d'acquittement, etc.) puis insère les données utiles du PDU dans
 * le buffer de réception du socket. Cette fonction utilise la fonction
 * app_buffer_put().
 */
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_ip_addr local_addr, mic_tcp_ip_addr remote_addr)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    if (is_port_active(pdu.header.dest_port)) {
        app_buffer_put(pdu.payload); // On insère le PDU dans le buffer de réception
    }

}
