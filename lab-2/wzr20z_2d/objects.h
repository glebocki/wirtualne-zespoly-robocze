#include <stdio.h>
#include "quaternion.h"

#define PI 3.1415926

struct ObjectState
{
	Vector3 vPos;              // polozenie obiektu (�rodka geometrycznego obiektu) 
	quaternion qOrient;        // orientacja (polozenie katowe)
	Vector3 vV, vA;            // predkosc, przyspiesznie liniowe
	Vector3 vV_ang, vA_ang;    // predkosc i przyspieszenie liniowe
	float wheel_angle;         // kat skretu kol w radianach (w lewo - dodatni)
};

// Klasa opisuj�ca obiekty ruchome
class MovableObject
{
public:
	int iID;                  // identyfikator obiektu

	ObjectState state;

	float F, Fb;                    // si�y dzia�aj�ce na obiekt: F - pchajaca do przodu, Fb - w prawo (silnik rakietowy)
	float breaking_factor;          // stopie� hamowania Fh_max = friction*Fy*ham
	float wheel_turn_speed;        // pr�dko�� kr�cenia kierownic�
	bool if_keep_steer_wheel;       // czy kierownica jest trzymana (pr�dko�� mo�e by� zerowa, a kierownica trzymana w pewnym po�o�eniu nie wraca do po�. zerowego)

	float mass_own;				    // masa w�asna obiektu	
	float length, width, height;    // rozmiary w kierunku lokalnych osi x,y,z
	float clearance;                // wysoko�� na kt�rej znajduje si� podstawa obiektu
	float front_axis_dist;          // odleg�o�� od przedniej osi do przedniego zderzaka 
	float back_axis_dist;           // odleg�o�� od tylniej osi do tylniego zderzaka	
	float wheel_turn_return_speed;    // pr�dko�� powrotu kierownicy po puszczeniu
	float friction_linear;          // wsp�czynnik tarcia obiektu o pod�o�e 
	float friction_rot;             // tarcie obrotowe obrotowe opon po pod�o�u (w szczeg�lnych przypadkach mo�e by� inne ni� liniowe)
	float friction_roll;            // wsp�czynnik tarcia tocznego
	float friction_air;             // wsp�czynnik oporu powietrza (si�a zale�y od kwadratu pr�dko�ci)
	float elasticity;               // wsp�czynnik spr�ysto�ci (0-brak spr�ysto�ci, 1-doskona�a spr�ysto��) 
	float wheel_angle_max;          // maksymalny skr�t k�
	float F_max;                    // maksymalna si�a wytwarzana przez silnik

	int iID_collision;              // identyfikator pojazdu, z kt�rym nast�pi�a kolizja (je�li nie: -1) 
	Vector3 dV_collision;           // poprawka pr�dko�ci innego obiektu po kolizji  
public:
	MovableObject();          // konstruktor
	~MovableObject();
	void ChangeState(ObjectState state);          // zmiana stateu obiektu
	ObjectState State();        // metoda zwracajaca state obiektu
	void Simulation(float dt);  // symulacja ruchu obiektu w oparciu o biezacy state, przylozone sily
	// oraz czas dzialania sil. Efektem symulacji jest nowy state obiektu 
	void DrawObject();			   // odrysowanie obiektu					
};

// Klasa opisuj�ca terrain, po kt�rym poruszaj� si� obiekty
class Terrain
{
public:
	float **height_map;          // wysoko�ci naro�nik�w oraz �rodk�w p�l
	float ***d;            // warto�ci wyrazu wolnego z r�wnania p�aszczyzny dla ka�dego tr�jk�ta
	Vector3 ***Norm;       // normalne do p�aszczyzn tr�jk�t�w
	float field_size;    // length boku kwadratowego pola na mapie
	long number_of_rows, number_of_columns; // liczba wierszy i kolumn mapy (kwadrat�w na wysoko�� i szeroko��)     
	Terrain();
	~Terrain();
	float DistFromGround(float x, float z);      // okre�lanie wysoko�ci dla punktu o wsp. (x,z) 
	void Draw();	                      // odrysowywanie terrainu   
	void DrawInitialisation();               // tworzenie listy wy�wietlania
	int ReadMap(char filename[128]);
};

// k�t pomi�dzy pojazdami na podstawie kwaternion�w orientacji   
float AngleBetweenQuats(quaternion q1, quaternion q2);

// odleg�o�� pomi�dzy punktami w �wiecie toroidalnym (wymaga uwzgl�dnienia przeskok�w pomi�dzy ko�cem i pocz�tkiem)
float DistanceBetweenPointsOnTetraMap(Vector3 p1, Vector3 p2);

// realizacja kroku scenariusza dla podanego obiektu, scenariusza i czasu od pocz�tku
// zwraca informacj� czy scenariusz dobieg� ko�ca, umieszcza w obj parametry sterowania (si�a, predko�� skr�tu k�, stopie� ham.)
bool test_scenario_step(MovableObject *obj, float test_scenario[][4], int number_of_actions, float __time);