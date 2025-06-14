# BE RESEAU
## TPs BE Reseau - 3 MIC


# Notes des étudiants:

## Pour la compilation

Aucun changement n'a été fait : make

## Ce qui marche

Toutes les versions marchent jusqu'à la 4.2 inclue

## Choix d'implémentation

### Stop & Wait



### Négociation du taux limite de pertes

Pour la phase d'établissement de connexion, nous avons décidé de laisser le choix au client pour le taux de pertes, ce choix était le plus pertinent pour nous. Le serveur en est informé par le biais de la PDU qui envoie le SYN de connexion.


### Asynchronicité côté serveur

Dans la version 4.1 mic_tcp_accept nous attendions que la connexion soit établie en vérifiant avec un while si l'état de la connexion est établie (ESTABLISHED). Dans la version 4.2 nous avons maintenant l'utilisation d'un mutex et d'une variable de condition. Ainsi mic_tcp_accept consomme moins de ressources et se débloque quand le ACK final est reçu par process_received_PDU (pthread_cond_broadcast).

##Avantages de MICTCP_v4 vs MICTCP_v2

MICTCP-v4 par rapport à TCP & MICTCP-v2 met en place un mécanisme de fiabilité partielle au lieu d'une fiabilité totale. Cela permet, pour le transfert de vidéo en direct par exemple, de perdre en qualité d'image mais de gagner en fluidité. Cette perte de qualité peut-être gérée avec un taux limite plus ou moins élevé en fonction de la qualité d'image on est prêt à perdre. Nous avons aussi une phase d'établissement de connexion qui permet au serveur et au client de se mettre d'accord sur un numéro de séquence et au client d'envoyer le taux de pertes accepté.








## Contenu du dépôt « template » fourni
Ce dépôt inclut le code source initial fourni pour démarrer le BE. Plus précisément : 
  - README.md (ce fichier) qui notamment décrit la préparation de l’environnement de travail et le suivi des versions de votre travail; 
  - tsock_texte et tsock_video : lanceurs pour les applications de test fournies. 
  - dossier include : contenant les définitions des éléments fournis que vous aurez à manipuler dans le cadre du BE.
  - dossier src : contenant l'implantation des éléments fournis et à compléter dans le cadre du BE.
  - src/mictcp.c : fichier au sein duquel vous serez amenés à faire l'ensemble de vos contributions (hors bonus éventuels). 




5. Afin de nous permettre d’avoir accès à votre dépôt, merci de bien vouloir renseigner l'URL de votre dépôt sur le fichier accessible depuis le lien "fichier URLs dépôts étudiants" se trouvant sur moodle (au niveau de la section: BE Reseau).

## Compilation du protocole mictcp et lancement des applications de test fournies

Pour compiler mictcp et générer les exécutables des applications de test depuis le code source fourni, taper :

    make

Deux applicatoins de test sont fournies, tsock_texte et tsock_video, elles peuvent être lancées soit en mode puits, soit en mode source selon la syntaxe suivante:

    Usage: ./tsock_texte [-p|-s destination] port
    Usage: ./tsock_video [[-p|-s] [-t (tcp|mictcp)]

Seul tsock_video permet d'utiliser, au choix, votre protocole mictcp ou une émulation du comportement de tcp sur un réseau avec pertes.



