#include <mictcp.h>
#include <api/mictcp_core.h>


#define MAX_PORTS 32

unsigned static int num_sequence = 0;
unsigned static int expected_seq_num = 0; // Utilisée pour la gestion du numéro de séquence attendu à la réception
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

int mic_tcp_send (int mic_sock, char* mesg, int mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    
    unsigned int static nombreDeMessage=0; // prend en compte les pertes aussi

    unsigned int static nombreDeMessageRecu=0; // ne prend pas en compte les pertes

    // Créer un mic_tcp_pdu
    mic_tcp_pdu pdu;
    pdu.payload.data = mesg;
    pdu.payload.size = mesg_size;

    int numeroPortDest = active_ports[mic_sock].remote_addr.port;
    int numeroPortSrc = active_ports[mic_sock].local_addr.port;
    pdu.header.dest_port = numeroPortDest;
    pdu.header.source_port = numeroPortSrc;
    pdu.header.seq_num = num_sequence % 2; // modulo 2 pour alternance
    pdu.header.ack = 0;
    pdu.header.syn = 0;
    pdu.header.fin = 0;

    struct mic_tcp_ip_addr mictcp_socket_addr = active_ports[mic_sock].remote_addr.ip_addr;

    int sent_size = IP_send(pdu, mictcp_socket_addr);
    if (sent_size == -1)
    {
        printf("[MIC-TCP] Erreur lors de l'envoi du PDU\n");
        return -1;
    }
    
    mic_tcp_pdu pduACK;
    pduACK.payload.data = malloc(8); // 8 octets suffisent pour le header
    pduACK.payload.size = 8;
    mic_tcp_ip_addr pduLocal;
    mic_tcp_ip_addr pduRemote;
    while(IP_recv(&pduACK, &pduLocal, &pduRemote, 1000)==-1 || pduACK.header.ack == 0||pduACK.header.ack_num != (num_sequence % 2))
    {
            //utiliser le buffer circulaire
            if (nombreDeMessage<100){
                taux_limite=nombreDeMessageRecu/nombreDeMessage; //attention div par 0
            }else { // on a au moins 100 msg
                taux_limite=
            }   


            //si le numéro de séquence de l'ACK reçu correspond à celui attendu
            if (pduACK.header.ack_num == (num_sequence % 2)){
                printf("[MIC-TCP] PDU de type ACK reçu\n");
                printf("[MIC-TCP] Numéro de séquence : %d\n", pduACK.header.seq_num);
                printf("[MIC-TCP] Numéro d'acquittement : %d\n", pduACK.header.ack_num);
                printf("\n"); 
                num_sequence = (num_sequence + 1) % 2; //modulo 2 à changer
                nombreDeMessage++;
                nombreDeMessageRecu++;
                break; // sinon on fait un sent pour rien
            }else{ // si le numéro de séquence ne correspond pas à celui attendu
                printf("PDU de type ACK non reçu, une autre PDU a été reçue\n");
                printf("Numéro de séquence : %d\n", pduACK.header.seq_num);
                /*
                    if (pourcentage_perte>taux_limite){
                        //rien 
                    }else {
                        //le numéro de séquence n'est pas à changer car il n'est pas bon, il sera bon à la prochaine itération
                        //à changer quand on sera pas modulo 2
                        nombreDeMessage++;
                        break;
                    }

                */
            }
            sent_size = IP_send(pdu, mictcp_socket_addr); // on renvoie la pdu après l'attente d'une seconde
    }
        

    

    

    free(pduACK.payload.data);
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
    printf("message reçu : %s\n", payload.data);
    if (taille == -1)
    {
        printf("[MIC-TCP] Erreur lors de la récupération du PDU\n");
        return -1;
    }
    return taille;
    
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
        // Si c'est un PDU DATA (pas un ACK)
        if (pdu.header.ack == 0) {
            if ((pdu.header.seq_num % 2) == (expected_seq_num % 2)) {
                // Nouveau PDU attendu
                app_buffer_put(pdu.payload);
                expected_seq_num = (expected_seq_num + 1) % 2;
            }
            // Sinon, c'est un doublon (même numéro de séquence), on ne réinsère pas dans le buffer

            // Dans tous les cas, on renvoie un ACK pour le dernier SEQ reçu
            mic_tcp_pdu ack_pdu;
            memset(&ack_pdu, 0, sizeof(mic_tcp_pdu));
            ack_pdu.header.ack = 1;
            ack_pdu.header.ack_num = pdu.header.seq_num % 2;
            ack_pdu.header.dest_port = pdu.header.source_port;
            ack_pdu.header.source_port = pdu.header.dest_port;
            ack_pdu.payload.data = NULL;
            ack_pdu.payload.size = 0;
            IP_send(ack_pdu, remote_addr);
        }
        // Si c'est un ACK, rien à faire ici (la logique d'attente d'ACK est côté send)
    }
    else {
        printf("[MIC-TCP] Erreur : port non actif pour le PDU reçu\n");
    }
}
