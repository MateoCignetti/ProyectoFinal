| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# Control for the Dimmer #1 Prototype

This firmware controls the Dimmer #1 prototype using an ESP32-S3 microcontroller. It detects zero crossings of the AC signal and uses two hardware timers to precisely control a TRIAC for phase-cut dimming. The dimming level is set via an analog input (ADC), and all timing is handled without blocking the main program. The code includes optional test signal generation for development without real AC hardware. All configuration and control logic is implemented in `main.c`.

| Dispositivos Soportados | ESP32-S3 |
| ----------------------- | -------- |

# Control para el Prototipo Dimmer #1

Este firmware controla el prototipo Dimmer #1 utilizando un microcontrolador ESP32-S3. Detecta los cruces por cero de la señal de CA y utiliza dos temporizadores de hardware para controlar con precisión un TRIAC mediante atenuación por corte de fase. El nivel de atenuación se ajusta mediante una entrada analógica (ADC), y toda la temporización se gestiona sin bloquear el programa principal. El código incluye la generación opcional de señales de prueba para el desarrollo sin hardware de CA real. Toda la configuración y lógica de control está implementada en `main.c`.