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
