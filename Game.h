#include "vector.h"
#include "offsets.h"
#include "memory.h"

inline uint64_t rotl64 ( uint64_t x, int8_t r )
{
  return (x << r) | (x >> (64 - r));
}

void set_client_vars(uint64_t base);

enum CharacterStance : uint32_t 
{
	Standing = 0,
	Crouching = 4,
	Prone = 8,
	Downed = 16
};

struct nameentry_t
{
	uint32_t index;
	char name[36];
	uint8_t pad[92];
	int32_t health;
	uint8_t pad2[70];
};

class player_t
{
private:
	uint64_t address = 0;
public:

	player_t(uint64_t address)
	{
		this->address = address;
	}

	bool is_valid();
	bool is_dead();
	uint16_t team_id();
	Vector get_pos();
};

namespace GameFunctions
{
	int local_index();
	Vector camera_position();
	bool world_to_screen(const Vector& world_pos, Vector2D* screen_pos);
	Vector get_bone_base_pos();
	Vector get_bone_position(const Vector& base_pos, const int bone);
	nameentry_t get_name_entry(uint32_t index);

	uint64_t get_client_info();
	uint64_t get_ref_def();
	void refresh_ref_def();
}

namespace GameDecrypts 
{
    uint64_t decrypt_client_info();
    uint64_t decrypt_client_base();
    uint64_t decrypt_bone_base();
    uint64_t get_bone_index(uint64_t bone_index);
}
