#include <mictcp.h>
#include <api/mictcp_core.h>

#define MAX_PORTS 32
#define TAILLE_BUF_CIRC 100

// Structure pour le buffer circulaire utilisé dans la fenêtre glissante
typedef struct {
    char buffer[TAILLE_BUF_CIRC];
    int head;
    int nbData;
} buffer_circ;

static struct mic_tcp_sock active_ports[MAX_PORTS];
static int nb_active_ports = 0;
int num_sequence=0;
int booleenInitialise = 0;
int TAUX_LIMITE = -1;
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
    memset(&active_ports[nb_active_ports], 0, sizeof(mic_tcp_sock));
    nb_active_ports++;

    return nb_active_ports - 1; // retourne l'index du socket créé
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
    
    // La logique de réception du SYN et envoi du SYN-ACK est gérée dans process_received_PDU
    // Donc on attend juste que l'état passe à ESTABLISHED
    while (active_ports[socket].state != ESTABLISHED) {
        //on attend
    }
    *addr = active_ports[socket].remote_addr;// Stockage de l'adresse distante au niveau du socket

    return 0;
}

/*
 * Établit une connexion vers un serveur (mode client)
 * Retourne 0 si succès, -1 sinon
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);
    
    // Stockage de l'adresse distante
    active_ports[socket].remote_addr = addr;
    
    // Demande du taux limite à l'utilisateur
    int taux;
    printf("Entrez le taux de perte limite acceptable (en pourcentage): ");
    scanf("%d", &taux);
    
    // Construction du SYN
    mic_tcp_pdu pdu;
    pdu.header.source_port = active_ports[socket].local_addr.port;
    pdu.header.dest_port = addr.port;
    pdu.header.seq_num = 0;
    pdu.header.ack_num = 0;
    pdu.header.syn = 1;
    pdu.header.ack = 0;
    pdu.header.fin = 0;
    
    // On met le taux dans les données du PDU
    pdu.payload.size = sizeof(int);
    pdu.payload.data = malloc(sizeof(int));
    memcpy(pdu.payload.data, &taux, sizeof(int));

    // Essais multiples pour l'envoi du SYN
    int max_tries = 5; // 5 semble raisonnable
    int tries = 0;
    int syn_ack_received = 0;

    while (tries < max_tries && !syn_ack_received) {
        printf("[MIC-TCP] Tentative %d d'envoi du SYN\n", tries + 1);
        
        // Envoi du SYN avec le taux
        if (IP_send(pdu, addr.ip_addr) == -1) {
            tries++;
            continue;
        }

        // Attente du SYN-ACK
        mic_tcp_pdu synack;
        synack.payload.data = malloc(8);
        synack.payload.size = 8;
        mic_tcp_ip_addr local, remote;
        local.addr = malloc(16);
        remote.addr = malloc(16);
        local.addr_size = 16;
        remote.addr_size = 16;

        // Réception du SYN-ACK
        if (IP_recv(&synack, &local, &remote, 100) != -1) {
            if (synack.header.syn && synack.header.ack) {
                // SYN-ACK reçu, envoi de l'ACK final
                printf("[MIC-TCP] PDU de type SYN-ACK reçu\n");
                syn_ack_received = 1;
                
                // Construction de l'ACK final
                pdu.header.syn = 0;
                pdu.header.ack = 1;
                pdu.header.ack_num = (synack.header.seq_num + 1) %2;
                
                // Envoi de l'ACK final
                if (IP_send(pdu, addr.ip_addr) != -1) {
                    printf("[MIC-TCP] PDU de type ACK envoyé\n");
                    active_ports[socket].state = ESTABLISHED;  // Connexion établie
                    break;
                }else{
                    printf("[MIC-TCP] PDU de type ACK NON envoyé\n");
                } 

            }
        }

        free(synack.payload.data);
        free(local.addr);
        free(remote.addr);
        
        if (!syn_ack_received) {
            tries++;
            printf("[MIC-TCP] PDU de type SYN-ACK non reçu, nouvelle tentative\n");
        }
    }

    free(pdu.payload.data);
    return syn_ack_received ? 0 : -1;  // a la fin on constate s'il a été recu au bout de nos tentatives
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
    pdu.header.fin = 0;

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
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);
    
    // Trouver le socket correspondant au port de destination
    int socket_idx = -1;
    for (int i = 0; i < nb_active_ports; i++) {
        if (active_ports[i].local_addr.port == pdu.header.dest_port) {
            socket_idx = i;
            break;
        }
    }
    
    if (socket_idx == -1) return;

    // Gestion de l'établissement de connexion
    if (pdu.header.syn && !pdu.header.ack) {
        // Réception d'un SYN
        printf("[MIC-TCP] PDU de type SYN reçu\n");
        
        // Extraction du taux limite des données
        if (pdu.payload.size == sizeof(int)) {
            memcpy(&TAUX_LIMITE, pdu.payload.data, sizeof(int));
            printf("[MIC-TCP] Taux limite reçu: %d\n", TAUX_LIMITE);
        }
        // On récupère aussi l'adresse et le port pour le socket
        active_ports[socket_idx].remote_addr.ip_addr = remote_addr;
        active_ports[socket_idx].remote_addr.port = pdu.header.source_port;
        
        // Préparation et envoi du SYN-ACK
        mic_tcp_pdu synack;
        synack.header.source_port = pdu.header.dest_port;
        synack.header.dest_port = pdu.header.source_port;
        synack.header.syn = 1;
        synack.header.ack = 1;
        synack.header.fin = 0;
        synack.payload.data = NULL;
        synack.payload.size = 0;
        
        int max_tries = 5;
        int tries = 0;
        int ack_received = 0;

        while (tries < max_tries && !ack_received) {
            printf("[MIC-TCP] Tentative %d d'envoi du SYN-ACK\n", tries + 1);
            
            if (IP_send(synack, remote_addr) != -1) {
                // Attente de l'ACK final
                mic_tcp_pdu ack;
                ack.payload.data = malloc(8);
                ack.payload.size = 8;
                
                if (IP_recv(&ack, &local_addr, &remote_addr, 100) != -1) {
                    if (!ack.header.syn && ack.header.ack) {
                        printf("[MIC-TCP] ACK final reçu\n");
                        ack_received = 1;
                        active_ports[socket_idx].state = ESTABLISHED;
                    }
                }
                free(ack.payload.data);
            }
            
            if (!ack_received) {
                tries++;
                printf("[MIC-TCP] PDU de type ACK non reçu, nouvelle tentative\n");
            }
        }
        return;
    }

    if (!pdu.header.syn && pdu.header.ack) {
        // Si c'est l'ACK de l'établissement de connexion
        if (active_ports[socket_idx].state != ESTABLISHED) {
            printf("[MIC-TCP] PDU de type ACK reçu\n");
            printf("[MIC-TCP] Connection établie!\n");
            active_ports[socket_idx].state = ESTABLISHED;
            return;
        }
        // ACK normal pendant la communication
        // Traitement des ACKs pour le mécanisme de reprise sur perte
        if (verif_taux(buf_c)) {
            buffer_circ_push(&buf_c, 1);
        }
        return;
    }
    
    
    // Vérifier que c'est un PDU de données valide
    if (active_ports[socket_idx].state == ESTABLISHED && 
        pdu.payload.size > 0 && 
        !pdu.header.syn && 
        !pdu.header.ack && 
        !pdu.header.fin &&
        pdu.payload.data[0] != '\0') {// Vérification en plus pour ne pas afficher une pdu vide.

        // Envoi de l'ACK
        mic_tcp_pdu ack;
        ack.header.source_port = pdu.header.dest_port;
        ack.header.dest_port = pdu.header.source_port;
        ack.header.seq_num = pdu.header.ack_num;
        ack.header.ack_num = pdu.header.seq_num;
        ack.header.syn = 0;
        ack.header.ack = 1;
        ack.header.fin = 0;
        
        printf("[MIC-TCP] PDU de type ACK envoyé\n");
        IP_send(ack, remote_addr);
        
        // Insertion des données dans le buffer applicatif seulement si c'est un PDU de données valide
        app_buffer_put(pdu.payload);
    }
}