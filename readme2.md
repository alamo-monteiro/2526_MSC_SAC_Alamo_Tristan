# ⚙️ Rapport d'Implémentation et Validation du Contrôle Moteur (STM32G474)

Ce document résume la configuration du Timer et l'implémentation de la commande de vitesse par Shell pour le contrôle d'un moteur triphasé (MCC/FOC).

---

## 1. Configuration du Timer (TIM1) et du PWM

La configuration du Timer 1 est basée sur la fréquence système de $170 \text{ MHz}$ pour générer une fréquence PWM de $20 \text{ kHz}$ pour les bras de l'onduleur.

### 1.1. Paramètres Calculés

| Paramètre | Valeur | Justification |
| :--- | :--- | :--- |
| **Fréquence Clock ($f_{TIM}$)** | $170 \text{ MHz}$ | Classique sur G474. |
| **Fréquence PWM ($f_{PWM}$)** | $20 \text{ kHz}$ | Conforme au cahier des charges. |
| **ARR (Auto-Reload Register)** | $\mathbf{8499}$ | Calculé pour $\frac{170 \times 10^6}{20 \times 10^3} = 8500$. |
| **Résolution PWM** | $8500 \text{ pas}$ | $\approx 13$ bits de précision. |
| **Conception du Pulse (60%)** | $5100$ | $0.6 \times (8499+1)$. |

---

## 2. Validation des Signaux PWM (Oscilloscope)

Les mesures ont été effectuées sur les sorties **CH1/CH1N** et **CH2/CH2N** (Bras U et Bras V).

### 2.1. Mesures Globales

| Mesure | Valeur Relevée | Statut |
| :--- | :--- | :--- |
| **Fréquence** | $\mathbf{20.00 \text{ kHz}}$ | ✅ **OK** (Confirme le cahier des charges). |
| **Rapports Cycliques** | $\approx 58\%$ et $\approx 38\%$ | ✅ **Cohérent** (La somme $\approx 96\%$, le reste est le *Dead Time*). |
| **Dead Time** (Temps Mort) | $\mathbf{1.000 \ \mu \text{s}}$ | ✅ **OK** (Exactement la valeur visée pour la sécurité). |

### 2.2. Visualisation (Commande Complémentaire Décalée)

La sécurité du pont en H est validée. Visuellement, on observe :
* Les deux signaux (principal et complémentaire) ne sont **jamais hauts en même temps**.
* Un petit intervalle de temps (*trou*) entre les fronts de commutation, confirmant l'insertion effective du $\mathbf{1.000 \ \mu s}$ de *Dead Time* et prévenant le *shoot-through*.

![Fréquence 20.00 kHz sur l'oscilloscope (Zoom large)](https://github.com/user-attachments/assets/2ce35fbc-7082-4c82-be70-cbba5f1809de)
*Figure 1: Validation de la Fréquence ($20.00 \text{ kHz}$) et du Rapport Cyclique ( $\approx 58\%$).*

![Mesure du Dead Time à 1.000 µs](https://github.com/user-attachments/assets/5d56fa61-3f7d-4caf-a7a3-38229f9de34a)
*Figure 2: Mesure du Dead Time ($1.000 \ \mu \text{s}$) entre les signaux complémentaires.*

---

## 3. Implémentation de la Commande de Vitesse (`speed`)

La commande de vitesse est traitée par le Shell (`sh_speed`) et convertie en valeur de registre CCR.

### 3.1. Flux de Traitement Shell

* **Réception:** `HAL_UART_RxCpltCallback()` reçoit la ligne (ex: `"speed 600"`).
* **Parsing:** Le Shell découpe la ligne en `argv[0] = "speed"` et `argv[1] = "600"`.
* **Appel:** La fonction `sh_speed` est appelée pour traiter la valeur.

### 3.2. Conversion et Séquence de Saturation

La fonction `sh_speed` convertit `XXXX` en un pourcentage de *Duty* et effectue la **saturation** :

```c
// On récupère argv[1] = "XXXX" et on fait strtol → entier (raw_value)
// On sature à [0, MOTOR_SPEED_CMD_MAX] (ici, max 1000)

if (raw_value > MOTOR_SPEED_CMD_MAX)
{
    // Saturé à 1000, puis converti en 100% de duty_percent
    raw_value = MOTOR_SPEED_CMD_MAX; 
}
//... Conversion en duty_percent, puis application au CCR.
