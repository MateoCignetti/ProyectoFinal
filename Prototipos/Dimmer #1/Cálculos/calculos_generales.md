# Diseño dimmer control analógico o digital

![Dimmer](figures/circuito.png)

## Entradas y salidas
| Entrada                       | Salida                                |
|-------                        |-------                                |
|$V_{ca}$ = 48 Vca              | $UC_{phase}$ = Onda cuadrada 3.3 Vcc  |
|$5V_{cc}$ = 5 Vcc              | $Load$ = Onda recortada 0-48 Vca RMS  |
|$3.3V_{cc}$ = 3.3 Vcc          |                                       |
|$UC_{Dout}$ = Pulso 3.3 Vcc    |                                       |
|$UC_{Relay}$ = 0 V o 3.3 Vcc   |                                       |

## Circuito control analógico R-C

### Criterios de diseño
* $C1 = 100\  nF$
* $\hat{V}_{DIAC} = 32 V$ (Datasheet DB3)

### Desarrollo de fórmulas

1. $$ V_{DIAC} = I_{RMS} \cdot X_C $$
2. $$ I_{RMS} = \dfrac{V_{ca}}{|Z|} = \dfrac{V_{ca}}{\sqrt{R^2+{X_C}^2}} $$

*Reemplazando 2 en 1:*

3. $$ V_{DIAC} = \dfrac{V_{ca}}{\sqrt{R^2+{X_C}^2}} \cdot X_C $$
4. $$ \hat{V}_{DIAC} = \dfrac{V_{DIAC}}{\sqrt{2}} $$

*Reemplazando 4 en 3:*

5. $$ \hat{V}_{DIAC} = \dfrac{\sqrt{2} \cdot V_{ca} \cdot X_C }{\sqrt{R^2+{X_C}^2}} $$

*Pasando denominador y elevando ambos lados al cuadrado:*

6. $$ { \left( \hat{V}_{DIAC} \cdot \sqrt{R^2+{X_C}^2} \right) }^2 = {\left( \sqrt{2} \cdot V_{ca} \cdot X_C \right)} ^2 $$

7. $$ {\hat{V}_{DIAC}}^2 \cdot \left( R^2 + {X_C}^2  \right) = 2 \cdot {V_{ca}}^2 \cdot {X_C}^2 $$

8. $$ R^2 + {X_C}^2 = \dfrac{2 \cdot {V_{ca}}^2 \cdot {X_C}^2}{{\hat{V}_{DIAC}}^2} $$

9. $$ R^2 = \dfrac{2 \cdot {V_{ca}}^2 \cdot {X_C}^2}{{\hat{V}_{DIAC}}^2} - {X_C}^2 $$

10. $$ R^2 = {X_C}^2 \cdot \left( \dfrac{2 \cdot {V_{ca}}^2}{{\hat{V}_{DIAC}}^2} - 1 \right) $$

11. $$ R = \sqrt{{X_C}^2 \cdot \left( \dfrac{2 \cdot {V_{ca}}^2}{{\hat{V}_{DIAC}}^2} - 1 \right) } $$

*Distribuyendo raíz cuadrada:*

12. $$ R = X_C \cdot \sqrt{\dfrac{2 \cdot {V_{ca}}^2}{{\hat{V}_{DIAC}}^2} - 1 } $$

*Reemplazando valores para obtener el valor de la resistencia:*

13. $$ X_C = \dfrac{1}{2 \pi \cdot f \cdot C}$$

