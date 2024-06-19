#include <stdio.h>
#include <stdlib.h>
#include "SDL.h"
#include "SDL_net.h"
#include "ev3.h"
#include "ev3_tacho.h"

#define PI 3.14

int main(int arvc, char* argv[]) {
	SDL_Init(SDL_INIT_VIDEO);
	SDLNet_Init();
	
	const float baseDiameter = 11; // cm
	const float wheelDiameter = 5; // cm
	float wheelCircumference = wheelDiameter * PI;
	float baseCircumference = baseDiameter * PI;
	float travelDistance;
	float mazeLenght = 0;
	int mazeWidth = 0;
	FLAGS_T state;
	
	int recv_data;
	TCPsocket server, client;
	IPaddress serverIp;
	
	// initialize 
	mazeWidth = atoi(argv[1]);
	mazeLenght = atoi(argv[2]);
	
	if (ev3_init() == -1) return -1;
	if (ev3_tacho_init() == -1) return -1;
	uint8_t sn1, sn2, sn_port; 
	int max_speed, tacho_counts;
	ev3_search_tacho(LEGO_EV3_L_MOTOR, &sn1, 0);
	ev3_search_tacho(LEGO_EV3_L_MOTOR, &sn2, sn1+1);
	get_tacho_max_speed(sn1, &max_speed);
	get_tacho_count_per_rot(sn1, &tacho_counts);
	
	SDLNet_ResolveHost(&serverIp, NULL, 10504);
	server = SDLNet_TCP_Open(&serverIp);
	if (server) { printf("opened server socket \n"); }
	printf("listening for connections\n");
	
	while (!client) {
		client = SDLNet_TCP_Accept(server);
	}
	printf("received a connection\n");
	// 1 - forward
	// 2 - turn
	// 3 - end
	while (1) {
		SDLNet_TCP_Recv(client, &recv_data, sizeof(int)); // получение через протокол TCP, используется библиотека SDL
		printf("received code = %d\n", recv_data);
		switch (recv_data) {
			case 1: { // отработка комманды типа "ехать вперёд"
				SDLNet_TCP_Recv(client, &recv_data, sizeof(int));
				printf("run forward for = %f\n", travelDistance = recv_data * (mazeLenght/128));
				travelDistance = recv_data * (mazeLenght/128); // 0,1640625
				
				set_tacho_stop_action_inx(sn1, TACHO_COAST); // настройка двигателя
				set_tacho_polarity_inx(sn1, TACHO_NORMAL);
				set_tacho_speed_sp(sn1, max_speed/8);
				set_tacho_position_sp(sn1, (tacho_counts / 15.5) * travelDistance);
				set_tacho_ramp_up_sp(sn1, 200);
				set_tacho_ramp_down_sp(sn1, 200);
				set_tacho_command_inx(sn1, TACHO_RUN_TO_REL_POS); // запуск команды
				
				set_tacho_polarity_inx(sn2, TACHO_NORMAL); 
				set_tacho_stop_action_inx(sn2, TACHO_COAST);
				set_tacho_speed_sp(sn2, max_speed/8);
				set_tacho_position_sp(sn2, (tacho_counts / 15.5) * travelDistance);
				set_tacho_ramp_up_sp(sn2, 200);
				set_tacho_ramp_down_sp(sn2, 200);
				set_tacho_command_inx(sn2, TACHO_RUN_TO_REL_POS);
				do {
					get_tacho_state_flags(sn1, &state);
				} while(state); // проверка состояния двигателя
				break;
			}
			case 2: {
				SDLNet_TCP_Recv(client, &recv_data, sizeof(int));
				printf("turn for = %d degrees\n", recv_data);
				if (recv_data < 0) {
					recv_data = recv_data * -1;
					set_tacho_polarity_inx(sn1, TACHO_INVERSED);
					set_tacho_polarity_inx(sn2, TACHO_NORMAL);
				} else {
					set_tacho_polarity_inx(sn1, TACHO_NORMAL);
					set_tacho_polarity_inx(sn2, TACHO_INVERSED);
				}
				travelDistance = (baseCircumference / 360) * recv_data;
				
				set_tacho_stop_action_inx(sn1, TACHO_BRAKE);
				set_tacho_speed_sp(sn1, max_speed/8);
				set_tacho_position_sp(sn1, (tacho_counts / wheelCircumference) * travelDistance);
				set_tacho_ramp_up_sp(sn1, 200);
				set_tacho_ramp_down_sp(sn1, 200);
				set_tacho_command_inx(sn1, TACHO_RUN_TO_REL_POS);
				
				set_tacho_stop_action_inx(sn2, TACHO_BRAKE);
				set_tacho_speed_sp(sn2, max_speed/8);
				set_tacho_position_sp(sn2, (tacho_counts / wheelCircumference) * travelDistance);
				set_tacho_ramp_up_sp(sn2, 200);
				set_tacho_ramp_down_sp(sn2, 200);
				set_tacho_command_inx(sn2, TACHO_RUN_TO_REL_POS);
				do {
					get_tacho_state_flags(sn1, &state);
				} while(state);
				break;
			}
			case 3: {
				SDLNet_Quit();
				return 0;
				break;
			}
		}
	}
}