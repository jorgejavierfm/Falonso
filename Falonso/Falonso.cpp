// Falonso.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include "stdafx.h"
#include <stdlib.h>
#include <tchar.h>
#include <time.h>
#include <Windows.h>
#include "falonso2.h"
#include "stdafx.h"

struct
{
	int(*inicio)(int);
	int(*inicio_coche)(int*, int*, int);
	int(*avanza_coche)(int*, int*, int);
	int(*cambio_carril)(int*, int*, int);
	int(*velocidad)(int, int, int);
	int(*semaforo)(int, int);
	int(*fin)(int*);
	void(*pon_error)(const char *);
}DLL;
struct
{
	HANDLE posiciones[276]; //Usado para las posiciones del circuito y los dos mutex de cada semaforo
	HANDLE semaforo_pistoletazo; //Usado para el pistoletazo de salida
	HANDLE evento_creacion; //Usado para notificacion de la creacion de los hilos
	HANDLE evento_acabar; //Usado para informar a los hilos de los coches que tienen que terminar
	HANDLE evento_semaforo; //Usado para informar al hilo del semaforo que tiene que terminar
}IPC;

HINSTANCE cargarDLL();
DWORD WINAPI fnCoches(LPVOID);
DWORD WINAPI fnSemaforos(LPVOID);

void avanzar(int *posicion, int *carril, int color);
int cambio(int posicion, int carril, int *darpos, int *darcarril);
int cruce(int posicion, int carril);
int traductor(int posicion, int carril);
int randgen(int inicio, int fin);

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc != 3)
	{
		fprintf(stderr, "Cantidad de Argumentos invalida");
		exit(1);
	}

	int vueltas = 0; //Variable que va a contar la cantidad de vueltas
	int coches = _tstoi(argv[1]); //Guardamos la cantidad de coches y la velocidad 
	int velocidad = _tstoi(argv[2]);
	HANDLE *threads = (HANDLE*)malloc(sizeof(HANDLE)*coches); //Array con todos los HANDLE de los coches
	HANDLE semaforos;

	HINSTANCE pt_libreria = cargarDLL(); //Cargamos la libreria

	DLL.inicio(velocidad); //llamada la funcion FALONSO2_inicio

	for (int i = 0; i<276; i++)
	{
		IPC.posiciones[i] = CreateMutex(NULL, 0, NULL);
		if (IPC.posiciones[i] == NULL)
		{
			DLL.pon_error("Error al crear los mutex");
			exit(1);
		}
		if (i == 106 || i == 108 || i == 236 || i == 238) //Quitamos los mutex de las 4 posiciones del cruce 
			WaitForSingleObject(IPC.posiciones[i], 0);
	}

	IPC.semaforo_pistoletazo = CreateSemaphore(NULL, 0, coches + 1, NULL); //Semaforo que le avisara a los hilos que comiencen

	IPC.evento_creacion = CreateEvent(NULL, TRUE, FALSE, NULL); //Evento de creacion de los hilos
	IPC.evento_acabar = CreateEvent(NULL, TRUE, FALSE, NULL); //Evento de finalizacion de los coches
	IPC.evento_semaforo = CreateEvent(NULL, TRUE, TRUE, NULL); //Evento de finalizacion de los semaforos

															   /*Creamos los diferentes coches*/
	for (int i = 0; i<coches; i++)
	{
		threads[i] = CreateThread(NULL, 0, &fnCoches, (LPVOID)&vueltas, 0, NULL);
		WaitForSingleObject(IPC.evento_creacion, INFINITE);
		ResetEvent(IPC.evento_creacion);
	}

	/*Creamos el hilo que controlara los semaforos*/
	semaforos = CreateThread(NULL, 0, &fnSemaforos, (LPVOID)&velocidad, 0, NULL);
	WaitForSingleObject(IPC.evento_creacion, INFINITE);

	/*Avisamos a todos los hilos que pueden comenzar*/
	ReleaseSemaphore(IPC.semaforo_pistoletazo, coches + 1, NULL);

	Sleep(30000); //Amount of time that the program will be active -- ms
	SetEvent(IPC.evento_acabar); //Activamos el evento para que finalicen los coches

	WaitForMultipleObjects(coches, threads, TRUE, INFINITE); //Esperamos que todos los coches terminen

	ResetEvent(IPC.evento_semaforo); //Detenemos los semaforos (despues de los coches para evitar saltos)
	WaitForSingleObject(semaforos, INFINITE);

	DLL.fin(&vueltas);
	FreeLibrary(pt_libreria); //Liberamos la libreria
	return 7;
}

DWORD WINAPI fnSemaforos(LPVOID parametro)
{
	int velocidad = *(int*)(parametro);

	DLL.semaforo(VERTICAL, VERDE);
	WaitForSingleObject(IPC.posiciones[275], 0);
	DLL.semaforo(HORIZONTAL, ROJO);

	SetEvent(IPC.evento_creacion); //Avisamos que se ha creado el semaforo
	WaitForSingleObject(IPC.semaforo_pistoletazo, INFINITE); //Esperamos por el pistoletazo

	while (1)
	{
		if (WaitForSingleObject(IPC.evento_semaforo, 0) != WAIT_OBJECT_0)
			break;

		/*Solicitamos este mutex para cambiar de posicion o dejar que el coche avance*/
		ReleaseMutex(IPC.posiciones[274]);
		DLL.semaforo(VERTICAL, VERDE);
		if (velocidad)
			Sleep(2000);

		DLL.semaforo(VERTICAL, AMARILLO);
		if (velocidad)
			Sleep(1000);

		WaitForSingleObject(IPC.posiciones[274], INFINITE);
		DLL.semaforo(VERTICAL, ROJO);

		DLL.semaforo(HORIZONTAL, AMARILLO);
		if (velocidad)
			Sleep(1000);

		/*Solicitamos este mutex para cambiar de posicion o dejar que el coche avance*/
		ReleaseMutex(IPC.posiciones[275]);
		DLL.semaforo(HORIZONTAL, VERDE);
		if (velocidad)
			Sleep(2000);

		DLL.semaforo(HORIZONTAL, AMARILLO);
		if (velocidad)
			Sleep(1000);

		WaitForSingleObject(IPC.posiciones[275], INFINITE);
		DLL.semaforo(HORIZONTAL, ROJO);

		DLL.semaforo(VERTICAL, AMARILLO);
		if (velocidad)
			Sleep(1000);
	}

	return 1;
}
DWORD WINAPI fnCoches(LPVOID parametro)
{
	int posicion, carril, color, sum;
	int velocidad, tcarril;

	srand((unsigned int)time(NULL));

	do
	{
		carril = randgen(0, 1);
		sum = carril * 137;
		posicion = randgen(0, 136);
	} while (cruce(posicion, carril) || WaitForSingleObject(IPC.posiciones[posicion + sum], 0) != WAIT_OBJECT_0);

	velocidad = randgen(1, 99);
	color = randgen(0, 7);

	/*Situamos el coche en la posicion y carril calculadas arriba*/
	DLL.inicio_coche(&carril, &posicion, color);

	/*Avisamos que nos hemos creado*/
	SetEvent(IPC.evento_creacion);
	WaitForSingleObject(IPC.semaforo_pistoletazo, INFINITE); //Esperamos por el pistoletazo

	while (1)
	{
		/*Garantizamos que  estando en estas posiciones avanzo o espero que el semaforo cambie*/
		if ((posicion == 20 && carril == 0) || (posicion == 22 && carril == 1))
			WaitForSingleObject(IPC.posiciones[274], INFINITE);
		if ((posicion == 105 && carril == 0) || (posicion == 98 && carril == 1))
			WaitForSingleObject(IPC.posiciones[275], INFINITE);

		tcarril = carril;

		avanzar(&posicion, &carril, color);

		/*Incremento el contador de vueltas si estoy en estas posiciones y no cambio de carril*/
		if (((posicion == 133 && carril == 0) || (posicion == 131 && carril == 1)) && tcarril == carril)
			InterlockedIncrement((long*)(parametro));

		/*Si he cogio los mutex de arriba los libero para que el semaforo pueda cambiar de color*/
		ReleaseMutex(IPC.posiciones[274]);
		ReleaseMutex(IPC.posiciones[275]);

		DLL.velocidad(velocidad, carril, posicion);
	}

	return 1;
}

void avanzar(int *posicion, int *carril, int color)
{
	int valor_devuelto, tpos = 0, tcarril = 0;
	int apos = *posicion;
	int acarril = *carril;
	HANDLE posiciones[3]; //Vamos a añadir los dif. eventos por los que esperaremos

						  //EVENTOS A ESPERAR: EVENTO_ACABAR, MUTEX_POSICION_ADELANTE, MUTEX_POSICION_CAMBIO_DE_CARRIL

						  //Añadimos el evento acabar para detenernos y no avanzar mas*/
	posiciones[0] = IPC.evento_acabar;

	/*Vemos si estamos en alguna de las 4 posiciones especiales para compartir los mutex*/
	if (apos + 1 != traductor(apos + 1, acarril))
		posiciones[1] = IPC.posiciones[traductor(apos + 1, acarril)];
	else if (apos != 136) //Si no estamos en la posicion 136 solicitamos el mutex de la posicion siguiente
		posiciones[1] = IPC.posiciones[apos + acarril * 137 + 1];
	else
		posiciones[1] = IPC.posiciones[acarril * 137]; //Si estamos en la posicion 136 solicitamos el mutex de la primera posicion

													   //Añadimos el evento cambio de carril si procede
	if (cambio(apos, acarril, &tpos, &tcarril))
	{
		posiciones[2] = IPC.posiciones[tpos + tcarril * 137];
		valor_devuelto = WaitForMultipleObjectsEx(3, posiciones, FALSE, INFINITE, TRUE);
	}
	else
		valor_devuelto = WaitForMultipleObjectsEx(2, posiciones, FALSE, INFINITE, TRUE);

	switch (valor_devuelto)
	{
	case WAIT_OBJECT_0 + 0:
		ExitThread(7);
		break;
	case WAIT_OBJECT_0 + 1:
		DLL.avanza_coche(carril, posicion, color);
		break;
	case WAIT_OBJECT_0 + 2:
		DLL.cambio_carril(carril, posicion, color);
		break;
	}

	/*Si estamos en alguna posicion con mutex compartido lo liberamos llamando a traductor*/
	if (traductor(apos, acarril) != apos)
		ReleaseMutex(IPC.posiciones[traductor(apos, acarril)]);
	else
		ReleaseMutex(IPC.posiciones[apos + acarril * 137]);
}
int cambio(int pos, int carril, int *darpos, int *darcarril)
{
	int i, tpos, tcarril;

	/*En este array la ultima cifra va a ser lo que se le va a añadir (en caso de ser positivo) o lo que se le va a restar (en caso de ser negativo) a 	la posicion pasada como parametro,las dos primera cifras van a ser la maxima posicion en el carril en cuestion hasta donde va a ser posible aplicar 		la cifra anteriormente mencionada*/

	static int inter[2][11] = { { 130,281,600,-621,-652,-673,-684,-1295,-1303,-1342,-1361 },
	{ 150,-281,580,601,622,644,1255,1264,1283,1332,1360 } }; //Ver que hace con esta ultima posicion

	for (i = 0; i<11; i++)
	{
		tpos = inter[carril][i] / 10;
		if (pos <= abs(tpos) && tpos != 136)
		{
			tpos = pos + inter[carril][i] % 10;
			break;
		}
	}

	if (darpos != NULL)
		(*darpos) = tpos;

	tcarril = (carril == 1 ? 0 : 1);

	if (darcarril != NULL)
		(*darcarril) = tcarril;

	/*Ningun coche entra en el cruce cambiando de carril*/
	if (!cruce(tpos, tcarril))
		return 1;
	else
		return 0;
}
int cruce(int posicion, int carril)
{
	if ((carril == 0 && (posicion>20 && posicion<24)) || (carril == 1 && (posicion>22 && posicion<26)))
		return 1;
	if ((carril == 0 && (posicion>105 && posicion<109)) || (carril == 1 && (posicion>98 && posicion<102)))
		return 2;

	return 0;
}
int traductor(int posicion, int carril)
{
	/*Esta funcion va a ser principalmente usada para los mutex compartidos*/

	if (posicion == 106 && carril == 0)
		return 160;
	if (posicion == 99 && carril == 1)
		return 162;
	if (posicion == 101 && carril == 1)
		return 23;
	if (posicion == 108 && carril == 0)
		return 21;


	return posicion;
}
int randgen(int inicio, int fin)
{
	return rand() % (fin + 1 - inicio) + inicio;
}

HINSTANCE cargarDLL()
{
	HINSTANCE pt_inicio = LoadLibrary(TEXT("falonso2.dll"));
	if (!pt_inicio)
	{
		fprintf(stderr, "Error al cargar la biblioteca.\n");
		exit(1);
	}
	/*INICIO*/
	DLL.inicio = (int(*)(int))GetProcAddress(pt_inicio, "FALONSO2_inicio");
	/*INICIO_COCHE*/
	DLL.inicio_coche = (int(*)(int*, int*, int))GetProcAddress(pt_inicio, "FALONSO2_inicio_coche");
	/*AVANZA_COCHE*/
	DLL.avanza_coche = (int(*)(int*, int*, int))GetProcAddress(pt_inicio, "FALONSO2_avance_coche");
	/*VELOCIDAD*/
	DLL.velocidad = (int(*)(int, int, int))GetProcAddress(pt_inicio, "FALONSO2_velocidad");
	/*CAMBIO_CARRIL*/
	DLL.cambio_carril = (int(*)(int*, int*, int))GetProcAddress(pt_inicio, "FALONSO2_cambio_carril");
	/*LUZ SEMAFORO*/
	DLL.semaforo = (int(*)(int, int))GetProcAddress(pt_inicio, "FALONSO2_luz_semAforo");
	/*FIN*/
	DLL.fin = (int(*)(int*))GetProcAddress(pt_inicio, "FALONSO2_fin");
	/*PON_ERROR*/
	DLL.pon_error = (void(*)(const char *))GetProcAddress(pt_inicio, "pon_error");

	return pt_inicio;
}