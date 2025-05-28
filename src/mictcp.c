#include <mictcp.h>
#include <api/mictcp_core.h>


#define MAX_PORTS 32
#define TAILLE_BUF_CIRC 100

unsigned static int num_sequence = 0;
unsigned static int expected_seq_num = 0; // Utilisée pour la gestion du numéro de séquence attendu à la réception
static struct mic_tcp_sock active_ports[MAX_PORTS];
static int nb_active_ports = 0;
int booleenInitialise=0;

int TAUX_LIMITE=-1;


//définition du buffer circulaire pour la fenêtre glissante.
typedef struct {
    char buffer[TAILLE_BUF_CIRC];
    int head;
    int nbData;
} buffer_circ;


buffer_circ buf_c;
//fonction pour gérer le buffer circlaire:

int buffer_circ_push(buffer_circ *cbuf, char etat_ACK)
{
    if (cbuf == NULL) {
        perror("problème adresse null");
    }

    int next;

    next = cbuf->head + 1;  
    if (next >= TAILLE_BUF_CIRC)
        next = 0;

    cbuf->buffer[cbuf->head] = etat_ACK;  
    cbuf->head = next;
    if (cbuf->nbData<TAILLE_BUF_CIRC){//pour savoir si on a fait un tour de buffer ou pas
        cbuf->nbData++;  
    }
    return 0;  
}



// fonction pour calculer le pourcentage actuel 
int verif_taux (buffer_circ buf){
    int taux=-1;
    int somme=0;
    if (buf.nbData==0){
        return 1;
    }else{
        for (int i =0;i<buf.nbData;i++){
            somme+=buf.buffer[i];
        }
        taux=somme/buf.nbData;
        return (taux<TAUX_LIMITE); 
    }
}

void init_buffer (buffer_circ * buf){
    memset(buf->buffer, 0, sizeof(buf->buffer));
    buf->head=0;
    buf->nbData=0;
}




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
    set_loss_rate(5);

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
        active_ports[socket].remote_addr=addr; 

        mic_tcp_pdu pduSYN;
        pduSYN.payload.size=0; //sécurité
        pduSYN.header.syn=1;
        if(IP_send(pduSYN,active_ports[socket].remote_addr.ip_addr)){//envoie du SYN
            perror("problème IP_send mic_tcp_connect");
        } 
        int recv_status;
        
        mic_tcp_pdu pduACK;
        pduACK.payload.data = malloc(8); // 8 octets suffisent pour le header
        pduACK.payload.size = 8;

        mic_tcp_ip_addr addrLocal;
        mic_tcp_ip_addr addrRemote;

        addrLocal.addr = malloc(100);
        addrLocal.addr_size = 100;
        addrRemote.addr = malloc(100);
        addrRemote.addr_size = 100;
        while (1)
        {
            recv_status=IP_recv(&pduACK, &addrLocal, &addrRemote, 100);
            if (recv_status != -1 && pduACK.header.ack == 1 && pduACK.header.syn == 1) {
                // SYN-ACK attendu reçu
                printf("[MIC-TCP] PDU de type SYN-ACK reçu\n");
                break;
            } else {
                // Timeout
                if(IP_send(pduSYN,active_ports[socket].remote_addr.ip_addr)){//renvoie du SYN
                    perror("problème IP_send mic_tcp_connect");
                } 
            }
        }
        //si on est ici le SYN-ACK a été reçu
        mic_tcp_pdu pduACKF;
        pduSYN.payload.size=0; //sécurité
        pduSYN.header.ack=1;
        printf("Envoi ACK final de connexion\n");
        if(IP_send(pduACKF,active_ports[socket].remote_addr.ip_addr)){//envoie du SYN
            perror("problème IP_send mic_tcp_connect");
        }
        printf("ACK final de connexion envoyé\n");
        printf("\n ------------------------------------------------------------ \n");
        printf("Entrez le taux accepté pendant la connexion");
        scanf("%d", &TAUX_LIMITE);
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

    if (booleenInitialise==0){
        init_buffer(&buf_c);
        booleenInitialise=1;
        //ici ajouter au 1er message que c'est un ack à traité en réception, dans le cas où on aurait perdu le ack de connexion
        pdu.header.ack=1;
    }



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

    pduLocal.addr = malloc(100);
    pduLocal.addr_size = 100;
    pduRemote.addr = malloc(100);
    pduRemote.addr_size = 100;

    while (1)
    {
        int recv_status = IP_recv(&pduACK, &pduLocal, &pduRemote, 100);
        if (recv_status != -1 && pduACK.header.ack == 1 && pduACK.header.ack_num == (num_sequence % 2)) {
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
   // free(pduLocal.addr);
    free(pduRemote.addr);
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
