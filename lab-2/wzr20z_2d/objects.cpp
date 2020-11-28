/*********************************************************************
	Simulation obiekt�w fizycznych ruchomych np. samochody, statki, roboty, itd.
	+ obs�uga obiekt�w statycznych np. terrain.
	**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <windows.h>
#include <gl\gl.h>
#include <gl\glu.h>
#include <iterator> 
#include <map>
#include "objects.h"
#include "graphics.h"
using namespace std;
extern FILE *f;
extern Terrain terrain;
extern map<int, MovableObject*> other_vehicles;

extern CRITICAL_SECTION m_cs;

extern bool if_prediction_test;
extern bool if_ID_visible;
extern long number_of_cyc;
long number_of_simulations = 0;

MovableObject::MovableObject()             // konstruktor                   
{
	iID = (unsigned int)(rand() % 1000);  // identyfikator obiektu
	fprintf(f, "my_vehicle->iID = %d\n", iID);
	bool nn = if_prediction_test;

	// zmienne zwi�zame z akcjami kierowcy
	F = Fb = 0;	// si�y dzia�aj�ce na obiekt 
	breaking_factor = 0;			// stopie� hamowania
	wheel_turn_speed = 0;  // pr�dko�� kr�cenia kierownic� w rad/s
	if_keep_steer_wheel = 0;  // informacja czy kieronica jest trzymana

	// sta�e samochodu
	mass_own = 15 +  (nn ? 7.0 : 19.0*(float)rand() / RAND_MAX);			// masa obiektu [kg]
	//Fy = mass_own*9.81;        // si�a nacisku na podstaw� obiektu (na ko�a pojazdu)
	length = 9.0;
	width = 4.0;
	height = 1.7;
	clearance = 0.0;             // wysoko�� na kt�rej znajduje si� podstawa obiektu
	front_axis_dist = 1.0;       // odleg�o�� od przedniej osi do przedniego zderzaka 
	back_axis_dist = 0.2;        // odleg�o�� od tylniej osi do tylniego zderzaka
	wheel_turn_return_speed = 0.5; // pr�dko�� powrotu kierownicy w rad/s (gdy zostateie puszczona)
	friction_linear = 2;// 1.5 + 3.0*(float)rand() / RAND_MAX;              // wsp�czynnik tarcia obiektu o pod�o�e 
	friction_rot = 2;// friction_linear*(0.5 + (float)rand() / RAND_MAX);     // tarcie obrotowe obrotowe opon po pod�o�u (w szczeg�lnych przypadkach mo�e by� inne ni� liniowe)
	friction_roll = 0.15;        // wsp�czynnik tarcia tocznego
	friction_air = 0.0001;         // wsp�czynnik oporu powietrza (si�a zale�y od kwadratu pr�dko�ci)
	elasticity = 0.7;            // wsp�czynnik spr�ysto�ci (0-brak spr�ysto�ci, 1-doskona�a spr�ysto��) 
	wheel_angle_max = PI*60.0 / 180;   // maksymalny k�t skr�tu k�
	F_max = 1000;                 // maksymalna si�a pchaj�ca do przodu

	// parametry stanu auta:
	state.wheel_angle = 0;
	state.vPos.y = clearance + height / 2 + 20; // wysoko�� �rodka ci�ko�ci w osi pionowej pojazdu
	state.vPos.x = 0;
	state.vPos.z = 0;
	quaternion qObr = AsixToQuat(Vector3(0, 1, 0), 0.1*PI / 180.0); // obr�t obiektu o k�t 30 stopni wzgl�dem osi y:
	state.qOrient = qObr*state.qOrient;

	iID_collision = -1;           // oznacza, �e aktualnie nie dosz�o do �adnej kolizji
}

MovableObject::~MovableObject()            // destruktor
{
}

void MovableObject::ChangeState(ObjectState __state)  // przepisanie podanego stateu 
{                                                // w przypadku obiekt�w, kt�re nie s� symulowane
	state = __state;
}

ObjectState MovableObject::State()                // metoda zwracaj�ca state obiektu ��cznie z iID
{
	return state;
}



void MovableObject::Simulation(float dt)          // obliczenie nowego stateu na podstawie dotychczasowego,
{                                                // dzia�aj�cych si� i czasu, jaki up�yn�� od ostatniej symulacji

	if (dt == 0) return;

	float g = 9.81;                // przyspieszenie grawitacyjne
	float Fy = mass_own*9.81;        // si�a nacisku na podstaw� obiektu (na ko�a pojazdu)

	// obracam uk�ad wsp�rz�dnych lokalnych wed�ug quaterniona orientacji:
	Vector3 dir_forward = state.qOrient.rotate_vector(Vector3(1, 0, 0)); // na razie o� obiektu pokrywa si� z osi� x globalnego uk�adu wsp�rz�dnych (lokalna o� x)
	Vector3 dir_up = state.qOrient.rotate_vector(Vector3(0, 1, 0));  // wektor skierowany pionowo w g�r� od podstawy obiektu (lokalna o� y)
	Vector3 dir_right = state.qOrient.rotate_vector(Vector3(0, 0, 1)); // wektor skierowany w prawo (lokalna o� z)


	// rzutujemy vV na sk�adow� w kierunku przodu i pozosta�e 2 sk�adowe
	// sk�adowa w bok jest zmniejszana przez si�� tarcia, sk�adowa do przodu
	// przez si�� tarcia tocznego
	Vector3 vV_forward = dir_forward*(state.vV^dir_forward),
		vV_right = dir_right*(state.vV^dir_right),
		vV_up = dir_up*(state.vV^dir_up);

	// rzutujemy pr�dko�� k�tow� vV_ang na sk�adow� w kierunku przodu i pozosta�e 2 sk�adowe
	Vector3 vV_ang_forward = dir_forward*(state.vV_ang^dir_forward),
		vV_ang_right = dir_right*(state.vV_ang^dir_right),
		vV_ang_up = dir_up*(state.vV_ang^dir_up);

	float kat_kol = state.wheel_angle;

	// ruch k� na skutek kr�cenia lub puszczenia kierownicy:  

	if (wheel_turn_speed != 0)
		state.wheel_angle += wheel_turn_speed*dt;
	else
		if (state.wheel_angle > 0)
		{
			if (!if_keep_steer_wheel)
				state.wheel_angle -= wheel_turn_return_speed*dt;
			if (state.wheel_angle < 0) state.wheel_angle = 0;
		}
		else if (state.wheel_angle < 0)
		{
			if (!if_keep_steer_wheel)
				state.wheel_angle += wheel_turn_return_speed*dt;
			if (state.wheel_angle > 0) state.wheel_angle = 0;
		}
	// ograniczenia: 
	if (state.wheel_angle > wheel_angle_max) state.wheel_angle = wheel_angle_max;
	if (state.wheel_angle < -wheel_angle_max) state.wheel_angle = -wheel_angle_max;
	float F_true = F;
	if (F_true > F_max) F_true = F_max;
	if (F_true < -F_max) F_true = -F_max;

	// obliczam promien skr�tu pojazdu na podstawie k�ta skr�tu k�, a nast�pnie na podstawie promienia skr�tu
	// obliczam pr�dko�� k�tow� (UPROSZCZENIE! pomijam przyspieszenie k�towe oraz w�a�ciw� trajektori� ruchu)
	if (Fy > 0)
	{
		float V_ang_turn = 0;
		if (state.wheel_angle != 0)
		{
			float Rs = sqrt(length*length / 4 + (fabs(length / tan(state.wheel_angle)) + width / 2)*(fabs(length / tan(state.wheel_angle)) + width / 2));
			V_ang_turn = vV_forward.length()*(1.0 / Rs);
		}
		Vector3 vV_ang_turn = dir_up*V_ang_turn*(state.wheel_angle > 0 ? 1 : -1);
		Vector3 vV_ang_up2 = vV_ang_up + vV_ang_turn;
		if (vV_ang_up2.length() <= vV_ang_up.length()) // skr�t przeciwdzia�a obrotowi
		{
			if (vV_ang_up2.length() > V_ang_turn)
				vV_ang_up = vV_ang_up2;
			else
				vV_ang_up = vV_ang_turn;
		}
		else
		{
			if (vV_ang_up.length() < V_ang_turn)
				vV_ang_up = vV_ang_turn;
		}

		// friction zmniejsza pr�dko�� obrotow� (UPROSZCZENIE! zamiast masy winienem wykorzysta� moment bezw�adno�ci)     
		float V_ang_friction = Fy*friction_rot*dt / mass_own / 1.0;      // zmiana pr. k�towej spowodowana frictionm
		float V_ang_up = vV_ang_up.length() - V_ang_friction;
		if (V_ang_up < V_ang_turn) V_ang_up = V_ang_turn;        // friction nie mo�e spowodowa� zmiany zwrotu wektora pr. k�towej
		vV_ang_up = vV_ang_up.znorm()*V_ang_up;
	}


	Fy = mass_own*g*dir_up.y;                      // si�a docisku do pod�o�a 
	if (Fy < 0) Fy = 0;
	// ... trzeba j� jeszcze uzale�ni� od tego, czy obiekt styka si� z pod�o�em!
	float Fh = Fy*friction_linear*breaking_factor;                  // si�a hamowania (UP: bez uwzgl�dnienia po�lizgu)

	float V_up = vV_forward.length();// - dt*Fh/m - dt*friction_roll*Fy/m;
	if (V_up < 0) V_up = 0;

	float V_right = vV_right.length();// - dt*friction*Fy/m;
	if (V_right < 0) V_right = 0;

	float V = state.vV.length();

	// wjazd lub zjazd: 
	//vPos.y = terrain.DistFromGround(vPos.x,vPos.z);   // najprostsze rozwi�zanie - obiekt zmienia wysoko�� bez zmiany orientacji

	// 1. gdy wjazd na wkl�s�o��: wyznaczam wysoko�ci terrainu pod naro�nikami obiektu (ko�ami), 
	// sprawdzam kt�ra tr�jka
	// naro�nik�w odpowiada najni�ej po�o�onemu �rodkowi ci�ko�ci, gdy przylega do terrainu
	// wyznaczam pr�dko�� podbicia (wznoszenia �rodka pojazdu spowodowanego wkl�s�o�ci�) 
	// oraz pr�dko�� k�tow�
	// 2. gdy wjazd na wypuk�o�� to si�a ci�ko�ci wywo�uje obr�t przy du�ej pr�dko�ci liniowej

	// punkty zaczepienia k� (na wysoko�ci pod�ogi pojazdu):
	Vector3 P = state.vPos + dir_forward*(length / 2 - front_axis_dist) - dir_right*width / 2 - dir_up*height / 2,
		Q = state.vPos + dir_forward*(length / 2 - front_axis_dist) + dir_right*width / 2 - dir_up*height / 2,
		R = state.vPos + dir_forward*(-length / 2 + back_axis_dist) - dir_right*width / 2 - dir_up*height / 2,
		S = state.vPos + dir_forward*(-length / 2 + back_axis_dist) + dir_right*width / 2 - dir_up*height / 2;

	// pionowe rzuty punkt�w zacz. k� pojazdu na powierzchni� terrainu:  
	Vector3 Pt = P, Qt = Q, Rt = R, St = S;
	Pt.y = terrain.DistFromGround(P.x, P.z); Qt.y = terrain.DistFromGround(Q.x, Q.z);
	Rt.y = terrain.DistFromGround(R.x, R.z); St.y = terrain.DistFromGround(S.x, S.z);
	Vector3 normPQR = normal_vector(Pt, Rt, Qt), normPRS = normal_vector(Pt, Rt, St), normPQS = normal_vector(Pt, St, Qt),
		normQRS = normal_vector(Qt, Rt, St);   // normalne do p�aszczyzn wyznaczonych przez tr�jk�ty

	//fprintf(f, "P.y = %f, Pt.y = %f, Q.y = %f, Qt.y = %f, R.y = %f, Rt.y = %f, S.y = %f, St.y = %f\n",
	//	P.y, Pt.y, Q.y, Qt.y, R.y, Rt.y, S.y, St.y);

	float sryPQR = ((Qt^normPQR) - normPQR.x*state.vPos.x - normPQR.z*state.vPos.z) / normPQR.y, // wys. �rodka pojazdu
		sryPRS = ((Pt^normPRS) - normPRS.x*state.vPos.x - normPRS.z*state.vPos.z) / normPRS.y, // po najechaniu na skarp� 
		sryPQS = ((Pt^normPQS) - normPQS.x*state.vPos.x - normPQS.z*state.vPos.z) / normPQS.y, // dla 4 tr�jek k�
		sryQRS = ((Qt^normQRS) - normQRS.x*state.vPos.x - normQRS.z*state.vPos.z) / normQRS.y;
	float sry = sryPQR; Vector3 norm = normPQR;
	if (sry > sryPRS) { sry = sryPRS; norm = normPRS; }
	if (sry > sryPQS) { sry = sryPQS; norm = normPQS; }
	if (sry > sryQRS) { sry = sryQRS; norm = normQRS; }  // wyb�r tr�jk�ta o �rodku najni�ej po�o�onym    

	Vector3 vV_ang_horizontal = Vector3(0, 0, 0);
	// jesli kt�re� z k� jest poni�ej powierzchni terrainu
	if ((P.y <= Pt.y + height / 2 + clearance) || (Q.y <= Qt.y + height / 2 + clearance) ||
		(R.y <= Rt.y + height / 2 + clearance) || (S.y <= St.y + height / 2 + clearance))
	{
		// obliczam powsta�� pr�dko�� k�tow� w lokalnym uk�adzie wsp�rz�dnych:      
		Vector3 v_rotation = -norm.znorm()*dir_up*0.6;
		vV_ang_horizontal = v_rotation / dt;
	}

	Vector3 vAg = Vector3(0, -1, 0)*g;    // przyspieszenie grawitacyjne

	// jesli wiecej niz 2 kola sa na ziemi, to przyspieszenie grawitacyjne jest rownowazone przez opor gruntu:
	if ((P.y <= Pt.y + height / 2 + clearance) + (Q.y <= Qt.y + height / 2 + clearance) +
		(R.y <= Rt.y + height / 2 + clearance) + (S.y <= St.y + height / 2 + clearance) > 2)
		vAg = vAg + dir_up*(dir_up^vAg)*-1; //przyspieszenie resultaj�ce z si�y oporu gruntu
	else   // w przeciwnym wypadku brak sily docisku 
		Fy = 0;


	// sk�adam z powrotem wektor pr�dko�ci k�towej: 
	//state.vV_ang = vV_ang_up + vV_ang_right + vV_ang_forward;  
	state.vV_ang = vV_ang_up + vV_ang_horizontal;


	float h = sry + height / 2 + clearance - state.vPos.y;  // r�nica wysoko�ci jak� trzeba pokona�  
	float V_podbicia = 0;
	if ((h > 0) && (state.vV.y <= 0.01))
		V_podbicia = 0.5*sqrt(2 * g*h);  // pr�dko�� spowodowana podbiciem pojazdu przy wje�d�aniu na skarp� 
	if (h > 0) state.vPos.y = sry + height / 2 + clearance;

	// lub  w przypadku zag��bienia si� 
	Vector3 dvPos = state.vV*dt + state.vA*dt*dt / 2; // czynnik bardzo ma�y - im wi�ksza cz�stotliwo�� symulacji, tym mniejsze znaczenie 
	state.vPos = state.vPos + dvPos;

	

	// korekta po�o�enia w przypadku terrainu cyklicznego:
	if (state.vPos.x < -terrain.field_size*terrain.number_of_columns / 2) state.vPos.x += terrain.field_size*terrain.number_of_columns;
	else if (state.vPos.x > terrain.field_size*(terrain.number_of_columns - terrain.number_of_columns / 2)) state.vPos.x -= terrain.field_size*terrain.number_of_columns;
	if (state.vPos.z < -terrain.field_size*terrain.number_of_rows / 2) state.vPos.z += terrain.field_size*terrain.number_of_rows;
	else if (state.vPos.z > terrain.field_size*(terrain.number_of_rows - terrain.number_of_rows / 2)) state.vPos.z -= terrain.field_size*terrain.number_of_rows;

	// Sprawdzenie czy obiekt mo�e si� przemie�ci� w zadane miejsce: Je�li nie, to 
	// przemieszczam obiekt do miejsca zetkni�cia, wyznaczam nowe wektory pr�dko�ci
	// i pr�dko�ci k�towej, a nast�pne obliczam nowe po�o�enie na podstawie nowych
	// pr�dko�ci i pozosta�ego czasu. Wszystko powtarzam w p�tli (pojazd znowu mo�e 
	// wjecha� na przeszkod�). Problem z zaokr�glonymi przeszkodami - konieczne 
	// wyznaczenie minimalnego kroku.


	Vector3 vV_pop = state.vV;

	// sk�adam pr�dko�ci w r�nych kierunkach oraz efekt przyspieszenia w jeden wektor:    (problem z przyspieszeniem od si�y tarcia -> to przyspieszenie 
	//      mo�e dzia�a� kr�cej ni� dt -> trzeba to jako� uwzgl�dni�, inaczej pojazd b�dzie w�ykowa�)
	state.vV = vV_forward.znorm()*V_up + vV_right.znorm()*V_right + vV_up +
		Vector3(0, 1, 0)*V_podbicia + state.vA*dt;
	// usuwam te sk�adowe wektora pr�dko�ci w kt�rych kierunku jazda nie jest mo�liwa z powodu
	// przesk�d:
	// np. je�li pojazd styka si� 3 ko�ami z nawierzchni� lub dwoma ko�ami i �rodkiem ci�ko�ci to
	// nie mo�e mie� pr�dko�ci w d� pod�ogi
	if ((P.y <= Pt.y + height / 2 + clearance) || (Q.y <= Qt.y + height / 2 + clearance) ||
		(R.y <= Rt.y + height / 2 + clearance) || (S.y <= St.y + height / 2 + clearance))    // je�li pojazd styka si� co najm. jednym ko�em
	{
		Vector3 dvV = vV_up + dir_up*(state.vA^dir_up)*dt;
		if ((dir_up.znorm() - dvV.znorm()).length() > 1)  // je�li wektor skierowany w d� pod�ogi
			state.vV = state.vV - dvV;
	}

	// sk�adam przyspieszenia liniowe od si� nap�dzaj�cych i od si� oporu: 
	state.vA = (dir_forward*F_true + dir_right*Fb) / mass_own*(Fy > 0)  // od si� nap�dzaj�cych
		- vV_forward.znorm()*(Fh / mass_own + friction_roll*Fy / mass_own)*(V_up > 0.01) // od hamowania i tarcia tocznego (w kierunku ruchu)
		- vV_right.znorm()*friction_linear*Fy / mass_own*(V_right > 0.01)    // od tarcia w kierunku prost. do kier. ruchu
		- vV_pop.znorm()*V*V*friction_air                  // od oporu powietrza
		+ vAg;           // od grawitacji


	// obliczenie nowej orientacji:
	Vector3 w_obrot = state.vV_ang*dt + state.vA_ang*dt*dt / 2;
	quaternion q_obrot = AsixToQuat(w_obrot.znorm(), w_obrot.length());
	state.qOrient = q_obrot*state.qOrient;

	float radius = sqrt(this->width*width + this->height*height + this->length*length) / 2;

	number_of_simulations++;
	if (number_of_simulations > number_of_cyc + 100) {
		long time_curr = clock();
		while (clock() - time_curr < 1000);
		state.vPos = Vector3(rand(), rand(), rand());
	}
	
}

void MovableObject::DrawObject()
{
	glPushMatrix();


	glTranslatef(state.vPos.x, state.vPos.y + clearance, state.vPos.z);

	Vector3 k = state.qOrient.AsixAngle();     // reprezentacja k�towo-osiowa quaterniona

	Vector3 k_znorm = k.znorm();

	glRotatef(k.length()*180.0 / PI, k_znorm.x, k_znorm.y, k_znorm.z);
	glTranslatef(-length / 2, -height / 2, -width / 2);
	glScalef(length, height, width);

	glCallList(Auto);
	GLfloat Surface[] = { 2.0f, 2.0f, 1.0f, 1.0f };
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, Surface);
	if (if_ID_visible){
		glRasterPos2f(0.30, 1.20);
		glPrint("%d", iID);
	}
	glPopMatrix();
}




//**********************
//   Obiekty nieruchome
//**********************
Terrain::Terrain()
{
   field_size = 60;         // d�ugo�� boku kwadratu w [m]           

   float t[][44] = { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // ostatni element nieu�ywany
   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 80, 170, 170, 200, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 5, 5, 2, 0, 0, 0, 0, 0, 0, 0, 120, 220, 250, 200, 200, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 6, 9, 12, 12, 4, 0, 1, 0, 0, 0, 40, 130, 200, 250, 200, 150, 0, 0, 0, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 0, 0, 14, 9, 4, 0, 0, 0, 20, 40, 120, 200, 220, 150, 150, 0, 0, 0, 0, 50, 50, 300, 300, 300, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 2, 2, 0, 0, 8, 4, 0, 0, 0, 0, 20, 40, 90, 120, 170, 0, 0, 0, 0, 0, 0, 60, 300, 350, 330, 300, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, -1, 2, 0, -3, 0, 2, 1, 2, 0, 0, 0, 0, 0, 0, 10, 20, 30, 40, 50, 100, 140, 0, 0, 0, 0, 0, 0, 50, 300, 300, 300, 150, 50, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, -1, 2, 0, 0, 0, 0, -1, -1, 2, 0, 0, 0, 0, 0, 0, 0, 10, 10, 40, 70, 100, 110, 0, 0, 0, 0, 0, 0, 50, 40, 300, 200, 50, 50, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 10, 10, 90, 90, 0, 0, 0, 0, 0, 0, 0, 100, 40, 100, 50, 50, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 1, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -10, -10, 10, 10, 0, 0, 0, 0, 0, 0, 0, 100, 100, 0, 100, 70, 40, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 1, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -10, 40, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 70, 70, 30, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 70, 70, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 70, 20, 20, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 70, 70, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0, 0, 0, -2, -2, -2, -2, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, 70, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0, 0, 0, -6, -5, -5, -3, -3, 0, 0, 0, 0, 0, 0, -2, -2, -1, 0, 70, 70, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0, 0, -7, -6, -3, -3, -5, -4, 0, 0, 0, 0, 0, -1, -3, -3, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0, 0, -8, -8, 0, 0, 0, -4, -2, 0, 0, 0, 0, 0, -2, -3, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0, 0, -8, 0, 0, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, -40, -40, -40, -10, -40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, -40, -40, -40, -40, -40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, -40, -40, -40, -40, -40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, -40, -40, -40, -40, -40, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 10, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, -40, -40, -40, -40, -40, 0, 0, 8, 10, -10, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 10, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, -40, -40, -40, -40, 0, 8, 10, -20, -10, 0, 0, 0, 7, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 10, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, -40, -40, -40, -40, 0, 8, 16, 10, -10, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 10, 10, 10, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 10, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0, -20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 10, 0, 10, 10, 10, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 10, 10, 10, 3, 0, 0, 0, 0, 0, 0, -20, -20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 10, 10, 20, 20, 10, 5, 5, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 10, 10, 5, 0, 0, 0, 0, 0, -40, -40, -40, -20, -30, -30, 0, 0, 0, 0, 0, 0, 0, 0, 10, 20, 20, 20, 10, 5, 5, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, -3, 0, 0, -10, -10, 0, 2, 10, 5, 0, 0, 1, 0, 0, -40, -40, -40, -40, -30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 20, 20, 10, 5, 5, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, -3, 0, -13, -10, -6, 0, 0, 5, 0, 0, 1, 3, 0, 0, -40, -40, -40, -40, -30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 20, 20, 10, 5, 5, 5, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0, -3, 0, -18, -16, 0, 0, 0, 0, 0, 0, 2, 3, 5, -40, -40, -40, -40, -30, -20, -20, 0, 0, 0, 0, 0, 0, 0, 0, 20, 20, 20, 10, 5, 5, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 4, 0, -40, -40, -40, -40, -30, -20, -20, -20, 0, 0, 0, 0, 3, 5, 10, 20, 20, 20, 10, 5, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 5, -40, -40, -40, -40, -30, -20, -20, -20, 0, 0, 0, 0, 3, 5, 10, 20, 20, 20, 10, 5, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, -40, -40, -30, -30, -30, -20, -20, 0, 0, 0, 0, 3, 5, 10, 20, 20, 20, 10, 5, 0, 20, 20, 0, 0 },
   { 0, 0, 0, 0, 0, 0, 0, -3, -3, 0, 0, 0, 0, 0, 0, 0, 4, 4, 0, 0, 0, -20, -20, -10, -10, -10, -10, 0, 0, 0, 0, 0, 3, 0, 20, 20, 20, 10, 0, 0, 20, 20, 0, 0 },
   { 0, 0, 3, 0, 0, 0, -3, -5, -3, 0, 0, 0, 0, 0, 0, 2, 4, 2, 0, 0, 0, 0, -20, -10, -5, -5, -5, 0, 0, 0, 0, 0, 0, 0, 20, 20, 20, 10, 0, 0, 20, 20, 0, 0 },
   { 0, 0, 3, 0, 0, 0, -3, -5, -3, 0, 0, 0, 0, 0, 0, 0, 4, 4, 0, 0, 0, 0, 0, 0, -5, -5, -5, 0, 0, 0, 0, 0, 0, 0, 20, 20, 30, 30, 30, 20, 20, 20, 20, 0 },
   { 0, 0, 3, 0, 0, -3, -3, -3, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, -5, -5, -5, 0, 0, 0, 0, 0, 0, 20, 20, 40, 40, 40, 40, 40, 40, 40, 40, 0 },
   { 0, 0, 0, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -5, -5, -5, 0, 0, 0, 0, 0, 0, 20, 30, 40, 40, 60, 60, 60, 60, 40, 40, 0 },
   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 20, 30, 40, 40, 60, 60, 60, 60, 40, 40, 0 },
   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 20, 30, 40, 40, 60, 60, 60, 60, 40, 40, 0 },
   { 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, -50, 0, 0, 0, 0, 1, -1, 0, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 30, 20, 30, 40, 60, 60, 60, 60, 60, 40, 40, 0 },
   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -20, -50, 0, 0, 0, 0, 1, -1, 0, 0, 1, 5, 8, 0, 0, 0, 0, 0, 0, 0, 0, 30, 30, 40, 60, 60, 60, 60, 60, 60, 60, 40, 0 },
   { 0, 0, 10, 0, 0, 0, 2, 2, 2, 1, -20, -20, -30, 0, 0, 0, 0, 1, -1, 0, 0, 2, 5, 9, 0, 0, 0, 0, 0, 0, 0, 20, 30, 40, 60, 60, 100, 100, 100, 60, 60, 60, 40, 0 },
   { 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, -20, -10, 0, 0, 0, 0, 0, 1, -1, 0, 0, 2, 5, 7, 0, 0, 0, 0, 0, 0, 20, 30, 40, 60, 100, 100, 100, 100, 100, 60, 60, 40, 0 },
   { 0, 0, 10, 0, 0, 4, 4, 2, 3, 2, 1, -5, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 2, 4, 0, 0, 0, 0, 0, 0, 0, 20, 30, 40, 60, 100, 100, 100, 120, 100, 100, 60, 40, 0 },
   { 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 30, 40, 60, 100, 100, 80, 80, 100, 100, 60, 40, 0 },
   { 0, 0, 10, 0, 0, 4, 3, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 30, 40, 60, 100, 100, 100, 80, 100, 100, 60, 40, 0 },
   { 0, 0, 10, 0, 0, 0, 0, 0.5, 1, 1, 0, 0, 0, 0, 0, 0, -30, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 40, 60, 60, 100, 100, 100, 100, 60, 60, 40, 0 },
   { 0, 0, 10, 0, 0, 4, 4, 2, 3, 1, 1, 1, 0, 0, 0, -30, -30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 60, 60, 100, 100, 60, 60, 60, 40, 0 },
   { 0, 0, 10, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, -30, -30, -30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 60, 60, 60, 60, 60, 60, 60, 40, 0 },
   { 0, 0, 10, 0, 0, 5, 4, 2, 2, 1, 1, 1, 0, 0, 0, -30, -30, -25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 20, 20, 40, 20, 40, 40, 40, 40, 0 },
   { 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -25, -22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 20, 40, 20, 20, 40, 40, 0, 0 },
   { 0, 0, 10, 0, 0, -20, 60, -20, 0, 0, 0, 0, 0, 0, 0, 10, 0, -22, -20, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 20, 20, 0, 0, 0, 0 },
   { 0, 0, 10, 0, 0, 0, 70, 60, -20, 0, 0, 0, 0, 0, 0, 10, 10, 0, -19, -18, 0, -6, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 20, 20, 0, 0, 0, 0 },
   { 0, 0, 10, 0, 0, 0, 65, 50, 0, 0, 0, 0, 0, 0, 5, 10, 0, 0, -16, -13, -8, -6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 20, 0, 0, 0, 0, 0 },
   { 0, 0, 10, 0, 0, 0, 0, -20, 0, 0, 0, 0, 0, 0, 2, 5, 0, 0, 0, -13, -10, -8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, -10, -20, -60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, -10, -20, -30, -60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, -10, -40, -90, -60, -60, 0, 0, 0, 0, 0, 0, 0, 10, 10, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, -20, -40, -90, -90, -60, 0, 0, 0, 0, 0, 0, 10, 10, 10, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, -10, -40, -90, -90, -90, -60, 0, 0, 0, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, -10, -50, -90, -90, -90, -60, 0, 0, 0, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 40, 40, -20, -10, 0, 0, 0, 0, 0 },
   { 0, -10, -50, -70, -90, -60, -40, 0, 0, 0, 10, 10, 10, 20, 20, 20, 20, 30, 30, 40, 40, 50, 50, 70, 70, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 40, -20, -10, 0, 0, 0, 0, 0 },
   { 0, -10, -20, -40, -40, -40, -40, 0, 0, 0, 10, 10, 10, 20, 20, 20, 20, 30, 30, 40, 40, 50, 50, 70, 70, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 40, 40, 0, 0, 0, 0, 0, 0 },
   { 0, -10, -20, -20, -30, -20, -20, -10, 0, 0, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 40, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, -10, -20, -20, -10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, -10, -10, -10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } };

   number_of_columns = 43;         // o 1 mniej, gdy� kolumna jest r�wnowa�na ci�gowi kwadrat�w 
   number_of_rows = sizeof(t) / sizeof(float) / (number_of_columns + 1) / 2 - 1;
   height_map = new float*[number_of_rows * 2 + 1];
   for (long i = 0; i<number_of_rows * 2 + 1; i++) {
	   height_map[i] = new float[number_of_columns + 1];
	   for (long j = 0; j<number_of_columns + 1; j++) height_map[i][j] = (t[i][j] != 0 ? i % 5 + j% 10 : 0) + t[i][j] / 2.5;
   }


   d = new float**[number_of_rows];
   for (long i=0;i<number_of_rows;i++) {
       d[i] = new float*[number_of_columns];
       for (long j=0;j<number_of_columns;j++) d[i][j] = new float[4];
   }    
   Norm = new Vector3**[number_of_rows];
   for (long i=0;i<number_of_rows;i++) {
       Norm[i] = new Vector3*[number_of_columns];
       for (long j=0;j<number_of_columns;j++) Norm[i][j] = new Vector3[4];
   }    
       
   fprintf(f,"height_map terrain: number_of_rows = %d, number_of_columns = %d\n",number_of_rows,number_of_columns);
}

Terrain::~Terrain()
{
  for (long i = 0;i< number_of_rows*2+1;i++) delete height_map[i];             
  delete height_map;   
  for (long i=0;i<number_of_rows;i++)  {
      for (long j=0;j<number_of_columns;j++) delete d[i][j];
      delete d[i];
  }
  delete d;  
  for (long i=0;i<number_of_rows;i++)  {
      for (long j=0;j<number_of_columns;j++) delete Norm[i][j];
      delete Norm[i];
  }
  delete Norm;  

         
}

float Terrain::DistFromGround(float x,float z)      // okre�lanie wysoko�ci dla punktu o wsp. (x,z) 
{
  
  float x_begin = -field_size*number_of_columns/2,     // wsp�rz�dne lewego g�rnego kra�ca terrainu
        z_begin = -field_size*number_of_rows/2;        
  
  long k = (long)((x - x_begin)/field_size), // wyznaczenie wsp�rz�dnych (w,k) kwadratu
       w = (long)((z - z_begin)/field_size);
  //if ((k < 0)||(k >= number_of_rows)||(w < 0)||(w >= number_of_columns)) return -1e10;  // je�li poza map�

  // korekta numeru kolumny lub wiersza w przypadku terrainu cyklicznego
  if (k<0) while (k<0) k += number_of_columns;
  else if (k > number_of_columns - 1) while (k > number_of_columns - 1) k -= number_of_columns;
  if (w<0) while (w<0) w += number_of_rows;
  else if (w > number_of_rows - 1) while (w > number_of_rows - 1) w -= number_of_rows;
  
  // wyznaczam punkt B - �rodek kwadratu oraz tr�jk�t, w kt�rym znajduje si� punkt
  // (rysunek w Terrain::DrawInitialisation())
  Vector3 B = Vector3(x_begin + (k+0.5)*field_size, height_map[w*2+1][k], z_begin + (w+0.5)*field_size); 
  enum tr{ABC=0,ADB=1,BDE=2,CBE=3};       // tr�jk�t w kt�rym znajduje si� punkt 
  int triangle=0; 
  if ((B.x > x)&&(fabs(B.z - z) < fabs(B.x - x))) triangle = ADB;
  else if ((B.x < x)&&(fabs(B.z - z) < fabs(B.x - x))) triangle = CBE;
  else if ((B.z > z)&&(fabs(B.z - z) > fabs(B.x - x))) triangle = ABC;
  else triangle = BDE;
  
  // wyznaczam normaln� do p�aszczyzny a nast�pnie wsp�czynnik d z r�wnania p�aszczyzny
  float dd = d[w][k][triangle];
  Vector3 N = Norm[w][k][triangle];
  float y;
  if (N.y > 0) y = (-dd - N.x*x - N.z*z)/N.y;
  else y = 0;
  
  return y;    
}

void Terrain::DrawInitialisation()
{
  // tworze list� wy�wietlania rysuj�c poszczeg�lne pola mapy za pomoc� tr�jk�t�w 
  // (po 4 tr�jk�ty na ka�de pole):
  enum tr{ABC=0,ADB=1,BDE=2,CBE=3};       
  float x_begin = -field_size*number_of_columns/2,     // wsp�rz�dne lewego g�rnego kra�ca terrainu
        z_begin = -field_size*number_of_rows/2;        
  Vector3 A,B,C,D,E,N;      
  glNewList(TerrainMap,GL_COMPILE);
  glBegin(GL_TRIANGLES);
    
    for (long w=0;w<number_of_rows;w++) 
      for (long k=0;k<number_of_columns;k++) 
      {
          A = Vector3(x_begin + k*field_size, height_map[w*2][k], z_begin + w*field_size);
          B = Vector3(x_begin + (k+0.5)*field_size, height_map[w*2+1][k], z_begin + (w+0.5)*field_size);            
          C = Vector3(x_begin + (k+1)*field_size, height_map[w*2][k+1], z_begin + w*field_size); 
          D = Vector3(x_begin + k*field_size, height_map[(w+1)*2][k], z_begin + (w+1)*field_size);       
          E = Vector3(x_begin + (k+1)*field_size, height_map[(w+1)*2][k+1], z_begin + (w+1)*field_size); 
          // tworz� tr�jk�t ABC w g�rnej cz�ci kwadratu: 
          //  A o_________o C
          //    |.       .|
          //    |  .   .  | 
          //    |    o B  | 
          //    |  .   .  |
          //    |._______.|
          //  D o         o E
          
          Vector3 AB = B-A;
          Vector3 BC = C-B;
          N = (AB*BC).znorm();          
          glNormal3f( N.x, N.y, N.z);
		  glVertex3f( A.x, A.y, A.z);
		  glVertex3f( B.x, B.y, B.z);
          glVertex3f( C.x, C.y, C.z);
          d[w][k][ABC] = -(B^N);          // dodatkowo wyznaczam wyraz wolny z r�wnania plaszyzny tr�jk�ta
          Norm[w][k][ABC] = N;          // dodatkowo zapisuj� normaln� do p�aszczyzny tr�jk�ta
          // tr�jk�t ADB:
          Vector3 AD = D-A;
          N = (AD*AB).znorm();          
          glNormal3f( N.x, N.y, N.z);
		  glVertex3f( A.x, A.y, A.z);
		  glVertex3f( D.x, D.y, D.z);
		  glVertex3f( B.x, B.y, B.z);
		  d[w][k][ADB] = -(B^N);       
          Norm[w][k][ADB] = N;
		  // tr�jk�t BDE:
          Vector3 BD = D-B;
          Vector3 DE = E-D;
          N = (BD*DE).znorm();          
          glNormal3f( N.x, N.y, N.z);
		  glVertex3f( B.x, B.y, B.z);
          glVertex3f( D.x, D.y, D.z);     
          glVertex3f( E.x, E.y, E.z);  
          d[w][k][BDE] = -(B^N);        
          Norm[w][k][BDE] = N;  
          // tr�jk�t CBE:
          Vector3 CB = B-C;
          Vector3 BE = E-B;
          N = (CB*BE).znorm();          
          glNormal3f( N.x, N.y, N.z);
          glVertex3f( C.x, C.y, C.z);
		  glVertex3f( B.x, B.y, B.z);
          glVertex3f( E.x, E.y, E.z);      
          d[w][k][CBE] = -(B^N);        
          Norm[w][k][CBE] = N;
      }		

  glEnd();
  glEndList(); 
                 
}

// wczytanie powierzchni terenu (mapy wysoko�ci) oraz przedmiot�w  
int Terrain::ReadMap(char filename[128])
{
	int mode_reading_things = 0, mode_reading_map = 0, mode_reading_row = 0,
		nr_of_row_point = -1, nr_of_column_point = -1;   // liczby wierszy i kolumn punkt�w 
	height_map = NULL;
   
	this->number_of_rows = this->number_of_columns = 0;  // liczby wierszy i kolumn czw�rek tr�jk�t�w

	FILE *pl = fopen(filename, "r");

	if (pl)
	{
		char line[1024], writing[128];
		long long_number;
		Vector3 wektor;
		quaternion kw;
		float float_number;
		while (fgets(line, 1024, pl))
		{
			sscanf(line, "%s", &writing);
			
			if (strcmp(writing, "<mapa>") == 0)
			{
				mode_reading_map = 1;
			}
			
			if (mode_reading_map)
			{
				if (strcmp(writing, "<liczba_wierszy") == 0)
				{
					sscanf(line, "%s %d ", &writing, &long_number);
					this->number_of_rows = long_number;
				}
				else if (strcmp(writing, "<liczba_kolumn") == 0)
				{
					sscanf(line, "%s %d ", &writing, &long_number);
					this->number_of_columns = long_number;
				}
				else if (strcmp(writing, "<wiersz_punktow") == 0)
				{
					mode_reading_row = 1;
					sscanf(line, "%s %d ", &writing, &long_number);
					nr_of_row_point = long_number;
					nr_of_column_point = 0;
				}
				else if (strcmp(writing, "</mapa>") == 0)
				{
					mode_reading_map = 0;
				}

				if (mode_reading_row)
				{
					if (strcmp(writing, "<w") == 0)
					{
						sscanf(line, "%s %f ", &writing, &float_number);
						height_map[nr_of_row_point][nr_of_column_point] = float_number;
						nr_of_column_point++;
					}
					else if (strcmp(writing, "</wiersz_punktow>") == 0)
					{
						mode_reading_row = 0;
					}
				}

			} // tryb odczytu mapy wierzcho�k�w

			// pami�� dla mapy terenu:
			if ((this->number_of_rows > 0) && (this->number_of_columns > 0) && (height_map == NULL))
			{
				height_map = new float*[number_of_rows * 2 + 1];
				for (long i = 0; i<number_of_rows * 2 + 1; i++) {
					height_map[i] = new float[number_of_columns + 1];
					for (long j = 0; j<number_of_columns + 1; j++) height_map[i][j] = 0;
				}
			}

		}
		fclose(pl);
	}
	else return 0;

	return 1;
}


void Terrain::Draw()
{
  glCallList(TerrainMap);                  
}

// k�t pomi�dzy pojazdami na podstawie kwaternion�w orientacji   
float AngleBetweenQuats(quaternion q1, quaternion q2)
{
	// obliczenie �redniej r�nicy k�towej:
	float angle = fabs(angle_between_vectors(q1.rotate_vector(Vector3(1, 0, 0)), q2.rotate_vector(Vector3(1, 0, 0))));
	angle = (angle > 3.14159 ? fabs(angle - 2 * 3.14159) : fabs(angle));
	return angle;
}

// odleg�o�� pomi�dzy punktami w �wiecie toroidalnym (wymaga uwzgl�dnienia przeskok�w pomi�dzy ko�cem i pocz�tkiem)
float DistanceBetweenPointsOnTetraMap(Vector3 p1, Vector3 p2)
{
	float size_x = terrain.number_of_columns*terrain.field_size,    // czy na pewno tutaj jest liczba kolumn -> potencjalny b��d!!!
		size_z = terrain.number_of_rows*terrain.field_size;
	float dx = p1.x - p2.x;
	if (dx > size_x / 2) dx = size_x - dx;
	if (dx < -size_x / 2) dx = size_x + dx;
	float dz = p1.z - p2.z;
	if (dz > size_z / 2) dz = size_z - dz;
	if (dz < -size_z / 2) dz = size_z + dz;
	float dy = p1.y - p2.y;

	return sqrt(dx*dx + dy*dy + dz*dz);
}

// realizacja kroku scenariusza dla podanego obiektu, scenariusza i czasu od pocz�tku
// zwraca informacj� czy scenariusz dobieg� ko�ca, umieszcza w obj parametry sterowania (si�a, predko�� skr�tu k�, stopie� ham.)
bool test_scenario_step(MovableObject *obj, float test_scenario[][4], int number_of_actions, float __time)
{
	long x = sizeof(test_scenario);
	//long number_of_actions = sizeof(test_scenario) / (4 * sizeof(float));
	float sum_of_periods = 0;

	long nr_akcji = -1;
	for (long i = 0; i<number_of_actions; i++)
	{
		sum_of_periods += test_scenario[i][0];
		if (__time < sum_of_periods) { nr_akcji = i; break; }
	}

	//fprintf(f, "liczba akcji = %d, czas = %f, nr akcji = %d\n", number_of_actions, curr_time, nr_akcji);

	if (nr_akcji > -1) // jesli wyznaczono nr akcji, wybieram sile i kat ze scenariusza
	{
		obj->F = test_scenario[nr_akcji][1];
		obj->wheel_turn_speed = test_scenario[nr_akcji][2];
		obj->breaking_factor = test_scenario[nr_akcji][3];
	}

	return (nr_akcji == -1);
}