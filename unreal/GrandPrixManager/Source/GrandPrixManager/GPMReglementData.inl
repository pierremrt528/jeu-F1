// Généré depuis js/reglement.js (gen/emit.mjs) : ères, catalogue de pièces par pôle, effets unitaires.
struct FW { const TCHAR* K; float V; };
struct FPoolPart { const TCHAR* Name; const TCHAR* Desc; TArray<FW> W; };
struct FEraData { TMap<FString, float> Mul; TArray<FPoolPart> Extra; };
static const UGPMFactorySubsystem::FEra GEras[] = {
	{ TEXT("fondation"), TEXT("Fondation"), TEXT(""), TEXT("Ère équilibrée : aucun pôle ne domine."), TEXT("Voitures longues, aileron avant à trois éléments, grande dérive.") },
	{ TEXT("effet-sol"), TEXT("Effet de sol"), TEXT("chassis"), TEXT("Le châssis est roi : plus de pièces châssis, beaucoup plus d’appui et d’adhérence."), TEXT("L’appui vient du dessous de la voiture : fond sculpté, pontons creusés, aileron avant à deux éléments et aileron de poutre.") },
	{ TEXT("hybride"), TEXT("Hybride"), TEXT("moteur"), TEXT("Le moteur est roi : plus de pièces moteur, accélération et vitesse de pointe renforcées."), TEXT("Moteurs à forte part électrique : voitures plus courtes, gros pontons pour refroidir les batteries, aileron arrière bas et large.") },
	{ TEXT("aero-active"), TEXT("Aéro active"), TEXT("aero"), TEXT("L’aéro est reine : plus de pièces aéro, appui et aspiration renforcés."), TEXT("Ailerons mobiles et nez très long : quatre éléments à l’avant, pontons étroits, très grande dérive.") },
	{ TEXT("securite"), TEXT("Sécurité"), TEXT("fiab"), TEXT("La fiabilité est reine : plus de pièces de fiabilité, voitures solides, plus d’adhérence."), TEXT("Structures renforcées, voitures plus larges et plus lourdes, aileron avant très large et sans dérive.") },
};
static const TArray<FEraData>& EraData()
{
	static const TArray<FEraData> D = {
		{ {  }, {  } },
		{ { { TEXT("kd"), 1.3f }, { TEXT("mu"), 1.25f }, { TEXT("vtop"), 0.9f } }, { { TEXT("Plancher abaissé"), TEXT("La voiture frôle la piste : l’effet de sol est maximal."), { { TEXT("kd"), 1.0f }, { TEXT("mu"), 0.4f } } }, { TEXT("Suspension anti-pompage"), TEXT("Le fond reste à hauteur constante : l’appui ne décroche plus."), { { TEXT("kd"), 0.8f }, { TEXT("mu"), 0.5f } } }, { TEXT("Raideur de caisse pilotée"), TEXT("Plus d’appui en courbe rapide, sans rebonds."), { { TEXT("kd"), 0.7f }, { TEXT("steer"), 0.4f } } } } },
		{ { { TEXT("acc"), 1.35f }, { TEXT("vtop"), 1.25f }, { TEXT("fuel"), 1.3f }, { TEXT("kd"), 0.85f } }, { { TEXT("Batterie double"), TEXT("Deux fois plus d’énergie électrique à chaque tour."), { { TEXT("acc"), 1.0f }, { TEXT("vtop"), 0.3f } } }, { TEXT("Turbo électrique"), TEXT("Le turbo tourne avant même que le pilote accélère."), { { TEXT("vtop"), 0.7f }, { TEXT("acc"), 0.6f } } }, { TEXT("Mode dépassement"), TEXT("Un surplus de puissance électrique en ligne droite."), { { TEXT("vtop"), 1.0f } } } } },
		{ { { TEXT("slip"), 1.5f }, { TEXT("kd"), 1.2f }, { TEXT("vtop"), 1.05f }, { TEXT("tyre"), 1.2f } }, { { TEXT("Volets pilotés"), TEXT("Les ailerons changent d’angle à chaque virage."), { { TEXT("kd"), 0.8f }, { TEXT("vtop"), 0.4f } } }, { TEXT("Mode ligne droite"), TEXT("Les deux ailerons s’effacent dans les lignes droites."), { { TEXT("vtop"), 1.0f }, { TEXT("slip"), 0.4f } } }, { TEXT("Capteurs de flux"), TEXT("L’aéro s’adapte à l’air sale en temps réel."), { { TEXT("slip"), 0.8f }, { TEXT("kd"), 0.3f } } } } },
		{ { { TEXT("rel"), 1.3f }, { TEXT("wall"), 1.3f }, { TEXT("off"), 1.3f }, { TEXT("mu"), 1.15f }, { TEXT("vtop"), 0.95f } }, { { TEXT("Cellule de survie étendue"), TEXT("Les chocs ne cassent presque plus rien."), { { TEXT("wall"), 1.0f }, { TEXT("rel"), 0.4f } } }, { TEXT("Double circuit de freinage"), TEXT("Un deuxième circuit prend le relais en cas de panne."), { { TEXT("rel"), 0.6f }, { TEXT("brake"), 0.5f } } }, { TEXT("Plancher absorbant"), TEXT("Les sorties de piste ne coûtent presque plus de temps."), { { TEXT("off"), 1.0f }, { TEXT("wall"), 0.4f } } } } },
	};
	return D;
}
static const TMap<FString, TArray<FPoolPart>>& Pool()
{
	static const TMap<FString, TArray<FPoolPart>> P = {
		{ TEXT("moteur"), {
			{ TEXT("Chambre de combustion compacte"), TEXT("Une combustion plus rapide et plus complète."), { { TEXT("vtop"), 1.0f }, { TEXT("acc"), 0.5f } } },
			{ TEXT("Turbo à géométrie variable"), TEXT("De la puissance sans temps de réponse."), { { TEXT("vtop"), 1.0f } } },
			{ TEXT("Volant moteur allégé"), TEXT("Le moteur prend ses tours plus vite."), { { TEXT("acc"), 1.0f } } },
			{ TEXT("Batterie haute densité"), TEXT("Plus d’énergie stockée à chaque freinage."), { { TEXT("acc"), 0.8f }, { TEXT("vtop"), 0.3f } } },
			{ TEXT("Moteur électrique avant"), TEXT("Une poussée de plus en sortie de virage."), { { TEXT("acc"), 1.0f } } },
			{ TEXT("Cartographie qualification"), TEXT("Un mode tout pour la vitesse sur un tour."), { { TEXT("vtop"), 1.0f } } },
			{ TEXT("Refroidissement direct"), TEXT("Le moteur tourne plus frais : il consomme et casse moins."), { { TEXT("fuel"), 1.0f }, { TEXT("rel"), 0.6f } } },
			{ TEXT("Allumage à préchambre"), TEXT("Plus de rendement pour chaque goutte de carburant."), { { TEXT("fuel"), 0.8f }, { TEXT("vtop"), 0.4f } } },
			{ TEXT("Échappement accordé"), TEXT("Les gaz sortent mieux : plus de puissance en haut."), { { TEXT("vtop"), 1.0f } } },
			{ TEXT("Embrayage sans coupure"), TEXT("Des relances sans à-coups."), { { TEXT("acc"), 1.0f } } },
			{ TEXT("Récupération à l’échappement"), TEXT("La chaleur perdue devient de la puissance."), { { TEXT("acc"), 0.5f }, { TEXT("vtop"), 0.5f } } },
			{ TEXT("Bloc monobloc"), TEXT("Plus rigide, plus léger et plus fiable."), { { TEXT("rel"), 1.0f }, { TEXT("acc"), 0.3f } } },
			{ TEXT("Moteur nouvelle génération"), TEXT("Le sommet du savoir-faire de l’écurie."), { { TEXT("vtop"), 1.0f }, { TEXT("acc"), 0.6f } } },
		} },
		{ TEXT("aero"), {
			{ TEXT("Fond à canaux Venturi"), TEXT("L’air accélère sous la voiture et la plaque au sol."), { { TEXT("kd"), 1.0f } } },
			{ TEXT("Aileron avant épuré"), TEXT("Moins de turbulences pour la voiture qui suit."), { { TEXT("kd"), 0.5f }, { TEXT("slip"), 0.5f } } },
			{ TEXT("Pontons creusés"), TEXT("L’air file vers l’arrière sans freiner la voiture."), { { TEXT("vtop"), 1.0f } } },
			{ TEXT("Lame de fond flexible"), TEXT("Un fond qui se plaque à haute vitesse."), { { TEXT("kd"), 0.8f }, { TEXT("vtop"), 0.3f } } },
			{ TEXT("Aileron arrière cuillère"), TEXT("Peu de traînée pour les lignes droites."), { { TEXT("vtop"), 1.0f }, { TEXT("kd"), -0.3f } } },
			{ TEXT("Volets mobiles"), TEXT("L’aileron s’ouvre en ligne droite derrière un rival."), { { TEXT("slip"), 1.0f } } },
			{ TEXT("Déflecteurs de roues"), TEXT("Le sillage des pneus est canalisé."), { { TEXT("kd"), 0.7f } } },
			{ TEXT("Écopes de freins soufflées"), TEXT("Les freins sont mieux refroidis."), { { TEXT("brake"), 1.0f } } },
			{ TEXT("Diffuseur à triple étage"), TEXT("Énormément d’appui à l’arrière."), { { TEXT("kd"), 1.0f } } },
			{ TEXT("Bord de plancher sculpté"), TEXT("Le fond reste efficace même en virage."), { { TEXT("kd"), 0.8f } } },
			{ TEXT("Capot moteur affiné"), TEXT("Moins de surface frontale, plus de vitesse."), { { TEXT("vtop"), 0.8f }, { TEXT("kd"), 0.2f } } },
			{ TEXT("Sillage maîtrisé"), TEXT("La voiture souffre moins dans l’air sale."), { { TEXT("slip"), 0.6f }, { TEXT("kd"), 0.4f } } },
			{ TEXT("Concept aéro intégral"), TEXT("Toute la voiture pensée comme une seule aile."), { { TEXT("kd"), 1.0f }, { TEXT("vtop"), 0.4f } } },
		} },
		{ TEXT("chassis"), {
			{ TEXT("Suspension à tirants"), TEXT("Un centre de gravité plus bas."), { { TEXT("mu"), 1.0f } } },
			{ TEXT("Disques ventilés"), TEXT("Des freinages plus courts."), { { TEXT("brake"), 1.0f } } },
			{ TEXT("Crémaillère rapide"), TEXT("La voiture tourne plus vite."), { { TEXT("steer"), 1.0f } } },
			{ TEXT("Étriers six pistons"), TEXT("Un freinage plus fort et plus constant."), { { TEXT("brake"), 1.0f } } },
			{ TEXT("Composite tressé"), TEXT("Un châssis plus léger et plus rigide."), { { TEXT("acc"), 0.5f }, { TEXT("mu"), 0.6f } } },
			{ TEXT("Amortisseurs reliés"), TEXT("Les pneus restent collés à la piste."), { { TEXT("mu"), 0.7f }, { TEXT("tyre"), 0.6f } } },
			{ TEXT("Géométrie anti-plongée"), TEXT("La voiture reste à plat au freinage."), { { TEXT("brake"), 0.6f }, { TEXT("mu"), 0.5f } } },
			{ TEXT("Barres antiroulis réglables"), TEXT("Un équilibre ajusté à chaque circuit."), { { TEXT("steer"), 0.6f }, { TEXT("mu"), 0.5f } } },
			{ TEXT("Moyeux allégés"), TEXT("Moins de masse non suspendue."), { { TEXT("acc"), 0.4f }, { TEXT("steer"), 0.6f } } },
			{ TEXT("Répartiteur de freinage actif"), TEXT("Le freinage s’adapte à chaque virage."), { { TEXT("brake"), 0.8f }, { TEXT("tyre"), 0.3f } } },
			{ TEXT("Lest réparti"), TEXT("Le poids est placé au mieux."), { { TEXT("mu"), 0.8f } } },
			{ TEXT("Monocoque de nouvelle ère"), TEXT("Le châssis le plus abouti de l’écurie."), { { TEXT("mu"), 1.0f }, { TEXT("steer"), 0.5f }, { TEXT("brake"), 0.4f } } },
		} },
		{ TEXT("fiab"), {
			{ TEXT("Faisceaux renforcés"), TEXT("Moins de pannes électriques."), { { TEXT("rel"), 1.0f } } },
			{ TEXT("Cellule anti-intrusion"), TEXT("Les chocs coûtent moins de vitesse."), { { TEXT("wall"), 1.0f } } },
			{ TEXT("Patins de fond en titane"), TEXT("Moins de perte dans l’herbe et le gravier."), { { TEXT("off"), 1.0f } } },
			{ TEXT("Rapports renforcés"), TEXT("Des passages de rapports sûrs."), { { TEXT("rel"), 0.8f }, { TEXT("acc"), 0.2f } } },
			{ TEXT("Capteurs de surveillance"), TEXT("Les problèmes sont repérés avant la casse."), { { TEXT("rel"), 1.0f } } },
			{ TEXT("Radiateurs surdimensionnés"), TEXT("Le moteur ne surchauffe plus."), { { TEXT("rel"), 1.0f } } },
			{ TEXT("Ailerons renforcés"), TEXT("Les contacts abîment moins la voiture."), { { TEXT("wall"), 1.0f }, { TEXT("rel"), 0.3f } } },
			{ TEXT("Fond plat blindé"), TEXT("On perd moins de vitesse hors piste."), { { TEXT("off"), 1.0f } } },
			{ TEXT("Nez absorbeur"), TEXT("Les chocs à l’avant font moins de dégâts."), { { TEXT("wall"), 1.0f } } },
			{ TEXT("Banc d’endurance"), TEXT("Chaque pièce roule 5 000 km au banc."), { { TEXT("rel"), 1.0f }, { TEXT("off"), 0.3f } } },
			{ TEXT("Contrôle qualité total"), TEXT("Zéro défaut à la sortie de l’usine."), { { TEXT("rel"), 1.0f }, { TEXT("wall"), 0.5f } } },
		} },
		{ TEXT("stand"), {
			{ TEXT("Écrous captifs"), TEXT("Les roues se changent en un geste."), { { TEXT("pit"), 1.0f } } },
			{ TEXT("Couvertures pilotées"), TEXT("Des pneus prêts dès la sortie des stands."), { { TEXT("tyre"), 1.0f } } },
			{ TEXT("Logiciel de consommation"), TEXT("Chaque goutte est comptée."), { { TEXT("fuel"), 1.0f } } },
			{ TEXT("Cric à levée rapide"), TEXT("La voiture est levée en un éclair."), { { TEXT("pit"), 1.0f } } },
			{ TEXT("Analyse de l’usure"), TEXT("Des relais calculés au tour près."), { { TEXT("tyre"), 1.0f } } },
			{ TEXT("Feux de sortie automatiques"), TEXT("Plus d’hésitation au départ du box."), { { TEXT("pit"), 1.0f } } },
			{ TEXT("Simulateur de course"), TEXT("La stratégie est jouée mille fois avant le départ."), { { TEXT("tyre"), 0.5f }, { TEXT("fuel"), 0.5f } } },
			{ TEXT("Entraînement quotidien"), TEXT("Des mécaniciens réglés comme une horloge."), { { TEXT("pit"), 1.0f } } },
			{ TEXT("Mélange carburant optimisé"), TEXT("Le même plein pour plus de tours."), { { TEXT("fuel"), 1.0f } } },
			{ TEXT("Équipe de stand d’élite"), TEXT("La meilleure équipe de stand du plateau."), { { TEXT("pit"), 1.0f }, { TEXT("tyre"), 0.4f } } },
			{ TEXT("Ravitaillement éclair"), TEXT("Le plein en un éclair."), { { TEXT("pit"), 0.8f } } },
		} },
	};
	return P;
}
static const TMap<FString, float>& UnitMap() { static const TMap<FString, float> M = { { TEXT("vtop"), 0.011f }, { TEXT("acc"), 0.03f }, { TEXT("kd"), 0.055f }, { TEXT("mu"), 0.018f }, { TEXT("brake"), 0.06f }, { TEXT("steer"), 0.07f }, { TEXT("slip"), 0.35f }, { TEXT("rel"), 10.0f }, { TEXT("wall"), -0.2f }, { TEXT("off"), 0.12f }, { TEXT("tyre"), -0.05f }, { TEXT("fuel"), -0.05f }, { TEXT("pit"), -0.3f } }; return M; }
static const TMap<FString, float>& LevelMap() { static const TMap<FString, float> M = { { TEXT("moteur"), 1.25f }, { TEXT("aero"), 1.0f }, { TEXT("chassis"), 1.05f }, { TEXT("fiab"), 0.55f }, { TEXT("stand"), 0.6f } }; return M; }
static const TMap<FString, float>& PerfMap() { static const TMap<FString, float> M = { { TEXT("vtop"), 0.6f }, { TEXT("acc"), 0.15f }, { TEXT("kd"), 0.08f }, { TEXT("mu"), 0.25f }, { TEXT("brake"), 0.04f }, { TEXT("steer"), 0.04f } }; return M; }
