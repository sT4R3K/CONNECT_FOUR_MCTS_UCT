/*
	Canvas pour algorithmes de jeux à 2 joueurs
	
	joueur 0 : humain
	joueur 1 : ordinateur
			
*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

// Couleurs pour l'affichage:
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"
// Exemple: printf(ANSI_COLOR_RED "This text is RED!" ANSI_COLOR_RESET "\n");

// Taille du plateau de jeu.
#define NB_LIGNES 6
#define NB_COLONNES 7

// Paramètres du jeu.
#define LARGEUR_MAX 7 		// nb max de fils pour un noeud (= nb max de coups possibles).
#define TEMPS 7		// temps de calcul pour un coup avec MCTS (en secondes).
#define CONST_C (sqrt(2)) // fixer la constante c.

// macros
#define AUTRE_JOUEUR(i) (1-(i))
#define min(a, b)       ((a) < (b) ? (a) : (b))
#define max(a, b)       ((a) < (b) ? (b) : (a))

// Valeurs possibles pour une case.
typedef enum {VIDE, JAUNE, ROUGE} Case;

// Critères de fin de partie.
typedef enum {NON, MATCHNUL,ORDI_GAGNE, HUMAIN_GAGNE} FinDePartie;

// Definition du type Etat (état/position du jeu).
typedef struct EtatSt {
	int joueur; // à qui de jouer ?

	Case plateau[NB_LIGNES][NB_COLONNES];

	// Pour compter les cases déjà remplies sur chaque colonne.
	int compteurs[NB_COLONNES];
} Etat;

// Definition du type Coup.
typedef struct {
	int colonne;
} Coup;

Etat* copieEtat (Etat *src) {
	Etat *etat = (Etat*) malloc (sizeof (Etat));

	etat->joueur = src->joueur;

	for (int i = 0; i < NB_LIGNES; i++)
		for (int j = 0; j < NB_COLONNES; j++)
			etat->plateau[i][j] = src->plateau[i][j];

	for (int i = 0; i < NB_COLONNES; i++)
		etat->compteurs[i] = src->compteurs[i];

	return etat;
}

Etat* etat_initial (void) {
	Etat *etat = (Etat*) malloc (sizeof (Etat));

	// Remplir les cases du plateau.
	for (int i = 0; i < NB_LIGNES; i++)
		for (int j = 0; j < NB_COLONNES; j++)
			etat->plateau[i][j] = VIDE;

	// Initialiser les compteurs
	for (int i = 0; i < NB_COLONNES; i++)
		etat->compteurs[i] = 0;

	return etat;
}

void afficheJeu (Etat *etat) {
	printf(ANSI_COLOR_CYAN "|" ANSI_COLOR_RESET);
	for (int j = 0; j < NB_COLONNES; j++)
		printf(" %d " ANSI_COLOR_CYAN "|" ANSI_COLOR_RESET, j+1);
	printf("\n");
	for (int i = 0; i < NB_COLONNES; i++)
		printf(ANSI_COLOR_CYAN "----" ANSI_COLOR_RESET);
	printf("\n");

	for (int i = 0; i < NB_LIGNES; i++)	{
		printf(ANSI_COLOR_CYAN "|" ANSI_COLOR_RESET);
		for (int j = 0; j < NB_COLONNES; j++)
			switch (etat->plateau[NB_LIGNES - i - 1][j]) { // Afficher du bas vers le haut.
				case ROUGE:
					printf(ANSI_COLOR_RED " O " ANSI_COLOR_CYAN "|" ANSI_COLOR_RESET);
					break;
				case JAUNE:
					printf(ANSI_COLOR_YELLOW " O " ANSI_COLOR_CYAN "|" ANSI_COLOR_RESET);
					break;
				case VIDE:
					printf(ANSI_COLOR_CYAN "   |" ANSI_COLOR_RESET);
			}
		printf("\n");
		for (int i = 0; i < NB_COLONNES; i++)
			printf(ANSI_COLOR_CYAN "----" ANSI_COLOR_RESET);
		printf("\n");
	}

	/*
		// Afficher les compteurs de chaque colonne:
		printf("|");
		for (int i = 0; i < NB_COLONNES; i++)
			printf(" %d |", etat->compteurs[i]);
	//*/
}

// Nouveau coup
Coup* nouveauCoup (int colonne) {
	Coup *coup = (Coup*) malloc (sizeof (Coup));

	coup->colonne = colonne;

	return coup;
}

// Demander à l'humain quel coup jouer 
Coup* demanderCoup () {
	int colonne;
	printf("\nQuelle colonne ? ");
	scanf("%d", &colonne);

	return nouveauCoup(colonne-1);
}

// Modifier l'état en jouant un coup 
// retourne 0 si le coup n'est pas possible
int jouerCoup (Etat *etat, Coup *coup) {
	// Colonne non existante:
	if (!(coup->colonne >= 0 && coup->colonne < NB_COLONNES))
		return 0;

	// Colonne déjà remplie:
	if (etat->compteurs[coup->colonne] == NB_COLONNES-1)
		return 0;
	else {
		etat->plateau[etat->compteurs[coup->colonne]][coup->colonne] = etat->joueur ? ROUGE : JAUNE;
		// Incrémenter le compteur de la colonne.
		etat->compteurs[coup->colonne]++;

		// à l'autre joueur de jouer
		etat->joueur = AUTRE_JOUEUR(etat->joueur);

		return 1;
	}
}

// Retourne une liste de coups possibles à partir d'un etat 
// (tableau de pointeurs de coups se terminant par NULL)
Coup** coups_possibles (Etat *etat) {
	Coup **coups = (Coup**) malloc ((1+LARGEUR_MAX) * sizeof (Coup*));

	int k = 0;

	for (int i = 0; i < NB_COLONNES; i++)
		if (etat->compteurs[i] < NB_LIGNES) {
			coups[k] = nouveauCoup (i);
			k++;
		}

	coups[k] = NULL;

	return coups;
}

// Definition du type Noeud
typedef struct NoeudSt {
	int joueur; // joueur qui a joué pour arriver ici
	Coup *coup; // coup joué par ce joueur pour arriver ici

	Etat *etat; // etat du jeu

	struct NoeudSt *parent;
	struct NoeudSt *enfants[LARGEUR_MAX]; // liste d'enfants : chaque enfant correspond à un coup possible
	int nb_enfants; // nb d'enfants présents dans la liste

	// POUR MCTS:
	int nb_victoires;
	int nb_simus;
} Noeud;

// Créer un nouveau noeud en jouant un coup à partir d'un parent 
// utiliser nouveauNoeud(NULL, NULL) pour créer la racine
Noeud* nouveauNoeud (Noeud *parent, Coup *coup) {
	Noeud *noeud = (Noeud*) malloc (sizeof (Noeud));

	if (parent != NULL && coup != NULL) {
		noeud->etat = copieEtat (parent->etat);
		jouerCoup (noeud->etat, coup);
		noeud->coup = coup;
		noeud->joueur = AUTRE_JOUEUR (parent->joueur);
	} else {
		noeud->etat = NULL;
		noeud->coup = NULL;
		noeud->joueur = 0;
	}
	noeud->parent = parent;
	noeud->nb_enfants = 0;

	// POUR MCTS:
	noeud->nb_victoires = 0;
	noeud->nb_simus = 0;

	return noeud;
}

// Ajouter un enfant à un parent en jouant un coup
// retourne le pointeur sur l'enfant ajouté
Noeud* ajouterEnfant (Noeud *parent, Coup *coup) {
	Noeud *enfant = nouveauNoeud (parent, coup);
	parent->enfants[parent->nb_enfants] = enfant;
	parent->nb_enfants++;
	return enfant;
}

void freeNoeud (Noeud *noeud) {
	if (noeud->etat != NULL)
		free (noeud->etat);

	while (noeud->nb_enfants > 0) {
		freeNoeud (noeud->enfants[noeud->nb_enfants-1]);
		noeud->nb_enfants--;
	}
	
	if (noeud->coup != NULL)
		free (noeud->coup);

	free (noeud);
}

// Test si l'état est un état terminal 
// et retourne NON, MATCHNUL, ORDI_GAGNE ou HUMAIN_GAGNE
FinDePartie testFin (Etat *etat) {
	/*
	// Tester le match nul:
	int nul = 1;
	for (int i = 0; i < NB_COLONNES; i++)
		if (etat->compteurs[i] < NB_LIGNES)
			nul = 0;
	if (nul)
		return MATCHNUL;*/

	// Tester si un joueur a gagné:
	int i, j, k, n = 0;
	for (int i = 0; i < NB_LIGNES; i++)
		for (int j = 0; j < NB_COLONNES; j++)
			if (etat->plateau[i][j] != VIDE) {
				n++; // nb coups joués.

				// lignes:
				k = 0;
				while (k < 4 && i+k < NB_LIGNES && etat->plateau[i+k][j] == etat->plateau[i][j])
					k++;
				if (k == 4)
					return etat->plateau[i][j] == ROUGE ? ORDI_GAGNE : HUMAIN_GAGNE;

				// colonnes:
				k = 0;
				while (k < 4 && j+k < NB_COLONNES && etat->plateau[i][j+k] == etat->plateau[i][j])
					k++;
				if (k == 4)
					return etat->plateau[i][j] == ROUGE ? ORDI_GAGNE : HUMAIN_GAGNE;

				// diagonales:
				k = 0;
				while (k < 4 && i+k < NB_LIGNES && j+k < NB_COLONNES && etat->plateau[i+k][j+k] == etat->plateau[i][j])
					k++;
				if (k == 4)
					return etat->plateau[i][j] == ROUGE ? ORDI_GAGNE : HUMAIN_GAGNE;

				k = 0;
				while (k < 4 && i+k < NB_LIGNES && j-k >= 0 && etat->plateau[i+k][j-k] == etat->plateau[i][j])
					k++;
				if (k == 4)
					return etat->plateau[i][j] == ROUGE ? ORDI_GAGNE : HUMAIN_GAGNE;

			}

	// et sinon tester le match nul	
	if (n == NB_LIGNES * NB_COLONNES) 
		return MATCHNUL;

	return NON;
}

// ----------------------------------------------------------------
//	Les fonctions suivantes sont utilisées par: ordijoue_mcts ().
// ----------------------------------------------------------------
// Calcul de la B-valeur du noeud passé en paramètre:
double bValeur  (Noeud *noeud) {
	// Eviter de faire une division par 0
	// si le noeud n'a jamais été visité.
	if (!noeud->nb_simus)
		return 0;

	double signe;
	if (noeud->parent->joueur == 0)
		signe = 1;
	else
		signe = -1;
	return	signe * (
			((double) noeud->nb_victoires /(double)  noeud->nb_simus) + //u(i) = s(i)/N(i).
			sqrt(2) * // c
			sqrt (log ((double) noeud->parent->nb_simus) / (double) noeud->nb_simus)
			);
}

// Renvoie le fils qui a la plus grande B-valeur:
Noeud* maxBValeurFils (Noeud *noeud) {
	Noeud *maxFils;
	// Calculer la B-valeur de chaque noeud enfant:
	double bValeurs[noeud->nb_enfants];
	for (int i = 0; i < noeud->nb_enfants; i++) {
		bValeurs[i] = bValeur (noeud->enfants[i]);
	}
	// Récuperer la B-valeur la plus grande:
	double maxBValeur = bValeurs[0];
	for (int i = 1; i < noeud->nb_enfants; i++)
		if (bValeurs[i] > maxBValeur) {
			maxBValeur = bValeurs[i];
		}

	// Récupérer les indices des noeuds avec B-valeur la plus grande
	int maxBValeursIndex[noeud->nb_enfants];
	int k = 0;
	for (int i = 0; i < noeud->nb_enfants; i++) {
		if (bValeurs[i] == maxBValeur) {
			maxBValeursIndex[k] = i;
			k++;
		}
	}
	// Choisir aléatoirement parmi les B-valeurs les plus grandes:
	int index = maxBValeursIndex[rand () % k];
	maxFils = noeud->enfants[index];

	return maxFils;
}

// Coisir aléatoirement parmi les fils non développés:
Noeud* selectFils (Noeud *noeud) {
	srand(time(NULL));
	Noeud *noeuds[LARGEUR_MAX];
	int k;

	Noeud *noeudCourant = noeud;
	Noeud *selectedNode;
	int selected = 0;

	do {
		// Si noeudCourant est terminal on le sélectionne:
		//if (testFin (noeudCourant->etat) != NON) {
		if (noeudCourant->nb_enfants == 0) {
			selectedNode = noeudCourant;
			selected = 1;
		}
		
		if (!selected) {
			// Récuperer les noeuds non développés si existants:
			k = 0;
			for (int i = 0; i < noeudCourant->nb_enfants; i++)
				if (noeudCourant->enfants[i]->nb_simus == 0) {
					noeuds[k] = noeudCourant->enfants[i];
					k++;
				}
			// S'il y a des noeuds non développés, 
			// sélectionner en un aléatoirement.
			if (k > 0) {
				int random = rand () % k;
				selectedNode = noeuds[random];
				selected = 1;
			}
		}

		// Si tous les noeuds ont été développés au moins une fois,
		// choisir le noeud avec la plus grande B-valeur:
		if (!selected) {
				noeudCourant = maxBValeurFils (noeudCourant);
		}
	} while (!selected);
	return selectedNode;
}

// Simuler la fin de la partie avec une marche aléatoire:
// Renvoie le noeud final de la marche.
Noeud* marcheAleatoire (Noeud *noeud) {
	srand(time(NULL));
	Noeud *noeudCourant = noeud;
	Coup **coups;
	int k;

	while (testFin (noeudCourant->etat) == NON) {
		coups = coups_possibles (noeudCourant->etat);
		// Compter les coups possibles:
		for (k = 0; coups[k] != NULL; k++);

		// Si les noeuds enfants sont déjà créés, ne pas les recréer.
		if (k != noeudCourant->nb_enfants)
			for (k = 0; coups[k] != NULL; k++)
				ajouterEnfant (noeudCourant, coups[k]);
		
		// Choisir un des enfants aléatoirement:
		noeudCourant = noeudCourant->enfants[rand () % noeudCourant->nb_enfants];

		free (coups);
	}

	return noeudCourant;
}

// Remonte l'arbre en mettant à jour nb_victoires et nb_simus
void miseAJour (Noeud *noeud, int victoire) {
	Noeud *noeudCourant = noeud;
	do {
		if (victoire)
			noeudCourant->nb_victoires++;
		noeudCourant->nb_simus++;

		noeudCourant = noeudCourant->parent;
	} while (noeudCourant->parent != NULL);

	// Et finalement pour la racine de l'arbre
	if (victoire)
			noeudCourant->nb_victoires++;
		noeudCourant->nb_simus++;
}

// Calcule et joue un coup de l'ordinateur avec MCTS-UCT
// en tempsmax secondes
void ordijoue_mcts (Etat *etat, int tempsmax) {
	clock_t tic, toc;
	tic = clock ();
	int temps;

	Coup **coups;
	Coup *meilleur_coup;

	// Créer l'arbre de recherche
	Noeud *racine = nouveauNoeud (NULL, NULL);
	racine->etat = copieEtat (etat);

	// créer les premiers noeuds:
	coups = coups_possibles (racine->etat);
	int k = 0;
	Noeud *enfant;
	while (coups[k] != NULL) {
		enfant = ajouterEnfant (racine, coups[k]);
		k++;
	}

	// MCTS-UCT pour déterminer le meilleur coup:
	Noeud *sNoeud;
	Noeud *mNoeud;
	int iter = 0;
	do {
		sNoeud = selectFils (racine);
		mNoeud = marcheAleatoire (sNoeud);
		miseAJour (mNoeud,(testFin (mNoeud->etat) == ORDI_GAGNE) ? 1 : 0);

		toc = clock ();
		temps = (int) (((double) (toc - tic)) / CLOCKS_PER_SEC);
		iter++;
	} while (temps < tempsmax);

	for (int i = 0; i < racine->nb_enfants; i++) {
		printf("%d\n", racine->enfants[i]->coup->colonne+1);
		printf("nb_simus %d\n", racine->enfants[i]->nb_simus);
		printf("nb_victoires %d\n", racine->enfants[i]->nb_victoires);
		printf("%f\n", bValeur(racine->enfants[i]));
		printf("\n");
	}

	enfant = maxBValeurFils (racine);
	meilleur_coup = enfant->coup;

	// Réponse à la question 1:
	printf("Simulations effectuées: %d, ", racine->nb_simus);
	printf("probabilité de gagner: %f .\n", ((double) racine->nb_victoires / (double) racine->nb_simus));

	// Jouer le meilleur premier coup:
	jouerCoup (etat, meilleur_coup);

	// Penser à libérer la mémoire:
	freeNoeud (racine);
	free (coups);
}

int main (void) {
	Coup *coup;
	FinDePartie fin;

	// initialisation
	Etat *etat = etat_initial();

	// Choisir qui commence : 
	printf("Qui commence (0 : humain, 1 : ordinateur) ? ");
	scanf("%d", &(etat->joueur));

	// boucle de jeu
	do  {
		printf("\n");
		afficheJeu (etat);
		printf("\n");

		if (etat->joueur == 0) {
			// tour de l'humain.
			do {
				coup = demanderCoup ();
			} while (!jouerCoup (etat, coup));
		} else {
			// tour de l'Ordinateur.
			ordijoue_mcts (etat, TEMPS);
		}

		fin = testFin (etat);
	} while (fin == NON);

	printf("\n");
	afficheJeu(etat);
	printf("\n");

	if ( fin == ORDI_GAGNE )
		printf( "** L'ordinateur a gagné **\n");
	else if ( fin == MATCHNUL )
		printf(" Match nul !  \n");
	else
		printf( "** BRAVO, l'ordinateur a perdu  **\n");
	return 0;
}