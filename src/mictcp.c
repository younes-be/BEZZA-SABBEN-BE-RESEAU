#include <mictcp.h>
#include <api/mictcp_core.h>

#define MAX_PORTS 32
#define TAILLE_BUF_CIRC 100

// Numéro de séquence courant pour l'envoi
unsigned static int num_sequence = 0;
// Numéro de séquence attendu à la réception
unsigned static int expected_seq_num = 0; 
// Tableau des sockets actifs
static struct mic_tcp_sock active_ports[MAX_PORTS];
static int nb_active_ports = 0;
int booleenInitialise = 0;

// Taux de perte limite accepté
int TAUX_LIMITE = -1;

// Structure pour le buffer circulaire utilisé dans la fenêtre glissante
typedef struct {
    char buffer[TAILLE_BUF_CIRC];
    int head;
    int nbData;
} buffer_circ;

buffer_circ buf_c;

// Fonction pour ajouter un état d'ACK dans le buffer circulaire
int buffer_circ_push(buffer_circ *cbuf, char etat_ACK)
{
    if (cbuf == NULL) {
        perror("problème adresse null");
    }
    int next = cbuf->head + 1;  
    if (next >= TAILLE_BUF_CIRC)
        next = 0;

    cbuf->buffer[cbuf->head] = etat_ACK;  
    cbuf->head = next;
    if (cbuf->nbData < TAILLE_BUF_CIRC) { // Vérifie si le buffer a fait un tour complet
        cbuf->nbData++;  
    }
    return 0;  
}

// Fonction pour vérifier si le taux de pertes est sous la limite
int verif_taux(buffer_circ buf)
{
    int taux = -1;
    int somme = 0;
    if (buf.nbData == 0) {
        return 1;
    } else {
        for (int i = 0; i < buf.nbData; i++) {
            somme += buf.buffer[i];
        }
        taux = somme / buf.nbData;
        return (taux < TAUX_LIMITE); 
    }
}

// Initialise le buffer circulaire
void init_buffer(buffer_circ *buf)
{
    memset(buf->buffer, 0, sizeof(buf->buffer));
    buf->head = 0;
    buf->nbData = 0;
}

/*
 * Création d'un socket MIC-TCP
 * Retourne le descripteur du socket ou -1 en cas d'erreur
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

    // Vérification de la sortie de initialize_components
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

// Vérifie si un port est actif dans la liste des sockets
static int is_port_active(int port)
{
    for (int i = 0; i < nb_active_ports; ++i) {
        if (active_ports[i].local_addr.port == port) return 1;
    }
    for (int i = 0; i < nb_active_ports; ++i) {
        if (active_ports[i].remote_addr.port == port) return 1;
    }
    return 0;
}

/*
 * Associe une adresse locale à un socket
 * Retourne 0 si succès, -1 sinon
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);
    if (socket < 0 || socket >= nb_active_ports) { 
        return -1;
    } else { 
        active_ports[socket].local_addr = addr;
        return 0;
    }
}

/*
 * Met le socket en attente de connexion (mode serveur)
 * Retourne 0 si succès, -1 sinon
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);

    mic_tcp_pdu pdu_syn;
    mic_tcp_ip_addr local_addr, remote_addr;
    int synRecu = 0;

    // Allocation mémoire pour les adresses et le PDU
    pdu_syn.payload.data = malloc(8); 
    pdu_syn.payload.size = 8;
    local_addr.addr = malloc(100);
    local_addr.addr_size = 100;
    remote_addr.addr = malloc(100);
    remote_addr.addr_size = 100;
    int statutRecv;

    // Attente de réception d'un SYN
    while (!synRecu) {
        statutRecv = IP_recv(&pdu_syn, &local_addr, &remote_addr, 10);
        if (statutRecv != -1 && pdu_syn.header.syn == 1 && pdu_syn.header.ack == 0) {
            synRecu = 1;
            printf("[MIC-TCP] PDU de type SYN reçu\n");

            // Copie profonde de l'adresse distante
            if (addr != NULL) {
                size_t addr_len = strlen(remote_addr.addr) + 1;
                addr->ip_addr.addr = malloc(addr_len);
                if (addr->ip_addr.addr == NULL) {
                    free(pdu_syn.payload.data);
                    free(local_addr.addr);
                    free(remote_addr.addr);
                    return -1;
                }
                memcpy(addr->ip_addr.addr, remote_addr.addr, addr_len);
                addr->ip_addr.addr_size = addr_len;
                addr->port = pdu_syn.header.source_port;
            } else {
                free(pdu_syn.payload.data);
                free(local_addr.addr);
                free(remote_addr.addr);
                return -1;
            }
            // Copie profonde pour le socket
            size_t addr_len2 = strlen(remote_addr.addr) + 1;
            active_ports[socket].remote_addr.ip_addr.addr = malloc(addr_len2);
            if (active_ports[socket].remote_addr.ip_addr.addr == NULL) {
                free(pdu_syn.payload.data);
                free(local_addr.addr);
                free(remote_addr.addr);
                return -1;
            }
            memcpy(active_ports[socket].remote_addr.ip_addr.addr, remote_addr.addr, addr_len2);
            active_ports[socket].remote_addr.ip_addr.addr_size = addr_len2;
            active_ports[socket].remote_addr.port = pdu_syn.header.source_port;

            // On récupère le taux limite ici
            if (pdu_syn.payload.size!=0){
                int taux_limite_recup;
                memcpy(&taux_limite_recup,pdu_syn.payload.data,sizeof(int));
                TAUX_LIMITE=taux_limite_recup;
                printf("Le taux limite est maintenant setup à %d \n",TAUX_LIMITE);
            }else{
                printf("Pas de data dans notre pdu_syn! \n");
            }

        }
    }


    // Préparation et envoi du SYN-ACK
    mic_tcp_pdu pdu_synack;
    memset(&pdu_synack, 0, sizeof(mic_tcp_pdu));
    pdu_synack.header.syn = 1;
    pdu_synack.header.ack = 1; // SYN-ACK
    pdu_synack.header.source_port = active_ports[socket].local_addr.port;
    pdu_synack.header.dest_port = active_ports[socket].remote_addr.port;
    IP_send(pdu_synack, active_ports[socket].remote_addr.ip_addr)==-1;
    printf("[MIC-TCP] PDU de type SYN-ACK envoyé\n");


    // Attente de l'ACK final
    mic_tcp_pdu pdu_ack;
    pdu_ack.payload.data = malloc(8);
    pdu_ack.payload.size = 8;
    int ackRecu = 0;
    while (!ackRecu) {
        statutRecv = IP_recv(&pdu_ack, &local_addr, &remote_addr, 100);
        if (statutRecv != -1 && pdu_ack.header.syn == 0 && pdu_ack.header.ack == 1) { //ACK final reçu
            ackRecu = 1;
            printf("[MIC-TCP] PDU de type ACK reçu\n");
        } else{
            IP_send(pdu_synack, active_ports[socket].remote_addr.ip_addr)==-1; //ACK final non reçu on renvoit SYN-ACK
        }
    }

    // Libération mémoire
    free(pdu_syn.payload.data);
    free(pdu_ack.payload.data);
    free(remote_addr.addr);

    return 0;
}

/*
 * Établit une connexion vers un serveur (mode client)
 * Retourne 0 si succès, -1 sinon
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);
    if (socket < 0 || socket >= nb_active_ports) { 
        return -1;
    } else { 
        //on demande le taux accepté au client (voir Readme pour nos explications)
        printf("Entrez le taux accepté pendant la connexion: ");
        int taux_limite;
        scanf("%d", &taux_limite);


        active_ports[socket].remote_addr = addr; 

        mic_tcp_pdu pduSYN;
        pduSYN.payload.size = 4; 
        pduSYN.payload.data = malloc(sizeof(int));
        memcpy(pduSYN.payload.data ,&taux_limite,sizeof(int));
        pduSYN.header.syn = 1;

        if ( IP_send(pduSYN, active_ports[socket].remote_addr.ip_addr) == -1 ) { // Envoi du SYN
            perror("problème IP_send mic_tcp_connect");
        } 
        int recv_status;
        
        mic_tcp_pdu pduACK;
        pduACK.payload.data = malloc(8); // 8 octets suffisent pour le header
        pduACK.payload.size = 8;

        mic_tcp_ip_addr addrLocal;
        mic_tcp_ip_addr addrRemote;

        addrLocal.addr = malloc(16);//taille de l'adresse IP
        addrLocal.addr_size = 100;
        addrRemote.addr = malloc(16);//taille de l'adresse IP
        addrRemote.addr_size = 100;
        while (1) {
            recv_status = IP_recv(&pduACK, &addrLocal, &addrRemote, 100);
            if (recv_status != -1 && pduACK.header.ack == 1 && pduACK.header.syn == 1) {
                // SYN-ACK attendu reçu
                printf("[MIC-TCP] PDU de type SYN-ACK reçu\n");
                break;
            } else {
                // Timeout, on renvoie le SYN
                if (IP_send(pduSYN, active_ports[socket].remote_addr.ip_addr)) {
                    perror("problème IP_send mic_tcp_connect");
                } 
            }
        }
        // SYN-ACK reçu, on envoie l'ACK final
        mic_tcp_pdu pduACKF;
        memset(&pduACKF, 0, sizeof(mic_tcp_pdu));
        pduACKF.header.syn = 0;
        pduACKF.header.ack = 1;
        pduACKF.header.source_port = active_ports[socket].local_addr.port;
        pduACKF.header.dest_port = active_ports[socket].remote_addr.port;
        pduACKF.payload.size = 0;
        pduACKF.payload.data = NULL;
        printf("Envoi ACK final de connexion\n");
        if (IP_send(pduACKF, active_ports[socket].remote_addr.ip_addr)) {
            perror("problème IP_send mic_tcp_connect");
        }
        printf("ACK final de connexion envoyé\n");
        printf("\n ------------------------------------------------------------ \n");



        return 0;
    }
}

/*
 * Envoie une donnée applicative via MIC-TCP
 * Retourne la taille envoyée, ou -1 en cas d'erreur
 */
int mic_tcp_send(int mic_sock, char* mesg, int mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);

    // Création du PDU
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

    // Initialisation du buffer circulaire au premier envoi
    if (booleenInitialise == 0) {
        init_buffer(&buf_c);
        booleenInitialise = 1;
    }

    struct mic_tcp_ip_addr mictcp_socket_addr = active_ports[mic_sock].remote_addr.ip_addr;

    int sent_size = IP_send(pdu, mictcp_socket_addr);
    if (sent_size == -1)
    {
        printf("[MIC-TCP] Erreur lors de l'envoi du PDU\n");
        return -1;
    }
    
    // Attente de l'ACK correspondant
    mic_tcp_pdu pduACK;
    pduACK.payload.data = malloc(8); // 8 octets suffisent pour le header
    pduACK.payload.size = 8;
    mic_tcp_ip_addr Local;
    mic_tcp_ip_addr Remote;

    Local.addr = malloc(16);//taille de l'adresse IP
    Local.addr_size = 100;
    Remote.addr = malloc(16);//taille de l'adresse IP
    Remote.addr_size = 100;

    while (1)
    {
        int statutRecv = IP_recv(&pduACK, &Local, &Remote, 100);
        if (statutRecv != -1 && pduACK.header.ack == 1 && pduACK.header.ack_num == (num_sequence % 2)) {
            // ACK attendu reçu
            printf("[MIC-TCP] PDU de type ACK reçu\n");
            num_sequence = (num_sequence + 1) % 2;
            buffer_circ_push(&buf_c, 1);
            break;
        } else {
            // Timeout ou mauvais ACK
            if (verif_taux(buf_c)) {
                // On accepte la perte, on passe au message suivant
                buffer_circ_push(&buf_c, 0);
                break;
            }
            // Sinon, on retransmet
            printf("avant le IP_send\n");
            sent_size = IP_send(pdu, mictcp_socket_addr);
            printf("après le send\n");
        }
    }

    free(pduACK.payload.data);
    free(Remote.addr);
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
    if ((payload.size = max_mesg_size) < 0) {
        return -1;
    }
    int taille = app_buffer_get(payload);
    
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
    } else {
        printf("[MIC-TCP] Erreur : port non actif pour le PDU reçu\n");
    }
}
