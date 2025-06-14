# BE RESEAU
## TPs BE Reseau - Younès Bezza et Matteo Sabben

## Pour la compilation

Pour compiler mictcp et générer les exécutables des applications de test depuis le code source fourni, taper :

    make

Deux applicatoins de test sont fournies, tsock_texte et tsock_video, elles peuvent être lancées soit en mode puits, soit en mode source selon la syntaxe suivante:

    Usage: ./tsock_texte [-p|-s destination] port
    Usage: ./tsock_video [[-p|-s] [-t (tcp|mictcp)]
    

## Ce qui marche

Toutes les versions marchent jusqu'à la 4.2 inclue

## Choix d'implémentation

### Stop & Wait

Pour mettre en place notre fiabilité partielle, nous avons fait le choix d'utiliser une fenêtre glissante mise en place à l'aide d'un buffer circulaire que nous utilisons pour calculer un taux de perte en comptant le nombre de pertes sur le nombre d'envoi. L'interêt d'utiliser une fenêtre glissante au lieu de simplement faire un ratio sur la totalité des envois permet d'éviter que lorsque les erreurs arrivent de façon consécutive (ce qui est souvent le cas), elles soient toutes acceptées car il n'y a pas eu d'erreur au tout début de la commmunication. 

Exemple on pourrait avoir une vidéo de 10mn sans aucune perte et soudain perdre toutes les images pendant 10 secondes si nous n'avions pas de fenêtre glissante. 

Le buffer circulaire permet d'être toujours à jour sur le taux de pertes des PDU dernièrement envoyées. La fenêtre glissante a une taille de 100, permettant d'observer un certain nombres de PDU sans trop en avoir (sinon nous risquions d'avoir le problème de l'exemple et de laisser passer trop de pertes).

### Négociation du taux limite de pertes

Pour la phase d'établissement de connexion, nous avons décidé de laisser le choix au client pour le taux de pertes, ce choix était le plus pertinent pour nous. Le serveur en est informé par le biais de la PDU qui envoie le SYN de connexion.

### Asynchronicité côté serveur

Dans la version 4.1 mic_tcp_accept nous attendions que la connexion soit établie en vérifiant avec un while si l'état de la connexion est établie (ESTABLISHED). Dans la version 4.2 nous avons maintenant l'utilisation d'un mutex et d'une variable de condition. Ainsi mic_tcp_accept consomme moins de ressources et se débloque (grâce à pthread_cond_broadcast) quand le ACK final est reçu par process_received_PDU.

### Avantages de MICTCP_v4 vs MICTCP_v2

MICTCP-v4 par rapport à TCP & MICTCP-v2 met en place un mécanisme de fiabilité partielle au lieu d'une fiabilité totale. Cela permet, pour le transfert de vidéo en direct par exemple, de perdre en qualité d'image mais de gagner en fluidité. Cette perte de qualité peut-être gérée avec un taux limite plus ou moins élevé en fonction de la qualité d'image on est prêt à perdre. Nous avons aussi une phase d'établissement de connexion qui permet au serveur et au client de se mettre d'accord sur un numéro de séquence et au client d'envoyer le taux de pertes accepté.
