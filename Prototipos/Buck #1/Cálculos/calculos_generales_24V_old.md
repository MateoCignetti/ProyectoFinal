# Diseño Convertidor Buck

![Buck](figures/buck.svg)

### Requisitos

  Entrada           Salida
  ----------------- -----------------
  $V_{in}$ = 24 V   $V_{out}$ = 6 V
  $I_{in}$ = 1 A    $I_{out}$ = 4 A
  P = 24 W          

Para el diseño se siguió un diseño de referencia de [Texas
Instrumens](https://www.ti.com/lit/an/slva477b/slva477b.pdf?ts=1740493656949).

# Diseño para 100 kHz

#### Criterios de diseño

-   R = 1,5 $\Omega$
-   Ripple
    -   $\Delta V_{out} = 0,02 *  V_{out} = 0,02 * 6 V = 0,12 V$
    -   $\Delta I_L = 0,3 * I_{out} = 0,3 * 4 A = 1,2 A$
-   Frecuencia de conmutación ($f_s$), 100 kHz

### Cálculo de Duty Cycle máximo

$$D_{max} = \frac{V_{out}}{V_{in} * \eta} = \frac{6 V}{24 V * 0,9} = 0,28$$

Se determina un rendimiento teórico del 90%, se lo toma como criterio de
diseño.

### Cálculo de capacitor de salida

$$C_{min} = \frac{\Delta I_L}{8 * f_s * \Delta V_{out}} = \frac{1,2 A}{8*100\times10^3 Hz * 0,12 V} = 12,5 \ \mu F$$

Se recomienda el uso de capacitores cerámicos de bajo ESR con material
dieléctrico X5R. Cualquier capacidad mayor a la calculada puede ser
utilizada, sin perder el funcionamiento del convertidor.

### Cálculo de inductor

$$L = \frac{(V_{in}-V_{out})*D_{max}}{f_s * \Delta I_L} = \frac{(24 V-6 V)*0,28}{100\times10^3 Hz * 1,2 A} = 42 \ \mu H$$

### Cálculo diodo

$$I_D = I_{out} *(1-D) = 4 \ A * (1-0.28) = 2,88 \ A$$

Corriente media aplicada en el diodo de rectificación. Se recomienda
utilizar un Schottky, la tensión soportada por el diodo debe ser al
menos un 30% más que la tensión de salida del convertidor

### Red Snubber

Para este cálculo se utilizó el siguiente documento
[Toshiba](https://toshiba.semicon-storage.com/info/application_note_en_20180901_AKX00078.pdf?did=63595).
No se pudo seguir por completo, pero se realizaron algunas
consideraciones para el cálculo.

$C_{OSS} = 400 \ pF \approx C_{snub}$

$$ R_{snub} = \frac{1}{2\pi f_p C_{snub}} = 3,9 \ k\Omega$$

$f_p$, representa la frecuencia de las oscilaciones existentes en la
conmutación del transistor, no se tiene este valor, pero para un cálculo
previo se utiliza 100 kHz correspondiente a la conmutación.

### Filtro RC para entrada de ADC

$f_c = 100 Hz$

$C = 10 \ \mu F$

$f_c = \frac{1}{2\pi * R * C}$

$$R = \frac{1}{2\pi * f_c * C} = 159.15 \ \Omega \approx 150 \ \Omega$$

### Resistencia limitadora de corriente para carga de capacitancia de gate de transistor de conmutación

[Infineon](https://www.infineon.com/dgdl/Infineon-power_mosfet_basics-Article-v01_00-EN.pdf?fileId=8ac78c8c8d2fe47b018e625961741a0e&redirId=273241)

[Stack
Exchange](https://electronics.stackexchange.com/questions/287792/what-the-best-way-to-calculate-rg-gate-driver-for-mosfet)

[Texas
Instruments](https://www.ti.com/lit/ml/slua618a/slua618a.pdf?ts=1741669157446)

# Diseño para 15 kHz

### Cálculo de Duty Cycle máximo

$$D_{max} = \frac{V_{out}}{V_{in} * \eta} = \frac{6 V}{24 V * 0,9} = 0,28$$

### Cálculo de capacitor de salida

$$C_{min} = \frac{\Delta I_L}{8 * f_s * \Delta V_{out}} = \frac{1,2 A}{8*15\times10^3 Hz * 0,12 V} = 83,33 \ \mu F$$

### Cálculo de inductor

$$L = \frac{(V_{in}-V_{out})*D_{max}}{f_s * \Delta I_L} = \frac{(24 V-6 V)*0,28}{15\times10^3 Hz * 1,2 A} = 280 \ \mu H$$

### Cálculo diodo

$$I_D = I_{out} *(1-D) = 4 \ A * (1-0.28) = 2,88 \ A$$

### Red Snubber

$C_{OSS} = 400 \ pF \approx C_{snub}$

$$ R_{snub} = \frac{1}{2\pi f_p C_{snub}} = 27 \ k\Omega$$

# Diseño para 2 kHz

### Cálculo de Duty Cycle máximo

$$D_{max} = \frac{V_{out}}{V_{in} * \eta} = \frac{6 V}{24 V * 0,9} = 0,28$$

### Cálculo de capacitor de salida

$$C_{min} = \frac{\Delta I_L}{8 * f_s * \Delta V_{out}} = \frac{1,2 A}{8*2\times10^3 Hz * 0,12 V} = 625 \ \mu F$$

### Cálculo de inductor

$$L = \frac{(V_{in}-V_{out})*D_{max}}{f_s * \Delta I_L} = \frac{(24 V-6 V)*0,28}{2\times10^3 Hz * 1,2 A} = 2.1 \ mH$$

### Cálculo diodo

$$I_D = I_{out} *(1-D) = 4 \ A * (1-0.28) = 2,88 \ A$$

### Red Snubber

$C_{OSS} = 400 \ pF \approx C_{snub}$

$$ R_{snub} = \frac{1}{2\pi f_p C_{snub}} = 200 \ k\Omega$$
