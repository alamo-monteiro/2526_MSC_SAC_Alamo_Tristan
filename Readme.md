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

