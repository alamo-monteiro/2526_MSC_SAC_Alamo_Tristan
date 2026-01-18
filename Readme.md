# Rapport d'Implémentation et Validation du Contrôle Moteur (STM32G474)

Ce document résume la configuration du Timer et l'implémentation de la commande de vitesse par Shell pour le contrôle d'un moteur triphasé (MCC/FOC).

---

On branche la carte, et on teste le shell.

<img width="518" height="169" alt="CaptureEcranTerminal" src="https://github.com/user-attachments/assets/e3bff7c4-c5d1-4184-9334-f0620aea72f7" />


On teste les caractères possibles et les différentes fonctionnalités déjà implémentées.
## 1. Configuration du Timer (TIM1) et du PWM

L’objectif est de configurer **TIM1** pour générer un **PWM à 20 kHz** destiné aux bras de l’onduleur, à partir d’une horloge timer de **170 MHz**.

### 1.1 Calcul du couple PSC / ARR

Formule utilisée (timer en comptage classique) :

`f_PWM = TIMclk / ((PSC + 1) * (ARR + 1))`

Avec :
- `TIMclk = 170 MHz`
- `f_PWM = 20 kHz`

Donc :

`(PSC + 1) * (ARR + 1) = 170e6 / 20e3 = 8500`

Choix simple et propre :
- `PSC = 0`
- `ARR = 8499`  → résolution PWM = `ARR + 1 = 8500` pas

> Résolution équivalente ≈ log2(8500) ≈ 13 bits (largement > 10 bits).

### 1.2 Horloge interne TIM1

Sur STM32G474, TIM1 est typiquement cadencé à :
- `TIM1 clock = 170 MHz`
- `CKD = 1`

On obtient un pas temporel :

`t_DTS ≈ 1 / 170e6 ≈ 5.882 ns`

### 1.3 Réglage du duty cycle (Pulse)

Dans **CubeMX → Parameter Settings → PWM Generation CH1/CH2**, la valeur `Pulse` correspond à :

`CCR = duty * (ARR + 1)`

Pour un duty initial à **60%** :

`CCR = 0.6 * 8500 = 5100`

✅ Valeur appliquée :
- `Pulse (16-bit value) = 5100`


### 1.1. Recap des Paramètres Calculés

| Paramètre | Valeur | Justification |
| :--- | :--- | :--- |
| **Fréquence Clock ($f_{TIM}$)** | $170 \text{ MHz}$ | Classique sur G474. |
| **Fréquence PWM ($f_{PWM}$)** | $20 \text{ kHz}$ | Conforme au cahier des charges. |
| **ARR (Auto-Reload Register)** | $\mathbf{8499}$ | Calculé pour $\frac{170 \times 10^6}{20 \times 10^3} = 8500$. |
| **Résolution PWM** | $8500 \text{ pas}$ | $\approx 13$ bits de précision. |
| **Conception du Pulse (60%)** | $5100$ | $0.6 \times (8499+1)$. |

---

## 2. Validation des signaux PWM (oscilloscope)

Une fois les PWM générés, nous avons vérifié leur comportement à l’oscilloscope sur les sorties :
- **CH1 / CH1N** (bras **U**)
- **CH2 / CH2N** (bras **V**)

<img width="800" height="480" alt="tek00003" src="https://github.com/user-attachments/assets/2ce35fbc-7082-4c82-be70-cbba5f1809de" />

<img width="800" height="480" alt="tek00002" src="https://github.com/user-attachments/assets/5d56fa61-3f7d-4caf-a7a3-38229f9de34a" />

### 2.1 Résultats observés

D’après les captures :

- **Fréquence mesurée : 20.00 kHz**  
  → conforme au cahier des charges.

- **Rapports cycliques mesurés :**
  - une voie ≈ **58 %**
  - l’autre ≈ **38 %**

  → c’est cohérent : ce sont les deux signaux **complémentaires**.  
  On observe que `58% + 38% ≈ 96%` : l’écart restant correspond au **dead-time**.  
  Pour une consigne théorique de **60%**, obtenir **~58%** est normal (quantification due à `ARR` fini + dead-time).

- **Dead-time mesuré : 1.000 µs**  
  → exactement la valeur visée.

### 2.2 Validation de la sécurité (shoot-through)

La sécurité du pont en H est validée : visuellement on constate que :
- les deux signaux (principal et complémentaire) ne sont **jamais à l’état haut en même temps** ;
- on observe un petit “trou” entre les commutations, ce qui confirme l’insertion effective du **dead-time de 1.000 µs** et empêche le **shoot-through**.



### 2.3. Mesures Globales Récap

| Mesure | Valeur Relevée | Statut |
| :--- | :--- | :--- |
| **Fréquence** | $\mathbf{20.00 \text{ kHz}}$ | ✅ **OK** (Confirme le cahier des charges). |
| **Rapports Cycliques** | $\approx 58\%$ et $\approx 38\%$ | ✅ **Cohérent** (La somme $\approx 96\%$, le reste est le *Dead Time*). |
| **Dead Time** (Temps Mort) | $\mathbf{1.000 \ \mu \text{s}}$ | ✅ **OK** (Exactement la valeur visée pour la sécurité). |

## 3. Implémentation de la commande `speed` (pilotage PWM)

Cette étape consiste à implémenter une commande shell `speed` permettant de fixer une consigne (0–1000) convertie en **duty cycle PWM** appliqué au moteur.

### 3.1 Chaîne de traitement côté shell

La réception UART est gérée caractère par caractère via l’interruption :

- `HAL_UART_RxCpltCallback()` reçoit les caractères
- puis appelle `shell_run(&hshell1);`

Le shell reconstruit ensuite la ligne tapée (ex : `speed 600`) et :

1. **Tokenise** la commande :
   - `argv[0] = "speed"`
   - `argv[1] = "600"`

2. **Recherche** la commande dans la table enregistrée par :

c
shell_add(&hshell1, "speed", sh_speed, "set motor speed: speed 0-1000");

### 3.2 Conversion de l’argument speed en PWM

Dans la fonction `sh_speed`, le programme récupère `argv[1]` (la chaîne de caractères "XXXX") et applique la logique suivante :

1. **Conversion** : `strtol()` $\rightarrow$ entier.
2. **Saturation** : contrainte de la valeur dans l'intervalle `[0, MOTOR_SPEED_CMD_MAX]`.
3. **Conversion PWM** : calcul du pourcentage.
4. **Application** : commande du moteur.

**Exemples de comportement :**

| Commande | `raw_value` | `duty_percent` | Conséquence Physique |
| :--- | :--- | :--- | :--- |
| `speed 0` | 0 | **0%** | Arrêt. |
| `speed 500` | 500 | **50%** | Bras U ≈ 50% et bras V ≈ 50%. <br> Tension moyenne U - V ≈ 0V (Freinage / Couple faible). |
| `speed 1000` | 1000 | **100%** | Vitesse maximale. |
| `speed 1500` | 1000 **(Saturé)** | **100%** | La valeur est plafonnée à `MOTOR_SPEED_CMD_MAX`. |

### 3.3 Application du duty-cycle : de `%` vers les registres du timer (TIM1)

La fonction `motor_set_duty_percent()` applique le rapport cyclique calculé aux registres de comparaison du timer.

- La valeur `ARR` fixe la **résolution** de la PWM (ici `ARR = 8499` → `8500` pas).
- Pour un duty de 60%, on obtient typiquement :

  - `CCR ≈ 0.6 × (ARR + 1)`  
  - de manière générale : `CCR = duty_percent × (ARR + 1) / 100` (avec saturation entre `0` et `ARR`)

Le timer **TIM1** est configuré à **20 kHz** avec sorties complémentaires et **deadtime** :

- `CH1/CH1N` pilote le **bras U**
- `CH2/CH2N` pilote le **bras V**
- Le rapport cyclique sur ces sorties correspond directement au `duty_percent` appliqué.

---

### 3.4 Problème observé : démarrage brutal à forte consigne

Lorsque l’on envoie directement une consigne élevée (ex : `speed 800` ou `speed 1000`) alors que le moteur est à l’arrêt :

- le duty-cycle passe brutalement à une grande valeur,
- **le courant de démarrage devient très élevé** (à l’arrêt, la f.é.m. du moteur est nulle, le courant est surtout limité par les paramètres électriques du moteur),
- le démarrage est **brutal** (à-coups mécaniques, vibrations, bruit),
- on peut observer une **chute de tension** ou l’activation de **protections** (alimentation / puissance).

Ce comportement est donc **peu contrôlé** et potentiellement agressif pour le moteur et l’électronique de puissance.

---

### 3.5 Solution : rampe de duty-cycle (soft-start)

Pour éviter un saut instantané de `0%` vers la consigne, on met en place une **rampe** :

- le duty-cycle augmente par petits pas à chaque période de contrôle,
- jusqu’à atteindre la valeur cible.

Cela permet :

- de limiter le **courant d’appel**,
- d’adoucir la mise en vitesse,
- d’améliorer la sécurité et le confort de fonctionnement.

### 3.6 Mise en place d’une rampe (soft-start) dans le code

#### Principe général

La commande `speed XXXX` ne modifie plus directement les registres de PWM. Elle :

- convertit `XXXX` (0 à 1000) en un duty-cycle **cible** en pourcentage (0 à 100 %),
- stocke cette consigne dans une variable globale :  
  `motor_target_duty_percent`

Une seconde variable mémorise la valeur réellement appliquée à l’instant courant :  
`motor_current_duty_percent`

#### Mise à jour progressive

Une fonction `motor_update()` est appelée régulièrement dans la boucle principale (`while(1)` dans `main.c`).

Son rôle est de faire converger progressivement `motor_current_duty_percent` vers `motor_target_duty_percent` :

- si `target > current` : on augmente `current` d’un petit pas (ex : `+5 %`)
- si `target < current` : on diminue `current` d’un petit pas (ex : `-5 %`)
- on impose un délai entre deux pas via l’horloge système (`HAL_GetTick()`)

À chaque mise à jour de `motor_current_duty_percent`, la fonction recalcule les valeurs `CCR` des deux canaux PWM (bras U et bras V) en tenant compte du pilotage différentiel / complémentaire :
```c
ccr_u = (ARR + 1) * current / 100;           // bras U
```
```c
ccr_v = (ARR + 1) * (100 - current) / 100;   // bras V
```

#### Paramétrage de la rampe

Dans notre cas, nous avons choisi :

* Un pas de **5 %** :
    ```c
    const uint32_t step_percent = 5U;
    ```
* Un intervalle de **100 ms** entre deux pas :
    ```c
    const uint32_t step_delay_ms = 100U;
    ```

Ainsi, une montée de **0 % à 80 %** prend environ :

$$\frac{80}{5} = 16 \text{ pas}$$

$$16 \times 100\text{ ms} = 1.6\text{ s}$$

> **Note :** Ce réglage est suffisamment lent pour être clairement visible et permet de limiter le courant de démarrage (*soft-start*).

## Mesure de courant

### 1) Quel(s) courant(s) doit-on mesurer ?

À partir du schéma KiCad (bloc **Hall current**, composant **U601 – GO 10-SME/SP3**) :

- Les broches **IP+ / IP−** sont insérées *en série* entre **V_Bus_In** et **V_Bus_Out**.
- Le courant qui traverse le capteur est donc **le courant du bus DC** : **Ibus** (courant consommé/fourni par l’onduleur côté alimentation).

>  Courant mesuré par ce bloc : **Ibus (courant bus DC)**

Illustration (extrait schéma KiCad) :  
<img width="868" height="768" alt="Hall current - KiCad" src="https://github.com/user-attachments/assets/da2e7308-4b84-41a3-9e96-8e48b169dc4c" />

---

### 2) Fonction de transfert du capteur (datasheet)

Le capteur **GO 10-SME/SP3** est alimenté en **3.3 V isolé** (**ISO_3.3V / ISO_GND**) et fournit :

- **Uref** : tension de référence (typiquement ~1.65 V, soit Vcc/2)
- **Imes** : tension image du courant (sortie analogique)
- Sensibilité nominale : **S ≈ 50 mV/A**

La relation donnée par la datasheet est :

```text
U(Imes) - U(Uref) = S * Ibus      avec S ≈ 0.05 V/A
```

<img width="401" height="108" alt="image" src="https://github.com/user-attachments/assets/5cacaa90-0a0f-44d0-a941-d79a13614eeb" />

Pour traiter cette information dans le STM32, nous devons tenir compte de l'étape de **numérisation**. Le microcontrôleur convertit la tension analogique en un nombre binaire sur **12 bits** (allant de $0$ à $4095$).

* **Résolution de l'ADC :** $2^{12} = 4096$ pas.

La tension lue par l'ADC est définie par la relation : $V = \frac{N_{ADC} \times 3,3}{4096}$.

En intégrant la sensibilité du capteur et le décalage de la tension de référence ($U_{ref}$), la fonction de transfert numérique pour obtenir le courant en Ampères est :

$$I_{bus} = \frac{(N_{Imes} - N_{Uref}) \times 3,3}{4096 \times 0,05}$$

Où :
* **$N_{Imes}$** : Valeur numérique lue sur le canal ADC correspondant à la sortie du capteur.
* **$N_{Uref}$** : Valeur numérique de la tension de référence (offset du zéro).
* **$4096$** : Représente la quantification totale sur 12 bits.

### 3) Côté MCU : quelles pins STM32 / ADC reçoivent les mesures ?

D’après le schéma **“MCU Side”** (liaison carte puissance → Nucleo-G474RE), les signaux analogiques de mesure arrivent sur les entrées suivantes :

<img width="1211" height="745" alt="MCU Side - Nucleo connections" src="https://github.com/user-attachments/assets/a57d6f47-238a-465e-b39d-ea752db2b326" />

On lit directement sur le schéma :

- `Bus_Imes` → **PC2** (connecteur **J103**, pin **36**)  
- `U_Imes` → **PA1**  
- `V_Imes` → **PB1** (connecteur **J104**)  
- `W_Imes` → **PB0**  
- (tension bus) `Bus_V` → **PA0**

---

### 4) Courants mesurés

Le système permet de mesurer plusieurs courants (bus + phases) :

- **Courant de bus DC** : `Bus_Imes`  
- **Courant phase U** : `U_Imes`  
- **Courant phase V** : `V_Imes`  
- **Courant phase W** : `W_Imes`

---

### 5) Récapitulatif des pins STM32 utilisées (Nucleo-G474RE)

| Grandeur mesurée | Net KiCad | Pin STM32 |
|---|---|---|
| Courant bus DC | `Bus_Imes` | **PC2** |
| Courant phase U | `U_Imes` | **PA1** |
| Courant phase V | `V_Imes` | **PB1** |
| Courant phase W | `W_Imes` | **PB0** |
| Tension bus (info) | `Bus_V` | **PA0** |


### 6) Acquisition du courant via ADC en DMA (2 canaux)

La mesure de l’ADC en **polling** s’est avérée peu exploitable dans notre cas (comportement instable / difficile à synchroniser). Nous sommes donc passés directement à une acquisition **ADC + DMA** sur **2 canaux** (phases U et V), avec conversion régulière.

#### 6.1 Principe

L’objectif initial est simple : **mesurer et afficher les courants de phase `Iu` et `Iv`**.

- Lecture des deux voies ADC (`U_Imes` et `V_Imes`) via **DMA** dans un buffer circulaire.
- Conversion des valeurs brutes en tension, puis en courant via la fonction de transfert (capteur centré autour de `Uref ≈ 1.65 V`).
- Affichage dans le shell :
  - `adcraw` → affiche les valeurs brutes DMA
  - `ibus` / `current` → affiche les courants calculés (mA)

#### 6.2 Observations expérimentales (résultats bruts + courants)

> **Remarque :** avant de démarrer la PWM (`start`), les valeurs DMA sont à zéro et le courant calculé est incohérent (ex : `-33000 mA`). Dès que la PWM est active, les mesures deviennent réalistes.

| Condition | Commande | ADC raw (ch0/ch1) | Courants affichés |
|---|---|---:|---:|
| Avant `start` | `adcraw` / `ibus` | `0 / 0` | `-33000 / -33000 mA` |
| PWM activée (0 speed) | `start` puis `adcraw` / `ibus` | `1975 / 1980` | `177 / 161 mA` |
| PWM 20% | `speed 200` | `2063 / 1958` | `1369 / 290 mA` |
| PWM 30% | `speed 300` | `2083 / 1990` | `999 / 290 mA` |
| PWM 70% | `speed 700` | `1908 / 1963` | `-1547 / -145 mA` |

#### 6.3 Interprétation et diagnostic (ADC/DMA + offset)

Très vite, on a observé des valeurs anormales, par exemple :

- `-33000 mA` (≈ **-33 A**)
- ou encore des valeurs de l’ordre de **1–2 A au repos**

On s’est donc demandé si le problème venait :

- d’une **mauvaise conversion** (fonction de transfert)
- d’un **ADC/DMA qui ne tourne pas**
- d’un **offset** (capteur / conditionnement / ADC)
- ou d’un **bruit lié au PWM**

---

### 6.3.1 Diagnostic : ajout de `adcraw` (lecture brute DMA)

Pour savoir si le problème venait de la conversion ou de l’acquisition, on a ajouté une commande `adcraw` afin d’afficher **directement les valeurs brutes** du buffer DMA.

**Exemples observés :**

- Avant `start` : `ch0 = 0 | ch1 = 0`
- Après `start` : valeurs autour de `~1980–2000`

**Conclusion clé :**

Quand `adcraw = 0 / 0`, la conversion donne forcément :

<img width="306" height="83" alt="image" src="https://github.com/user-attachments/assets/d0ed69e3-e117-4a95-ba7e-8d2e52828da2" />

Donc l’erreur `-33000 mA` n’était pas un phénomène “physique” : c’était le symptôme que l’ADC **n’échantillonnait pas** (cas typique : ADC déclenché par un timer / TRGO pas encore actif, ou DMA non lancé avant la séquence `start`).

---

### 6.3.2 Deuxième observation : offset non nul même quand l’ADC fonctionne

Une fois l’ADC “vivant” (raw ≈ 2000), on a constaté que même sans courant “attendu”, `ibus` affichait par exemple :

- `Iu ≈ 1.6 A`
- `Iv ≈ -0.2 A`

Ce type d’erreur est typique d’un **offset** :

- capteur pas exactement centré à `Uref = 1.65 V`
- offset analogique (filtre / ampli / conditionnement)
- offset ADC (erreurs de gain/offset)
- bruit PWM + couplages (mesure non parfaitement synchrone / parasites)

Le professeur avait justement indiqué qu’on pouvait ajouter un **terme correctif** : cela nous a menés à mettre en place une **calibration à 0 A** via la commande `cal0`, afin de recalculer automatiquement l’offset et recentrer la mesure autour de `Uref`.
### 6.4 Mise en place de la correction : calibration du zéro (commande `cal0`)

Plutôt que “deviner” un correctif à la main, on a mis en place une procédure de calibration simple et reproductible.

#### 6.4.1 Principe

L’idée est la suivante :

- À **courant nul**, on mesure la **tension réelle** délivrée par le capteur (moyennée sur plusieurs échantillons).
- On calcule ensuite un **offset** `corr_v_offset` afin de forcer la condition :

`Umes + corr_v_offset = Uref`

Ainsi, la conversion courant est recentrée correctement autour de la référence.

---

#### 6.4.2 Commande `cal0`

On a ajouté la commande :

- `cal0 N` : moyenne sur **N échantillons** → calcule un offset (en mV) → applique cet offset dans la conversion.

Concrètement, on utilise **deux offsets indépendants**, car les deux voies ADC peuvent avoir des erreurs différentes :

- `corr_v_offset_u` pour la voie U
- `corr_v_offset_v` pour la voie V

---

#### 6.4.3 Validation expérimentale : effet immédiat de `cal0`

Une fois la PWM lancée (`start`), on observe :

**Avant calibration :**
- `ibus : Iu = 1676 mA | Iv = -241 mA`

**Calibration :**
- `cal0 200`
- offsets calculés : `offU ≈ 53 mV`, `offV ≈ 56 mV`

**Après calibration :**
- `ibus : Iu = -16 mA | Iv = -48 mA`

**Résultat :** le “zéro courant” est ramené proche de **0 mA**, donc la correction d’offset fonctionne.

---

#### 6.4.4 Interprétation

La tendance est correcte : l’offset est bien compensé.

L’erreur initiale vient du fait que, même si la fonction de transfert du capteur est correcte, la valeur numérique associée à `Uref` (**NUref**) n’est pas exactement à mi-échelle (2048) en pratique, à cause de :

- l’offset intrinsèque du capteur,
- les décalages/offsets du front-end analogique,
- la valeur réelle de `VDDA` (pas exactement 3.3 V),
- les erreurs internes de l’ADC.

D’où l’intérêt d’ajouter un **terme correctif** logiciel, via deux offsets séparés (`corr_v_offset_u` et `corr_v_offset_v`) adaptés à chaque canal.

On affiche adcraw + ibus/current

Une fois la fonction de calibration implémentée : 

=> Monsieur Shell v0.2.2 without FreeRTOS <=
MSC@SAC-TP:/start
start: PWM enabled at 50% (zero speed, calib en cours)
MSC@SAC-TP:/ibus
Iu = 1676 mA | Iv = -241 mA
MSC@SAC-TP:/adcraw
ADC raw: ch0=1991 | ch1=1982
MSC@SAC-TP:/cal0 200
cal0 ok (N=200) offU=53mV offV=56mV
MSC@SAC-TP:/ibus
Iu = -16 mA | Iv = -48 mA
MSC@SAC-TP:/adcraw
ADC raw: ch0=1987 | ch1=1981
MSC@SAC-TP:/speed 200
speed set: cmd=200 (max=1000) -> duty=20%
MSC@SAC-TP:/ibus
Iu = 1289 mA | Iv = -64 mA
MSC@SAC-TP:/adcraw
ADC raw: ch0=2070 | ch1=2002
MSC@SAC-TP:/speed 300
speed set: cmd=300 (max=1000) -> duty=30%
MSC@SAC-TP:/ibus
Iu = 1095 mA | Iv = 322 mA
MSC@SAC-TP:/speed 700
speed set: cmd=700 (max=1000) -> duty=70%
MSC@SAC-TP:/ibus
Iu = -1112 mA | Iv = -96 mA
MSC@SAC-TP:/


<img width="656" height="643" alt="image" src="https://github.com/user-attachments/assets/0f08f765-e637-4f44-8937-59d007501993" />



Avant cal0 : On a un gros offset

adcraw ~ 1991 / 1982 → ça correspond à ~1.60 V (normal autour de 1.65 V)

mais ibus donne Iu = 1676 mA (≈ 1.7 A) alors qu'on est à “zéro vitesse”
Ça veut dire : le “zéro capteur” n’est pas exactement à 1.65 V, donc la formule (Umes - 1.65)/0.05 nous sort un faux courant.

al0 200 : l’offset trouvé est réaliste

offU=53mV, offV=56mV
50 mV d’erreur sur un capteur à 0.05 V/A = 1 A d’erreur potentielle.
Donc ça colle avec les ~1.7 A affichés avant correction

Après cal0 : le zéro est (presque) bon

Iu = -16 mA, Iv = -48 mA
(quelques dizaines de mA d’erreur résiduelle = bruit + quantification ADC + offset restant).

Quand on change le duty : on voit des courants “signés”
à speed 200 / 300 : Iu devient positif (~1.1–1.3 A)
à speed 700 : Iu devient négatif (~ -1.1 A)
