#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <random>
#include <chrono>
#include <iostream>
#include <cfloat>
#include "Game.h"
#include <thread>

Memory k_memory;
Memory client_mem;

bool firing_range = false;
bool active = true;
uintptr_t aimentity = 0;
uintptr_t tmp_aimentity = 0;
uintptr_t lastaimentity = 0;
float max = 999.0f;
float max_dist = 200.0f*40.0f;
int team_player = 0;
float max_fov = 15;
const int toRead = 150;
int aim = false;
bool esp = false;
bool item_glow = false;
bool player_glow = false;
bool aim_no_recoil = false;
bool aiming = false;
float smooth = 0.f;
int bone = 0;
bool thirdperson = false;
bool chargerifle = false;
bool shooting = false;

bool actions_t = false;
bool esp_t = false;
bool aim_t = false;
bool vars_t = false;
bool item_t = false;
uint64_t g_Base;
uint64_t c_Base;
bool next = false;
bool valid = false;
bool lock = false;

typedef struct player
{
	float dist = 0;
	int entity_team = 0;
	float boxMiddle = 0;
	float h_y = 0;
	float width = 0;
	float height = 0;
	float b_x = 0;
	float b_y = 0;
	bool knocked = false;
	bool visible = false;
	int health = 0;
	int shield = 0;
	char name[36] = { 0 };
}player;

struct Matrix
{
	float matrix[16];
};

float lastvis_esp[toRead];
float lastvis_aim[toRead];

int tmp_spec = 0, spectators = 0;
int tmp_all_spec = 0, allied_spectators = 0;

//////////////////////////////////////////////////////////////////////////////////////////////////

// /////////////////////////////////////////////////////////////////////////////////////////////////////

player players[toRead];

void assign_name(nameentry_t entry, int idx)
{
	for (int i = 0; i < 36; i++)
	{
		players[idx].name[i] = entry.name[i];
	}
}

float units_to_m(float units) {
	return units * 0.0254;
}

static void EspLoop()
{
	esp_t = true;
	while(esp_t)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		if (esp)
		{
			valid = false;

			player_t local(GameFunctions::get_client_info() + (GameFunctions::local_index() * offsets::player::size));
			if (&local == 0)
			{
				next = true;
				while(next && g_Base!=0 && c_Base!=0 && esp)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
				continue;
			}

			Vector2D root_pos_out;
			Vector2D head_pos_out;
			
			memset(players,0,sizeof(players));
			for (int i = 0; i < 150; i++)
			{
				uint64_t c_base = GameFunctions::get_client_info();
				player_t entity(c_base + (i * offsets::player::size));
				if (!entity.is_valid() || entity.is_dead())
					continue;

				int local_team = local.team_id();
				if (entity.team_id() == local_team)
					continue;

				Vector pos = entity.get_pos();

				nameentry_t local_name_entry = GameFunctions::get_name_entry(i);

				Vector root_pos = pos;
				Vector head_pos = pos + Vector{ 0, 0, 58 };
				if (GameFunctions::world_to_screen(head_pos, &head_pos_out) && GameFunctions::world_to_screen(root_pos, &root_pos_out)) {
					Vector local_pos = local.get_pos();
					float dist = units_to_m(local_pos.DistTo(root_pos));

					Vector2D hs = head_pos_out;
					Vector2D HeadPosition = head_pos_out;
					float height = abs(abs(hs.y) - abs(root_pos_out.y));
					float width = height / 2.0f;
					float boxMiddle = root_pos_out.x - (width / 2.0f);

					players[i] = 
					{
						dist,
						entity.team_id(),
						boxMiddle,
						hs.y,
						width,
						height,
						root_pos_out.x,
						root_pos_out.y,
						0,
						true,
						local_name_entry.health,
						0,
					};
					assign_name(local_name_entry, i);
					valid = true;
				}

				GameFunctions::refresh_ref_def();
			}
			next = true;
			while(next && g_Base!=0 && c_Base!=0 && esp)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	}
	esp_t = false;
}

static void set_vars(uint64_t add_addr)
{
	printf("Reading client vars...\n");
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	//Get addresses of client vars
	uint64_t check_addr = 0;
	client_mem.Read<uint64_t>(add_addr, check_addr);
	uint64_t aim_addr = 0;
	client_mem.Read<uint64_t>(add_addr + sizeof(uint64_t), aim_addr);
	uint64_t esp_addr = 0;
	client_mem.Read<uint64_t>(add_addr + sizeof(uint64_t)*2, esp_addr);
	uint64_t aiming_addr = 0;
	client_mem.Read<uint64_t>(add_addr + sizeof(uint64_t)*3, aiming_addr);
	uint64_t g_Base_addr = 0;
	client_mem.Read<uint64_t>(add_addr + sizeof(uint64_t)*4, g_Base_addr);
	uint64_t next_addr = 0;
	client_mem.Read<uint64_t>(add_addr + sizeof(uint64_t)*5, next_addr);
	uint64_t player_addr = 0;
	client_mem.Read<uint64_t>(add_addr + sizeof(uint64_t)*6, player_addr);
	uint64_t valid_addr = 0;
	client_mem.Read<uint64_t>(add_addr + sizeof(uint64_t)*7, valid_addr);
	uint64_t max_dist_addr = 0;
	client_mem.Read<uint64_t>(add_addr + sizeof(uint64_t)*8, max_dist_addr);
	uint64_t item_glow_addr = 0;
	client_mem.Read<uint64_t>(add_addr + sizeof(uint64_t)*9, item_glow_addr);
	uint64_t player_glow_addr = 0;
	client_mem.Read<uint64_t>(add_addr + sizeof(uint64_t)*10, player_glow_addr);
	uint64_t aim_no_recoil_addr = 0;
	client_mem.Read<uint64_t>(add_addr + sizeof(uint64_t)*11, aim_no_recoil_addr);
	uint64_t smooth_addr = 0;
	client_mem.Read<uint64_t>(add_addr + sizeof(uint64_t)*12, smooth_addr);
	uint64_t max_fov_addr = 0;
	client_mem.Read<uint64_t>(add_addr + sizeof(uint64_t)*13, max_fov_addr);
	uint64_t bone_addr = 0;
	client_mem.Read<uint64_t>(add_addr + sizeof(uint64_t)*14, bone_addr);
	uint64_t thirdperson_addr = 0;
	client_mem.Read<uint64_t>(add_addr + sizeof(uint64_t)*15, thirdperson_addr);
	uint64_t spectators_addr = 0;
	client_mem.Read<uint64_t>(add_addr + sizeof(uint64_t)*16, spectators_addr);
	uint64_t allied_spectators_addr = 0;
	client_mem.Read<uint64_t>(add_addr + sizeof(uint64_t)*17, allied_spectators_addr);
	uint64_t chargerifle_addr = 0;
	client_mem.Read<uint64_t>(add_addr + sizeof(uint64_t)*18, chargerifle_addr);
	uint64_t shooting_addr = 0;
	client_mem.Read<uint64_t>(add_addr + sizeof(uint64_t)*19, shooting_addr);
	

	uint32_t check = 0;
	client_mem.Read<uint32_t>(check_addr, check);
	
	if(check != 0xABCD)
	{
		printf("Incorrect values read. Check if the add_off is correct. Quitting.\n");
		active = false;
		return;
	}
	vars_t = true;
	while(vars_t)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		if(c_Base!=0 && g_Base!=0)
		{
			client_mem.Write<uint32_t>(check_addr, 0);
			printf("\nReady\n");
		}

		while(c_Base!=0 && g_Base!=0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			client_mem.Write<uint64_t>(g_Base_addr, g_Base);
			client_mem.Write<int>(spectators_addr, spectators);
			client_mem.Write<int>(allied_spectators_addr, allied_spectators);

			client_mem.Read<int>(aim_addr, aim);
			client_mem.Read<bool>(esp_addr, esp);
			client_mem.Read<bool>(aiming_addr, aiming);
			client_mem.Read<float>(max_dist_addr, max_dist);
			client_mem.Read<bool>(item_glow_addr, item_glow);
			client_mem.Read<bool>(player_glow_addr, player_glow);
			client_mem.Read<bool>(aim_no_recoil_addr, aim_no_recoil);
			client_mem.Read<float>(smooth_addr, smooth);
			client_mem.Read<float>(max_fov_addr, max_fov);
			client_mem.Read<int>(bone_addr, bone);
			client_mem.Read<bool>(thirdperson_addr, thirdperson);
			client_mem.Read<bool>(shooting_addr, shooting);
			client_mem.Read<bool>(chargerifle_addr, chargerifle);

			if(esp && next)
			{
				if(valid)
					client_mem.WriteArray<player>(player_addr, players, toRead);
				client_mem.Write<bool>(valid_addr, valid);
				client_mem.Write<bool>(next_addr, true); //next

				bool next_val = false;
				do
				{
					client_mem.Read<bool>(next_addr, next_val);
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				} while (next_val && g_Base!=0 && c_Base!=0);
				
				next = false;
			}
		}
	}
	vars_t = false;
}

int main(int argc, char *argv[])
{
	if(geteuid() != 0)
	{
		printf("Error: %s is not running as root\n", argv[0]);
		return 0;
	}

	const char* cl_proc = "client_ap.exe";
	const char* ap_proc = "cod.exe";

	//Client "add" offset
	uint64_t add_off = 0x3d810;

	std::thread aimbot_thr;
	std::thread esp_thr;
	std::thread actions_thr;
	std::thread itemglow_thr;
	std::thread vars_thr;
	std::thread cache_thr;

	while(active)
	{
		if(k_memory.get_proc_status() != process_status::FOUND_READY)
		{
			if(aim_t)
			{
				aim_t = false;
				esp_t = false;
				actions_t = false;
				item_t = false;
				g_Base = 0;

				esp_thr.~thread();
				cache_thr.~thread();
			}

			std::this_thread::sleep_for(std::chrono::seconds(1));
			printf("Searching for COD process...\n");

			k_memory.open_proc(ap_proc);
			k_memory.load_proc_info(ap_proc);

			if(k_memory.get_proc_status() == process_status::FOUND_READY)
			{
				g_Base = k_memory.get_proc_baseaddr();
				printf("\nCOD process found\n");
				printf("Base: %lx\n", g_Base);

				esp_thr = std::thread(EspLoop);
				esp_thr.detach();

				cache_thr = std::thread(set_client_vars, g_Base);
				cache_thr.detach();
			}
		}
		else
		{
			k_memory.check_proc();
		}

		if(client_mem.get_proc_status() != process_status::FOUND_READY)
		{
			if(vars_t)
			{
				vars_t = false;
				c_Base = 0;

				vars_thr.~thread();
			}
			
			std::this_thread::sleep_for(std::chrono::seconds(1));
			printf("Searching for client process...\n");

			client_mem.open_proc(cl_proc);

			if(client_mem.get_proc_status() == process_status::FOUND_READY)
			{
				c_Base = client_mem.get_proc_baseaddr();
				printf("\nClient process found\n");
				printf("Base: %lx\n", c_Base);

				vars_thr = std::thread(set_vars, c_Base + add_off);
				vars_thr.detach();
			}
		}
		else
		{
			client_mem.check_proc();
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	return 0;
}
