# Diseño Convertidor Buck

![Buck](figures/buck.svg)

### Requisitos
| Entrada | Salida |
|-------|-------|
|$V_{in}$ = 12 V| $V_{out}$ = 6 V|
|$I_{in}$ = 1 A | $I_{out}$ = 2 A|
|P = 12 W|

Para el diseño se siguió un diseño de referencia de
[Texas Instrumens](https://www.ti.com/lit/an/slva477b/slva477b.pdf?ts=1740493656949).

#### Criterios de diseño
* R = 1,5 $\Omega$
* Ripple
  * $\Delta V_{out} = 0,02 \ V_{out} = 0,02 * 6 V = 0,12 \ V$ 
  * $\Delta I_L = 0,3 \ I_{out} = 0,3 * 2 A = 0,6 \ A$
* Frecuencia de conmutación ($f_s$), 15 kHz

### Cálculo de Duty Cycle máximo

$$D_{max} = \frac{V_{out}}{V_{in} * \eta} = \frac{6 V}{12 V * 0,9} = 0,55$$

Se determina un rendimiento teórico del 90%, se lo toma como criterio de diseño.

### Cálculo de capacitor de salida

$$C_{min} = \frac{\Delta I_L}{8 * f_s * \Delta V_{out}} = \frac{0,6 A}{8*15\times10^3 Hz * 0,12 V} = 41,66 \ \mu F$$

Se recomienda el uso de capacitores cerámicos de bajo ESR con material dieléctrico
X5R. Cualquier capacidad mayor a la calculada puede ser utilizada, sin perder el funcionamiento 
del convertidor.

### Cálculo de inductor

$$L = \frac{(V_{in}-V_{out})*D_{max}}{f_s * \Delta I_L} = \frac{(12 V-6 V)*0,55}{15\times10^3 Hz * 0,6 A} = 366,66 \ \mu H$$

### Cálculo diodo

$$I_D = I_{out} *(1-D) = 2 \ A * (1-0.55) = 0,9 \ A$$

Corriente media aplicada en el diodo de rectificación. Se recomienda utilizar un Schottky, la 
tensión soportada por el diodo debe ser al menos un 30% más que la tensión de salida del convertidor

### Red Snubber 

Para este cálculo se utilizó el siguiente documento [Toshiba](https://toshiba.semicon-storage.com/info/application_note_en_20180901_AKX00078.pdf?did=63595).
No se pudo seguir por completo, pero se realizaron algunas consideraciones para el cálculo.

Se supone un valor de inductancia parásita típico de 50 nH. $C_P = C_{OSS}$

+ IRF9540

$C_{OSS} = 400 \ pF \approx C_{snub}(min)$

$$f_{ring} = \frac{1}{2 \pi * \sqrt{L_P * C_P}} = \frac{1}{2 \pi * \sqrt{50 \ nH * 400 \ pF}} = 35,6 MHz$$

$$ R_{snub} = \frac{1}{2\pi f_{ring} C_{snub}} = 11,2 \ \Omega$$

$f_{ring}$, representa la frecuencia de las oscilaciones existentes en la conmutación del transistor, no se
tiene este valor, pero se calcula en función de un valor estimado de Inductancia parásita, lo ideal es
medirlo uno vez el circuito se encuentre armado.

+ IRLML6344

$C_{OSS} = 65 \ pF \approx C_{snub}(min)$

$$f_{ring} = \frac{1}{2 \pi * \sqrt{L_P * C_P}} = \frac{1}{2 \pi * \sqrt{50 \ nH * 65 \ pF}} = 88,3 MHz$$

$$ R_{snub} = \frac{1}{2\pi f_{ring} C_{snub}} = 27,7 \ \Omega$$

### Filtro RC para entrada de ADC

$f_c = 100 Hz$

$C = 10 \ \mu F$

$f_c = \frac{1}{2\pi * R * C}$

$$R = \frac{1}{2\pi * f_c * C} = 159.15 \ \Omega \approx 150 \ \Omega$$

### Resistencia limitadora de corriente para carga de capacitancia de gate de transistor utilizado como driver

Microcontrolador a utilizar ESP32-S3, según la hoja de datos la corriente acumulada de todos los pines
es de 1,5 A. [Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf), como posee 45 GPIOS, el resultado aproximado de corriente por pin es de 33,33 mA.

$$R_{min} = \frac{V}{I} = \frac{3,3 \ V}{33,33 mA} = 99 \Omega$$

Se elige una resistencia de 150 $\Omega$. Esta resistencia es necesaria porque el microcontrolador
vería un cortocircuito cuando el capacitor de entrada está descargado generando una alta corriente,
además ayuda a prevenir corrientes residuales de oscilación. A tener en cuenta, cuanto mayor es el valor
de resistencia, la respuesta del transistor disminuye.

[Infineon](https://www.infineon.com/dgdl/Infineon-power_mosfet_basics-Article-v01_00-EN.pdf?fileId=8ac78c8c8d2fe47b018e625961741a0e&redirId=273241), página 11

[Stack Exchange](https://electronics.stackexchange.com/questions/287792/what-the-best-way-to-calculate-rg-gate-driver-for-mosfet)

[Texas Instruments](https://www.ti.com/lit/ml/slua618a/slua618a.pdf?ts=1741669157446)

### Resistencia para transistor de conmutación

La corriente requerida para cargar la gate del transistor está determinada por su capacitancia de gate, y
el período de conmutación.

$t=1/f_s = 66,66 \ \mu s$

$$I_g = \frac{Q_g}{t} = \frac{97 \ nC}{66,66 \ \mu s} = 1,455 \ mA$$

$$R_{max} = \frac{V_{in}}{I_g} = \frac{12 \ V}{1,455 \ mA} = 8,25 \ k\Omega$$

Este valor fue probado en simulación pero generaba una disminución drástica en el tiempo de respuesta del transistor, por ello se probará en la práctica para determinar si este cálculo es adecuado o no.