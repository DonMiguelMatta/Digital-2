/*
 * main.c
 *
 * LAB1
 */

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <stdint.h>

#include "timer/TIMER.h"
#include "7seg/7seglib.h"

/****************************************/
// Configuracion

#define META                    4
#define TIEMPO_ANTIRREBOTE_MS   20
#define TIEMPO_CUENTA_MS        1000
#define CUENTA_REGRESIVA_INICIO 5

/****************************************/
// Inicio de carrera

// D12
#define BOTON_INICIO PB4

/****************************************/
// Jugador 1

// D9 y D8
#define LED_J1_1    PB1
#define LED_J1_2    PB0

// D7 y D6
#define LED_J1_3    PD7
#define LED_J1_4    PD6

// D11
#define BOTON_J1    PB3

#define LEDS_J1_PORTB_MASK \
	((1 << LED_J1_1) | (1 << LED_J1_2))

#define LEDS_J1_PORTD_MASK \
	((1 << LED_J1_3) | (1 << LED_J1_4))

/****************************************/
// Jugador 2

// D5, D4, D3 y D2
#define LED_J2_1    PD5
#define LED_J2_2    PD4
#define LED_J2_3    PD3
#define LED_J2_4    PD2

// D10
#define BOTON_J2    PB2

#define LEDS_J2_PORTD_MASK \
	((1 << LED_J2_1) | \
	 (1 << LED_J2_2) | \
	 (1 << LED_J2_3) | \
	 (1 << LED_J2_4))

/****************************************/
// Estructura para antirrebote

typedef struct
{
	uint8_t estadoEstable;
	uint8_t ultimaLectura;
	uint8_t contadorTiempo;

} Boton_t;

/****************************************/
// Variables globales

static Boton_t botonJugador1 =
{
	.estadoEstable = 1,
	.ultimaLectura = 1,
	.contadorTiempo = 0
};

static Boton_t botonJugador2 =
{
	.estadoEstable = 1,
	.ultimaLectura = 1,
	.contadorTiempo = 0
};

static Boton_t botonInicio =
{
	.estadoEstable = 1,
	.ultimaLectura = 1,
	.contadorTiempo = 0
};

static uint8_t contadorJugador1 = 0;
static uint8_t contadorJugador2 = 0;

static uint8_t carreraIniciada = 0;
static uint8_t carreraFinalizada = 0;
static uint8_t ganador = 0;

static uint8_t cuentaRegresivaActiva = 0;
static uint8_t valorCuentaRegresiva = CUENTA_REGRESIVA_INICIO;
static uint16_t contadorCuentaMs = 0;

/****************************************/
// Prototipos

static void sistemaInit(void);

static uint8_t actualizarAntirrebote(
	Boton_t *boton,
	uint8_t lecturaActual
);

static uint8_t rutinaBotonInicio(void);
static uint8_t rutinaJugador1(void);
static uint8_t rutinaJugador2(void);

static void mostrarProgresoJugador1(uint8_t contador);
static void mostrarProgresoJugador2(uint8_t contador);

static void apagarLEDsJugador1(void);
static void apagarLEDsJugador2(void);

static void iniciarCuentaRegresiva(void);
static void actualizarCuentaRegresiva(void);
static void finalizarCarrera(uint8_t jugadorGanador);
static void tareaCada1ms(void);

/****************************************/
// Programa principal

int main(void)
{
	sistemaInit();

	while (1)
	{
		/*
		 * Timer0 genera una bandera cada 1 ms.
		 * No se utilizan retardos bloqueantes.
		 */
		if (timer0CompareAFlag())
		{
			timer0ClearCompareAFlag();

			tareaCada1ms();
		}
	}

	return 0;
}

/****************************************/
// Inicializacion

static void sistemaInit(void)
{
	/************************************/
	// LEDs del jugador 1

	DDRB |= LEDS_J1_PORTB_MASK;
	DDRD |= LEDS_J1_PORTD_MASK;

	/************************************/
	// LEDs del jugador 2

	DDRD |= LEDS_J2_PORTD_MASK;

	/************************************/
	// Botones D12, D11 y D10 como entradas

	DDRB &= ~((1 << BOTON_INICIO) | (1 << BOTON_J1) | (1 << BOTON_J2));

	/*
	 * Activar resistencias pull-up internas.
	 *
	 * Boton sin presionar = 1
	 * Boton presionado = 0
	
	 */
	PORTB |= (1 << BOTON_INICIO) | (1 << BOTON_J1) | (1 << BOTON_J2);

	/************************************/
	// Apagar todos los LEDs

	apagarLEDsJugador1();
	apagarLEDsJugador2();

	/************************************/
	// Inicializar display

	display_init();
	display_show(0);

	/************************************/
	// Configurar Timer0 para 1 ms

	initTimer0_CTC(
		timer0_usToTicks(
			1000,
			TIMER_PRESCALER_64
		),
		TIMER_PRESCALER_64
	);
}

/****************************************/
// Antirrebote 

static uint8_t actualizarAntirrebote(
	Boton_t *boton,
	uint8_t lecturaActual
)
{
	
	if (lecturaActual != boton->ultimaLectura)
	{
		boton->ultimaLectura = lecturaActual;
		boton->contadorTiempo = 0;

		return 0;
	}


	if (boton->contadorTiempo < TIEMPO_ANTIRREBOTE_MS)
	{
		boton->contadorTiempo++;
	}

	/*
	 * Aceptar el nuevo estado despues de 20 ms.
	 */
	if ((boton->contadorTiempo >= TIEMPO_ANTIRREBOTE_MS) &&
	    (lecturaActual != boton->estadoEstable))
	{
		boton->estadoEstable = lecturaActual;

		/*
		 * Como se utiliza pull-up:
		 * estado 0 significa boton presionado.
		 */
		if (boton->estadoEstable == 0)
		{
			return 1;
		}
	}

	return 0;
}

/****************************************/
// Rutina del boton de inicio

static uint8_t rutinaBotonInicio(void)
{
	uint8_t lecturaBoton;

	if (PINB & (1 << BOTON_INICIO))
	{
		lecturaBoton = 1;
	}
	else
	{
		lecturaBoton = 0;
	}

	return actualizarAntirrebote(
		&botonInicio,
		lecturaBoton
	);
}

/****************************************/
// Rutina del jugador 1

static uint8_t rutinaJugador1(void)
{
	uint8_t lecturaBoton;

	if (PINB & (1 << BOTON_J1))
	{
		lecturaBoton = 1;
	}
	else
	{
		lecturaBoton = 0;
	}

	return actualizarAntirrebote(
		&botonJugador1,
		lecturaBoton
	);
}

/****************************************/
// Rutina del jugador 2

static uint8_t rutinaJugador2(void)
{
	uint8_t lecturaBoton;

	if (PINB & (1 << BOTON_J2))
	{
		lecturaBoton = 1;
	}
	else
	{
		lecturaBoton = 0;
	}

	return actualizarAntirrebote(
		&botonJugador2,
		lecturaBoton
	);
}

/****************************************/
// Apagar LEDs del jugador 1

static void apagarLEDsJugador1(void)
{
	PORTB &= ~LEDS_J1_PORTB_MASK;
	PORTD &= ~LEDS_J1_PORTD_MASK;
}

/****************************************/
// Apagar LEDs del jugador 2

static void apagarLEDsJugador2(void)
{
	PORTD &= ~LEDS_J2_PORTD_MASK;
}

/****************************************/
// Mostrar progreso del jugador 1

static void mostrarProgresoJugador1(uint8_t contador)
{
	apagarLEDsJugador1();

	if (contador >= 1)
	{
		// Primer LED: D9
		PORTB |= (1 << LED_J1_1);
	}

	if (contador >= 2)
	{
		// Segundo LED: D8
		PORTB |= (1 << LED_J1_2);
	}

	if (contador >= 3)
	{
		// Tercer LED: D7
		PORTD |= (1 << LED_J1_3);
	}

	if (contador >= 4)
	{
		// Cuarto LED: D6
		PORTD |= (1 << LED_J1_4);
	}
}

/****************************************/
// Mostrar progreso del jugador 2

static void mostrarProgresoJugador2(uint8_t contador)
{
	apagarLEDsJugador2();

	if (contador >= 1)
	{
		// Primer LED: D5
		PORTD |= (1 << LED_J2_1);
	}

	if (contador >= 2)
	{
		// Segundo LED: D4
		PORTD |= (1 << LED_J2_2);
	}

	if (contador >= 3)
	{
		// Tercer LED: D3
		PORTD |= (1 << LED_J2_3);
	}

	if (contador >= 4)
	{
		// Cuarto LED: D2
		PORTD |= (1 << LED_J2_4);
	}
}

/****************************************/
// Iniciar cuenta regresiva

static void iniciarCuentaRegresiva(void)
{
	contadorJugador1 = 0;
	contadorJugador2 = 0;

	carreraIniciada = 0;
	carreraFinalizada = 0;
	ganador = 0;

	cuentaRegresivaActiva = 1;
	valorCuentaRegresiva = CUENTA_REGRESIVA_INICIO;
	contadorCuentaMs = 0;

	apagarLEDsJugador1();
	apagarLEDsJugador2();
	display_show(valorCuentaRegresiva);
}

/****************************************/
// Actualizar cuenta regresiva

static void actualizarCuentaRegresiva(void)
{
	contadorCuentaMs++;

	if (contadorCuentaMs < TIEMPO_CUENTA_MS)
	{
		return;
	}

	contadorCuentaMs = 0;

	if (valorCuentaRegresiva > 0)
	{
		valorCuentaRegresiva--;
		display_show(valorCuentaRegresiva);
	}
	else
	{
		cuentaRegresivaActiva = 0;
		carreraIniciada = 1;
	}
}

/****************************************/
// Finalizar carrera

static void finalizarCarrera(uint8_t jugadorGanador)
{
	carreraFinalizada = 1;
	ganador = jugadorGanador;

	if (ganador == 1)
	{
		/*
		 * Encender los cuatro LEDs del jugador 1.
		 */
		PORTB |= LEDS_J1_PORTB_MASK;
		PORTD |= LEDS_J1_PORTD_MASK;

		/*
		 * Apagar los cuatro LEDs del jugador 2.
		 */
		apagarLEDsJugador2();

		/*
		 * Mostrar jugador 1 en el display.
		 */
		display_show(1);
	}
	else
	{
		/*
		 * Apagar los cuatro LEDs del jugador 1.
		 */
		apagarLEDsJugador1();

		/*
		 * Encender los cuatro LEDs del jugador 2.
		 */
		PORTD |= LEDS_J2_PORTD_MASK;

		/*
		 * Mostrar jugador 2 en el display.
		 */
		display_show(2);
	}
}

/****************************************/
// Tarea ejecutada cada 1 ms

static void tareaCada1ms(void)
{
	uint8_t pulsacionInicio;
	uint8_t pulsacionJugador1;
	uint8_t pulsacionJugador2;

	/*
	 * Leer los botones en cada ejecucion.
	 * El antirrebote de un boton no bloquea al otro.
	 */
	pulsacionInicio = rutinaBotonInicio();
	pulsacionJugador1 = rutinaJugador1();
	pulsacionJugador2 = rutinaJugador2();

	if (pulsacionInicio)
	{
		if (((carreraIniciada == 0) && (cuentaRegresivaActiva == 0)) ||
		    carreraFinalizada)
		{
			iniciarCuentaRegresiva();
		}
	}

	if (cuentaRegresivaActiva)
	{
		actualizarCuentaRegresiva();
		return;
	}

	if (carreraIniciada == 0)
	{
		return;
	}

	/*
	 * Cuando la carrera finaliza, los contadores
	 * dejan de incrementarse.
	 */
	if (carreraFinalizada)
	{
		return;
	}

	/************************************/
	// Contador del jugador 1

	if (pulsacionJugador1)
	{
		if (contadorJugador1 < META)
		{
			contadorJugador1++;
		}
	}

	/************************************/
	// Contador del jugador 2

	if (pulsacionJugador2)
	{
		if (contadorJugador2 < META)
		{
			contadorJugador2++;
		}
	}

	/************************************/
	// Comprobar ganador

	if ((contadorJugador1 >= META) &&
	    (contadorJugador2 >= META))
	{
		/*
		 * Si ambos alcanzan la meta dentro del mismo
		 * milisegundo, se asigna prioridad al jugador 1.
		 */
		finalizarCarrera(1);
	}
	else if (contadorJugador1 >= META)
	{
		finalizarCarrera(1);
	}
	else if (contadorJugador2 >= META)
	{
		finalizarCarrera(2);
	}
	else
	{
		mostrarProgresoJugador1(contadorJugador1);
		mostrarProgresoJugador2(contadorJugador2);
	}
}
