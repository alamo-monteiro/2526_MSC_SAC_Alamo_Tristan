On branche la carte, et on teste le shell.

<img width="518" height="169" alt="CaptureEcranTerminal" src="https://github.com/user-attachments/assets/e3bff7c4-c5d1-4184-9334-f0620aea72f7" />


On teste les caractères possibles et les différentes fonctionnalités déjà implémentées.

Timer 1 déjà initialisé
clock à 170MHz 

Si TIMclk = 170 MHz et f_PWM = 20 kHz ⇒
 (PSC+1)∗(ARR+1)=170e6/20e3=8500
Choix simple et propre :
PSC = 0
ARR = 8499 → résolution = ARR+1 = 8500 (~13 bits, > 10 bits)
On a TIM1 clock = 170 MHz (classique sur G474), CKD = 1 → tDTS ≈ 5.882 ns.

Toujours dans Parameter Settings → PWM Generation Channel 1/2 :
Pulse (16-bit value) = 0.6 × (ARR+1) = 0.6 × 8500 = 5100

On génère les PWM

<img width="800" height="480" alt="tek00003" src="https://github.com/user-attachments/assets/2ce35fbc-7082-4c82-be70-cbba5f1809de" />

<img width="800" height="480" alt="tek00002" src="https://github.com/user-attachments/assets/5d56fa61-3f7d-4caf-a7a3-38229f9de34a" />

D'après la capture : 

Fréquence : 20.00 kHz → pile le cahier des charges

Rapports cycliques :

Une voie à ~58 %

L’autre à ~38 %
→ c’est cohérent : ce sont les deux complémentaires (58 % + 38 % ≈ 96 %, le reste étant le deadtime). Pour un “60 %” de consigne, 58 % c’est normal avec la quantification (ARR fini)

Deadtime : la mesure en bas indique 1.000 µs → exactement ce qu’on visait
Visuellement, on voit bien :

les deux signaux ne sont jamais hauts en même temps,

un petit “trou” entre les fronts → pas de recouvrement, donc pas de shoot-through.



Implémentation de la commande speed dans motor.c

Le shell détecte la commande "speed" .

On récupère argv[1] = "XXXX" et on fait strtol → entier.

On sature à [0, MOTOR_SPEED_CMD_MAX].

On convertit ça en % de PWM puis on applique au moteur.


On fait un complémentaire décalé pour faire tourner la MCC


Traitement par le shell

HAL_UART_RxCpltCallback() reçoit les caractères → appelle shell_run(&hshell1);

Le shell reconstruit la ligne "speed 600" puis :

découpe en tokens : argv[0] = "speed", argv[1] = "600"

trouve la commande "speed" enregistrée par :

shell_add(&hshell1, "speed", sh_speed, "set motor speed: speed 0-1000");

appelle alors la fonction :

sh_speed(&hshell1, argc, argv);

Fonction sh_speed : de XXXX → % PWM

speed 0 → raw_value = 0 → duty_percent = 0 %

speed 500 → duty = 50 % → U ≈ 50 %, V ≈ 50 % → moyenne ≈ 0 V (quasi freinage)

speed 1000 → raw_value = 1000 → duty_percent = 100 %

speed 1500 → saturé à 1000 → duty_percent = 100 %

On a bien 4 PWM actives : CH1/CH1N, CH2/CH2N,

chaque bras est complémentaire avec deadtime,
et les deux bras sont “décalés” l’un par rapport à l’autre → la tension U−V est vraiment modulée.

Fonction motor_set_duty_percent : % → registres du timer

ARR donne la résolution de notre PWM (ici 8499 → 8500 pas).

Pour un duty de 60 %, on obtient grosso modo CCR ≈ 0,6 × (ARR+1).

Le timer TIM1 étant configuré à 20 kHz, avec sorties complémentaires + deadtime :

TIM1 génère un signal PWM sur CH1/CH1N (bras U) et CH2/CH2N (bras V),

le rapport cyclique sur ces sorties correspond au duty_percent calculé.

Quels problèmes observez vous ?


Quand on envoie directement une consigne de vitesse élevée (par exemple speed 800 ou speed 1000), le moteur part d’un état arrêté.
Le rapport cyclique de la PWM passe brutalement à une grande valeur, ce qui provoque :

un courant de démarrage très important (le moteur se comporte presque comme une charge résistive au début, donc I ≈ V/R),

un démarrage très brutal du moteur (à-coups mécaniques, vibrations, bruit),

parfois une chute de tension ou l’activation des protections du hacheur / de l’alimentation.

On observe donc un comportement peu contrôlé et agressif pour le moteur et l’électronique lorsqu’on applique directement une grande consigne de PWM.

Pour pallier à ce problème, on met en place une montée progressive du rapport cyclique (rampe) : au lieu de passer instantanément de 0 % à la valeur de consigne, on augmente le duty-cycle par petits pas jusqu’à atteindre la valeur cible.
Cela limite le courant de démarrage, adoucit la mise en vitesse du moteur et améliore le confort et la sécurité de fonctionnement.

   

Principe général

La commande speed XXXX ne modifie plus directement les registres de PWM.
Elle :

convertit XXXX (0 à 1000) en un duty-cycle cible en pourcentage (0 à 100 %),

stocke cette consigne dans une variable globale :

motor_target_duty_percent

Une seconde variable mémorise la valeur réellement appliquée à l’instant courant :

motor_current_duty_percent


Une fonction motor_update() est appelée régulièrement dans la boucle principale (while(1) dans main.c).

Son rôle est de faire converger progressivement motor_current_duty_percent vers motor_target_duty_percent :

Si target > current : on augmente current d’un petit pas (par exemple +5 %),

Si target < current : on diminue current d’un petit pas (par exemple –5 %),

On attend un certain temps entre deux pas grâce à l’horloge système (HAL_GetTick()).

À chaque fois que motor_current_duty_percent est mis à jour, la fonction recalcule les valeurs CCR des deux canaux PWM (bras U et bras V) en tenant compte du fonctionnement complémentaire :

ccr_u = (ARR + 1) * current / 100      // bras U
ccr_v = (ARR + 1) * (100 - current) / 100  // bras V

Paramétrage de la rampe

Dans notre cas, nous avons choisi :

un pas de 5 % :

const uint32_t step_percent = 5U;

un intervalle de 100 ms entre deux pas :

const uint32_t step_delay_ms = 100U;

Ainsi, une montée de 0 % à 80 % prend environ :

80/5=16 pas,

16×100 ms=1,6 s,

ce qui est suffisamment lent pour être clairement visible et pour limiter le courant de démarrage.



<img width="868" height="768" alt="image" src="https://github.com/user-attachments/assets/da2e7308-4b84-41a3-9e96-8e48b169dc4c" />

Le bloc “Hall current” avec le composant U601 – GO 10-SME/SP3

À gauche : V_Bus_In / V_Bus_Out sur les pins IP+ / IP− → c’est le courant bus DC qui traverse le capteur.

À droite :

Imes = sortie Uout

Uref = sortie Uref

ISO_3.3V et ISO_GND → alim isolée 3,3 V du capteur

En bas, les condos C601, C602, C603 (4,7 nF) font juste du filtrage sur Imes, Uref et l’alim.

Quel courant on mesure ? → le courant de bus DC (entre V_Bus_In et V_Bus_Out) via ce capteur Hall GO 10-SME/SP3.

✅ Capteur / fonction de transfert : c’est ce GO 10-SME/SP3, dont la datasheet donne :

Alim : 3,3 V,

Référence : Uref ≈ 1,65 V,

Sensibilité nominale : 50 mV/A.

La relation est donc du type :

<img width="584" height="63" alt="image" src="https://github.com/user-attachments/assets/8a1e7793-51b6-4a97-a947-c93c2247a544" />

<img width="401" height="108" alt="image" src="https://github.com/user-attachments/assets/5cacaa90-0a0f-44d0-a941-d79a13614eeb" />



#### 2. Fonction de transfert - Numérisation (ADC)
Pour traiter cette information dans le STM32, nous devons tenir compte de l'étape de **numérisation**. Le microcontrôleur convertit la tension analogique en un nombre binaire sur **12 bits** (allant de $0$ à $4095$).

**Paramètres du système :**
* **Sensibilité du capteur ($S$) :** $50\text{ mV/A}$ (soit $0,05\text{ V/A}$).
* **Résolution de l'ADC :** $2^{12} = 4096$ pas.
* **Plage de tension :** $0$ à $3,3\text{ V}$.

La tension lue par l'ADC est définie par la relation : $V = \frac{N_{ADC} \times 3,3}{4096}$.

En intégrant la sensibilité du capteur et le décalage de la tension de référence ($U_{ref}$), la fonction de transfert numérique pour obtenir le courant en Ampères est :

$$I_{bus} = \frac{(N_{Imes} - N_{Uref}) \times 3,3}{4096 \times 0,05}$$

Où :
* **$N_{Imes}$** : Valeur numérique lue sur le canal ADC correspondant à la sortie du capteur.
* **$N_{Uref}$** : Valeur numérique de la tension de référence (offset du zéro).
* **$4096$** : Représente la quantification totale sur 12 bits.


<img width="1211" height="745" alt="image" src="https://github.com/user-attachments/assets/a57d6f47-238a-465e-b39d-ea752db2b326" />

Hall current → dit quel courant est mesuré et comment (capteur Hall GO 10-SME/SP3, nets Imes / Uref).

MCU Side → explique où ça arrive sur le Nucleo (quelles pins STM32 / ADC).

on lit :

Bus_Imes → va sur PC2 (J103, pin 36)

Bus_V → PA0

U_Imes → PA1

W_Imes → PB0

V_Imes → PB1 (sur le connecteur J104)

Courants mesurés

Courant de bus DC : Bus_Imes

Courant phase U : U_Imes

Courant phase V : V_Imes

Courant phase W : W_Imes

Pins STM32 utilisées pour la mesure de courant (côté Nucleo-G474RE)

Bus_Imes → pin PC2

U_Imes → pin PA1

V_Imes → pin PB1

W_Imes → pin PB0



<img width="335" height="298" alt="image" src="https://github.com/user-attachments/assets/1e8fb000-445f-4b8f-b647-2ba05b3d144c" />

On désactive tous les channels pour ne pas bloquer la mesure de l'ADC en pulling



Acquisition courant via ADC en mode DMA (2 canaux, conversion régulière)

Voici les différentes commandes effectués avec les résultats ci-dessous.

=> Monsieur Shell v0.2.2 without FreeRTOS <=
MSC@SAC-TP:/ibus
Iu = -33000 mA | Iv = -33000 mA
MSC@SAC-TP:/adcraw
ADC raw: ch0=0 | ch1=0
MSC@SAC-TP:/start
start: PWM enabled at 50% (zero speed, calib en cours)
MSC@SAC-TP:/ibus
Iu = 177 mA | Iv = 161 mA
MSC@SAC-TP:/adcraw
ADC raw: ch0=1975 | ch1=1980
MSC@SAC-TP:/speed 200
speed set: cmd=200 (max=1000) -> duty=20%
MSC@SAC-TP:/ibus
Iu = 1369 mA | Iv = 290 mA
MSC@SAC-TP:/adcraw
ADC raw: ch0=2063 | ch1=1958
MSC@SAC-TP:/speed 300
speed set: cmd=300 (max=1000) -> duty=30%
MSC@SAC-TP:/ibus
Iu = 999 mA | Iv = 290 mA
MSC@SAC-TP:/adcraw
ADC raw: ch0=2083 | ch1=1990
MSC@SAC-TP:/speed 700
speed set: cmd=700 (max=1000) -> duty=70%
MSC@SAC-TP:/ibus
Iu = -1547 mA | Iv = -145 mA
MSC@SAC-TP:/adcraw
ADC raw: ch0=1908 | ch1=1963

La tendance est bonne mais pas l'échelle des ampères.
En effet, la fonction de transfert est bonne, mais NUref n’est pas exactement à mi-échelle à cause des offsets capteur + analog front-end + VDDA réel
Donc on ajoute un terme correctif 

