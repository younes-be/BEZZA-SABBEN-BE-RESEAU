#include <mictcp.h>
#include <api/mictcp_core.h>


#define MAX_PORTS 32


static struct mic_tcp_sock active_ports[MAX_PORTS];
static int nb_active_ports = 0;
/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */



int mic_tcp_socket(start_mode sm)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);
    if (nb_active_ports >= MAX_PORTS) {
        printf("[MIC-TCP] Erreur : nombre maximum de sockets atteint\n");
        return -1;
    }
    int result = initialize_components(sm); /* Appel obligatoire */
    set_loss_rate(0);

    //vérification de la sortie de initialize_components
    if (result == -1) {
        printf("[MIC-TCP] Erreur lors de l'initialisation des composants\n");
        return -1;
    }

    // Initialisation de la structure du socket
    
    memset(&active_ports[nb_active_ports].local_addr, 0, sizeof(mic_tcp_sock_addr));
    memset(&active_ports[nb_active_ports].remote_addr, 0, sizeof(mic_tcp_sock_addr));
    nb_active_ports++;

    return nb_active_ports - 1; // retourne l'index du socket créé
}

// Ajoute un port à la liste des ports actifs
/*static void add_active_port(int port) {
    if (nb_active_ports < MAX_PORTS) {
        struct mic_tcp_sock_addr addrlocal;
        addrlocal.port = port;
        addrlocal.ip_addr.addr = "127.0.0.1";
        addrlocal.ip_addr.addr_size = strlen(addrlocal.ip_addr.addr) + 1;
        active_ports[nb_active_ports].remote_addr=addrlocal;
        nb_active_ports++;
    }else{
        printf("[MIC-TCP] Erreur : nombre maximum de ports actifs atteint\n");
    }
}*/

// Vérifie si un port est actif
static int is_port_active(int port) {
    for (int i = 0; i < nb_active_ports; ++i) {
        if (active_ports[i].local_addr.port == port) return 1;
    }
    for (int i = 0; i < nb_active_ports; ++i) {
        if (active_ports[i].remote_addr.port == port) return 1;
    }
    return 0;
}

/*
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);
    if (socket < 0 || socket >= nb_active_ports){ 
        return -1;
    }else{ 
        active_ports[socket].local_addr = addr;
        return 0;
    }
}

/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);
    return 0;// à coder plus tard
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);
    if (socket < 0 || socket >= nb_active_ports){ 
        return -1;
    }else{ 
        active_ports[socket].remote_addr = addr;
        return 0;
    }
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */

int mic_tcp_send (int mic_sock, char* mesg, int mesg_size) //mic_sock est l'indice du tableau de sockets
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    unsigned static int num_sequence =0;
	// Créer un mic_tcp_pdu
	mic_tcp_pdu pdu;
	pdu.payload.data = mesg;
	pdu.payload.size = mesg_size;

    int numeroPortDest=active_ports[mic_sock].remote_addr.port;
	pdu.header.dest_port = numeroPortDest;

    pdu.header.seq_num=num_sequence; // pdu construite


    struct mic_tcp_ip_addr mictcp_socket_addr=active_ports[mic_sock].remote_addr.ip_addr;

    int sent_size = IP_send(pdu,mictcp_socket_addr);// contenue dans la structure mictcp_socket correspondant au socket identifié par mic_sock passé en paramètre).
    if (sent_size == -1)
    {
        printf("[MIC-TCP] Erreur lors de l'envoi du PDU\n");
        return -1;
    }

    int notReceived = -1;
    mic_tcp_pdu pduACK;

    notReceived = IP_recv(&pduACK, &active_ports[mic_sock].local_addr.ip_addr, &active_ports[mic_sock].remote_addr.ip_addr, 1000); // on attend 1 seconde, donc on utilise le timer dans la fonction, permet de ne pas avoir un while qui lance la fonction IP_recv plein de fois

    while(notReceived==-1) // dans le futur on pourra faire un for pour abandonner une pdu qui poserait problème
    {
        notReceived = IP_recv(&pduACK, &active_ports[mic_sock].local_addr.ip_addr, &active_ports[mic_sock].remote_addr.ip_addr, 1000); 
        if(notReceived != -1){
                if (pduACK.header.ack == (num_sequence)){
                    notReceived=0;
                    printf("[MIC-TCP] PDU de type ACK reçu\n");
                    printf("[MIC-TCP] Numéro de séquence : %d\n", pduACK.header.seq_num);
                    printf("[MIC-TCP] Numéro d'acquittement : %d\n", pduACK.header.ack_num);
                    printf("\n"); 
                }else{
                    printf("PDU de type ACK non reçu, une autre PDU a été reçue\n");
                    printf("Numéro de séquence : %d\n", pduACK.header.seq_num);
                }
        }else{
            sent_size = IP_send(pdu,mictcp_socket_addr);//on renvoie la pdu après l'attente d'une seconde
        }
    }

    num_sequence=(num_sequence+1)%2;//on change le numéro de séquence pour la prochaine pdu

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
    static int expected_seq_num = 0;
    mic_tcp_pdu pdu;
    mic_tcp_ip_addr local_addr, remote_addr;
    int recv_size = -1;

    // Préparer le buffer pour recevoir le PDU
    pdu.payload.data = malloc(max_mesg_size);
    pdu.payload.size = max_mesg_size;

    
        recv_size = IP_recv(&pdu, &local_addr, &remote_addr, 0);
        if (recv_size < 0) {
            free(pdu.payload.data);
            printf("[MIC-TCP] Erreur lors de la réception du PDU\n");
            return -1;
        }

        // numéro de séquence attendu reçu
        if (pdu.header.seq_num == expected_seq_num) {
            // création et envoie du ACK
            mic_tcp_pdu ack_pdu;
            memset(&ack_pdu, 0, sizeof(mic_tcp_pdu));
            ack_pdu.header.ack = 1;
            ack_pdu.header.ack_num = expected_seq_num;
            ack_pdu.header.dest_port = pdu.header.source_port;
            ack_pdu.header.source_port = pdu.header.dest_port;
            ack_pdu.payload.data = NULL;
            ack_pdu.payload.size = 0;
            IP_send(ack_pdu, remote_addr);

            int size_to_copy;
            // Copier la donnée reçue dans le buffer utilisateur
            if(recv_size < max_mesg_size) {
                size_to_copy = recv_size;
            }else {
                size_to_copy = max_mesg_size;
            }

            memcpy(mesg, pdu.payload.data, size_to_copy);// transfert de la donnée dans le buffer utilisateur

            free(pdu.payload.data);

            expected_seq_num = (expected_seq_num + 1) % 2;
            return size_to_copy;
        } else {//numéro de déquence inattendu
            printf("[MIC-TCP] Numéro de séquence inattendu: %d, attendu: %d\n", pdu.header.seq_num, expected_seq_num);
            free(pdu.payload.data);
            return -1;
        }
    
}

/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close (int socket)
{
    printf("[MIC-TCP] Appel de la fonction :  "); printf(__FUNCTION__); printf("\n");
    return 0;
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
