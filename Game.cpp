#include "Game.h"
#include <thread>

extern Memory k_memory;

struct ref_def_key
{
	int ref0;
	int ref1;
	int ref2;
};

struct ref_def_view {
	Vector2D tan_half_fov;
	char pad[0xC];
	Vector axis[3];
};

struct ref_def_t {
	int x;
	int y;
	int width;
	int height;
	ref_def_view view;
};

ref_def_t ref_def;

namespace GameGlobals
{
	uint64_t process_peb  = 0;
	uint64_t module_base  = 0;
	uintptr_t module_size = 0;

	uint64_t client       = 0;
	uint64_t c_bone	  	  = 0;
	uint64_t client_base  = 0;

	namespace Cache
	{
		uint64_t name_entry_address;
	}
}

void set_client_vars(uint64_t base)
{
	GameGlobals::module_base = base;
	GameGlobals::module_size = k_memory.get_module_size();
	printf("Module size: %p\n", GameGlobals::module_size);

	GameGlobals::process_peb = k_memory.get_peb_addr();
	printf("PEB: %lx\n", GameGlobals::process_peb);

	auto ref_def_ptr = GameFunctions::get_ref_def();
	printf("Ref def ptr: %p\n", ref_def_ptr);

	k_memory.Read<ref_def_t>(ref_def_ptr, ref_def);

	while(true) {
		GameGlobals::client = GameDecrypts::decrypt_client_info();

		GameGlobals::client_base = GameDecrypts::decrypt_client_base();

		GameGlobals::c_bone = GameDecrypts::decrypt_bone_base();
	
		uintptr_t ptr;
		k_memory.Read<uintptr_t>(GameGlobals::module_base + offsets::name_array, ptr);
		GameGlobals::Cache::name_entry_address = ptr + offsets::name_array_pos;

		std::this_thread::sleep_for(std::chrono::milliseconds(5000));
	}
}

int GameFunctions::local_index()
{
	uint64_t local_index;
	int ret;

	k_memory.Read<uint64_t>(GameGlobals::client + offsets::local_index, local_index);
	k_memory.Read<int>(local_index + offsets::local_index_pos, ret);
	return ret;
}

Vector GameFunctions::camera_position()
{
	uint64_t camera;
	Vector ret = {};

	k_memory.Read<uint64_t>(GameGlobals::module_base + offsets::camera_base, camera);
	if (camera)
		k_memory.Read<Vector>(camera + offsets::camera_pos, ret);
	return ret;
}

bool GameFunctions::world_to_screen(const Vector& world_pos, Vector2D* screen_pos)
{
	Vector view_origin = camera_position();
	auto refdef = ref_def;

	Vector v_local, v_transform;
	v_local = world_pos - view_origin;
	v_transform.x = v_local.Dot(refdef.view.axis[1]);
	v_transform.y = v_local.Dot(refdef.view.axis[2]);
	v_transform.z = v_local.Dot(refdef.view.axis[0]);

	// printf(" Transform.z: %f\n", v_transform.z);

	// make sure it is in front of us
	if (v_transform.z < 0.01f)
			return false;
	screen_pos->x = ((refdef.width / 2) * (1 - (v_transform.x / refdef.view.tan_half_fov.x / v_transform.z)));
	screen_pos->y = ((refdef.height / 2) * (1 - (v_transform.y / refdef.view.tan_half_fov.y / v_transform.z)));

	if (screen_pos->x < 1 || screen_pos->y < 1 || (screen_pos->x > refdef.width) || (screen_pos->y > refdef.height)) {
			return false;
	}

	return true;
}

nameentry_t GameFunctions::get_name_entry(uint32_t index)
{
	nameentry_t ret;
	k_memory.Read<nameentry_t>(GameGlobals::Cache::name_entry_address + (index * 0xD0), ret);
	return ret;
}

uint64_t GameFunctions::get_client_info()
{
	return GameGlobals::client_base;
}

typedef unsigned long long eint64_t;
auto xor_bit(eint64_t v1, eint64_t v2)
{
	return v1 ^ v2;
}

uint64_t GameFunctions::get_ref_def()
{
	ref_def_key crypt;
	// k_memory.Read<ref_def_key>(GameGlobals::module_base + offsets::ref_def_ptr, crypt);
	uint64_t baseAddr = GameGlobals::module_base;
	// printf(" Base address: %p\n", GameGlobals::module_base);
	// printf(" Virtual read crypt: %p\n", crypt);

	k_memory.Read<int>(GameGlobals::module_base + (offsets::ref_def_ptr + 0x00), crypt.ref0);
	k_memory.Read<int>(GameGlobals::module_base + (offsets::ref_def_ptr + 0x04), crypt.ref1);
	k_memory.Read<int>(GameGlobals::module_base + (offsets::ref_def_ptr + 0x08), crypt.ref2);

		// REF0: 2052724712
		// REF1: -683526162
		// REF2: 1849860978

	eint64_t offset1 = baseAddr + offsets::ref_def_ptr;
	eint64_t offset2 = baseAddr + offsets::ref_def_ptr + 0x4;

	eint64_t bit1 = xor_bit((eint64_t)(crypt.ref2), offset1); // (crypt.ref2 ^ (uint64_t)(baseAddr + offsets::ref_def_ptr))
	eint64_t bit2 = xor_bit((eint64_t)(crypt.ref2), offset2); // (crypt.ref2 ^ (uint64_t)(baseAddr + offsets::ref_def_ptr + 0x4))

	eint64_t add_l = bit1 + 2;
	eint64_t add_u = bit2 + 2;

	eint64_t final_l = bit1 * add_l;
	eint64_t final_u = bit2 * add_u;

	DWORD l_bit = (uint32_t)(crypt.ref0) ^ (uint32_t)(final_l); //  
	eint64_t u_bit = (uint32_t)(crypt.ref1) ^ (uint32_t)(final_u); // 

	return (eint64_t)u_bit << 32 | l_bit;
}

void GameFunctions::refresh_ref_def()
{
	k_memory.Read<ref_def_t>(GameFunctions::get_ref_def(), ref_def);
}

bool player_t::is_valid()
{
	bool ret;
	k_memory.Read<bool>(address + offsets::player::valid, ret);
	return ret;
}

bool player_t::is_dead()
{
	bool dead1;
	bool dead2;

	k_memory.Read<bool>(address + offsets::player::dead_1, dead1);
	k_memory.Read<bool>(address + offsets::player::dead_2, dead2);
	return dead1 || dead2;
}

int player_t::team_id()
{
	int ret;
	k_memory.Read<int>(address + offsets::player::team, ret);
	return ret;
}

Vector player_t::get_pos()
{
	uint64_t local_pos;
	Vector ret = {};

	k_memory.Read<uint64_t>(address + offsets::player::pos, local_pos);
	k_memory.Read<Vector>(local_pos + 0x48, ret);
	return ret;
}

int player_t::get_stance()
{
	int ret;
	k_memory.Read<int>(address + offsets::player::stance, ret);
	return ret;
}

uintptr_t GameDecrypts::decrypt_client_info()
{
        const uint64_t mb = GameGlobals::module_base;
        uint64_t rax = mb, rbx = mb, rcx = mb, rdx = mb, rdi = mb, rsi = mb, r8 = mb, r9 = mb, r10 = mb, r11 = mb, r12 = mb, r13 = mb, r14 = mb, r15 = mb;
        rbx = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x130AD1E8);
        if(!rbx)
                return rbx;
        rdx= ~GameGlobals::process_peb;              //mov rdx, gs:[rax]
        rax = rbx;              //mov rax, rbx
        rax >>= 0x22;           //shr rax, 0x22
        rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
        rbx ^= rax;             //xor rbx, rax
        rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
        rcx ^= k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA1840E3);             //xor rcx, [0x00000000081983B0]
        rax = GameGlobals::module_base + 0x1343C359;              //lea rax, [0x000000001145061F]
        rbx += rdx;             //add rbx, rdx
        rcx = ~rcx;             //not rcx
        rbx += rax;             //add rbx, rax
        rax = 0xD63E4A83CB9A620B;               //mov rax, 0xD63E4A83CB9A620B
        rbx *= k_memory.ReadNb<uintptr_t>(rcx + 0x11);             //imul rbx, [rcx+0x11]
        rbx -= rdx;             //sub rbx, rdx
        rbx *= rax;             //imul rbx, rax
        rax = 0x57242547CAD98C71;               //mov rax, 0x57242547CAD98C71
        rbx -= rax;             //sub rbx, rax
        return rbx;
}
uintptr_t GameDecrypts::decrypt_client_base()
{
        const uint64_t mb = GameGlobals::module_base;
        uint64_t rax = mb, rbx = mb, rcx = mb, rdx = mb, rdi = mb, rsi = mb, r8 = mb, r9 = mb, r10 = mb, r11 = mb, r12 = mb, r13 = mb, r14 = mb, r15 = mb;
        rdx = k_memory.ReadNb<uintptr_t>(GameGlobals::client + 0x10e5a0);
        if(!rdx)
                return rdx;
        r11= ~GameGlobals::process_peb;              //mov r11, gs:[rax]
        rax = r11;              //mov rax, r11
        rax = rotl64(rax, 0x23);               //rol rax, 0x23
        rax &= 0xF;
        switch(rax) {
        case 0:
        {
                rbx = GameGlobals::module_base;           //lea rbx, [0xFFFFFFFFFE036AED]
                r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184129);              //mov r10, [0x00000000081BABAC]
                rax = rdx;              //mov rax, rdx
                rax >>= 0x7;            //shr rax, 0x07
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0xE;            //shr rax, 0x0E
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x1C;           //shr rax, 0x1C
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x38;           //shr rax, 0x38
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0xA;            //shr rax, 0x0A
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x14;           //shr rax, 0x14
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x28;           //shr rax, 0x28
                rdx ^= rax;             //xor rdx, rax
                rax = 0;                //and rax, 0xFFFFFFFFC0000000
                rax = rotl64(rax, 0x10);               //rol rax, 0x10
                rax ^= r10;             //xor rax, r10
                rax = __bswap_64(rax);            //bswap rax
                rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x15);             //imul rdx, [rax+0x15]
                rdx += rbx;             //add rdx, rbx
                rax = 0x6A51BC9BC4AA6767;               //mov rax, 0x6A51BC9BC4AA6767
                rdx *= rax;             //imul rdx, rax
                rax = 0x5447EBF1221B83E6;               //mov rax, 0x5447EBF1221B83E6
                rdx -= rax;             //sub rdx, rax
                rdx ^= r11;             //xor rdx, r11
                rdx ^= rbx;             //xor rdx, rbx
                return rdx;
        }
        case 1:
        {
                r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184129);              //mov r10, [0x00000000081BA775]
                rbx = GameGlobals::module_base;           //lea rbx, [0xFFFFFFFFFE036645]
                rdx -= r11;             //sub rdx, r11
                rax = GameGlobals::module_base + 0x6B3C0100;              //lea rax, [0x00000000693F6656]
                rax = ~rax;             //not rax
                rdx ^= rax;             //xor rdx, rax
                rdx ^= r11;             //xor rdx, r11
                rax = 0;                //and rax, 0xFFFFFFFFC0000000
                rax = rotl64(rax, 0x10);               //rol rax, 0x10
                rax ^= r10;             //xor rax, r10
                rax = __bswap_64(rax);            //bswap rax
                rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x15);             //imul rdx, [rax+0x15]
                rdx ^= rbx;             //xor rdx, rbx
                rax = rdx;              //mov rax, rdx
                rax >>= 0x24;           //shr rax, 0x24
                rdx ^= rax;             //xor rdx, rax
                rax = 0x4A2A83616AD92661;               //mov rax, 0x4A2A83616AD92661
                rdx *= rax;             //imul rdx, rax
                rax = 0xECFC5B4C57C54F28;               //mov rax, 0xECFC5B4C57C54F28
                rdx += rax;             //add rdx, rax
                rax = 0xF0FDCE631F7BA29F;               //mov rax, 0xF0FDCE631F7BA29F
                rdx ^= rax;             //xor rdx, rax
                return rdx;
        }
        case 2:
        {
                rbx = GameGlobals::module_base;           //lea rbx, [0xFFFFFFFFFE0361CB]
                rcx = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184129);              //mov rcx, [0x00000000081BA275]
                rax = 0xD511FD9CF85D2C07;               //mov rax, 0xD511FD9CF85D2C07
                rdx *= rax;             //imul rdx, rax
                rdx ^= r11;             //xor rdx, r11
                rax = r11;              //mov rax, r11
                uintptr_t RSP_0xFFFFFFFFFFFFFFCF;
                RSP_0xFFFFFFFFFFFFFFCF = GameGlobals::module_base + 0x2E433015;           //lea rax, [0x000000002C469199] : RBP+0xFFFFFFFFFFFFFFCF
                rax *= RSP_0xFFFFFFFFFFFFFFCF;          //imul rax, [rbp-0x31]
                rdx += rax;             //add rdx, rax
                rdx -= rbx;             //sub rdx, rbx
                rax = rdx;              //mov rax, rdx
                rax >>= 0x12;           //shr rax, 0x12
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x24;           //shr rax, 0x24
                rdx ^= rax;             //xor rdx, rax
                rax = 0x3EDAD65FDC1034FF;               //mov rax, 0x3EDAD65FDC1034FF
                rdx *= rax;             //imul rdx, rax
                rax = 0x2AE3002A8E8BF08B;               //mov rax, 0x2AE3002A8E8BF08B
                rdx -= rax;             //sub rdx, rax
                rax = 0;                //and rax, 0xFFFFFFFFC0000000
                rax = rotl64(rax, 0x10);               //rol rax, 0x10
                rax ^= rcx;             //xor rax, rcx
                rax = __bswap_64(rax);            //bswap rax
                rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x15);             //imul rdx, [rax+0x15]
                return rdx;
        }
        case 3:
        {
                r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184129);              //mov r10, [0x00000000081B9ED9]
                rbx = GameGlobals::module_base;           //lea rbx, [0xFFFFFFFFFE035DA9]
                rax = rbx + 0x1771cb1b;                 //lea rax, [rbx+0x1771CB1B]
                rax += r11;             //add rax, r11
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0xB;            //shr rax, 0x0B
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x16;           //shr rax, 0x16
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x2C;           //shr rax, 0x2C
                rdx ^= rax;             //xor rdx, rax
                rdx -= r11;             //sub rdx, r11
                rax = 0xFD500870540625B;                //mov rax, 0xFD500870540625B
                rdx *= rax;             //imul rdx, rax
                rax = 0x1BC06434489E44B5;               //mov rax, 0x1BC06434489E44B5
                rdx += rax;             //add rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x1F;           //shr rax, 0x1F
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x3E;           //shr rax, 0x3E
                rdx ^= rax;             //xor rdx, rax
                rax = 0;                //and rax, 0xFFFFFFFFC0000000
                rax = rotl64(rax, 0x10);               //rol rax, 0x10
                rax ^= r10;             //xor rax, r10
                rax = __bswap_64(rax);            //bswap rax
                rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x15);             //imul rdx, [rax+0x15]
                rax = 0x600D6B3C699E6524;               //mov rax, 0x600D6B3C699E6524
                rdx += rax;             //add rdx, rax
                return rdx;
        }
        case 4:
        {
                rbx = GameGlobals::module_base;           //lea rbx, [0xFFFFFFFFFE0357D8]
                r9 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184129);               //mov r9, [0x00000000081B9890]
                rax = 0;                //and rax, 0xFFFFFFFFC0000000
                rax = rotl64(rax, 0x10);               //rol rax, 0x10
                rax ^= r9;              //xor rax, r9
                rax = __bswap_64(rax);            //bswap rax
                rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x15);             //imul rdx, [rax+0x15]
                rax = 0x44DF33AE79D34CE7;               //mov rax, 0x44DF33AE79D34CE7
                rdx *= rax;             //imul rdx, rax
                rdx -= r11;             //sub rdx, r11
                rax = r11;              //mov rax, r11
                rax = ~rax;             //not rax
                rdx ^= rax;             //xor rdx, rax
                rax = GameGlobals::module_base + 0x3B36;          //lea rax, [0xFFFFFFFFFE038E0F]
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x27;           //shr rax, 0x27
                rdx ^= rax;             //xor rdx, rax
                rdx -= r11;             //sub rdx, r11
                rax = r11;              //mov rax, r11
                uintptr_t RSP_0xFFFFFFFFFFFFFFA7;
                RSP_0xFFFFFFFFFFFFFFA7 = GameGlobals::module_base + 0x27B9;               //lea rax, [0xFFFFFFFFFE037F4F] : RBP+0xFFFFFFFFFFFFFFA7
                rax *= RSP_0xFFFFFFFFFFFFFFA7;          //imul rax, [rbp-0x59]
                rdx += rax;             //add rdx, rax
                rax = r11;              //mov rax, r11
                rax -= rbx;             //sub rax, rbx
                rax += 0xFFFFFFFFD6AD7A46;              //add rax, 0xFFFFFFFFD6AD7A46
                rdx += rax;             //add rdx, rax
                return rdx;
        }
        case 5:
        {
                r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184129);              //mov r10, [0x00000000081B93D9]
                rbx = GameGlobals::module_base;           //lea rbx, [0xFFFFFFFFFE0352A9]
                rax = rdx;              //mov rax, rdx
                rax >>= 0x17;           //shr rax, 0x17
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x2E;           //shr rax, 0x2E
                rdx ^= rax;             //xor rdx, rax
                rax = rbx + 0xb7ef;             //lea rax, [rbx+0xB7EF]
                rax += r11;             //add rax, r11
                rdx += rax;             //add rdx, rax
                rdx -= r11;             //sub rdx, r11
                uintptr_t RSP_0xFFFFFFFFFFFFFFBF;
                RSP_0xFFFFFFFFFFFFFFBF = 0x20E3F69C982B8265;            //mov rax, 0x20E3F69C982B8265 : RBP+0xFFFFFFFFFFFFFFBF
                rdx ^= RSP_0xFFFFFFFFFFFFFFBF;          //xor rdx, [rbp-0x41]
                rax = 0;                //and rax, 0xFFFFFFFFC0000000
                rax = rotl64(rax, 0x10);               //rol rax, 0x10
                rax ^= r10;             //xor rax, r10
                rax = __bswap_64(rax);            //bswap rax
                rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x15);             //imul rdx, [rax+0x15]
                rax = 0xAEE1A029315E3D4F;               //mov rax, 0xAEE1A029315E3D4F
                rdx *= rax;             //imul rdx, rax
                rax = rbx + 0x618b;             //lea rax, [rbx+0x618B]
                rax += r11;             //add rax, r11
                rdx += rax;             //add rdx, rax
                rax = 0x4F576A9DC4CD39EE;               //mov rax, 0x4F576A9DC4CD39EE
                rdx += rax;             //add rdx, rax
                return rdx;
        }
        case 6:
        {
                r9 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184129);               //mov r9, [0x00000000081B8F58]
                rdx -= r11;             //sub rdx, r11
                rax = rdx;              //mov rax, rdx
                rax >>= 0x12;           //shr rax, 0x12
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x24;           //shr rax, 0x24
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x21;           //shr rax, 0x21
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x1A;           //shr rax, 0x1A
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x34;           //shr rax, 0x34
                rax ^= rdx;             //xor rax, rdx
                rdx = 0x17CF0497F2D22203;               //mov rdx, 0x17CF0497F2D22203
                rax *= rdx;             //imul rax, rdx
                rdx = rax;              //mov rdx, rax
                rdx >>= 0x25;           //shr rdx, 0x25
                rdx ^= rax;             //xor rdx, rax
                rax = 0xBE5CC72B0AEE64FD;               //mov rax, 0xBE5CC72B0AEE64FD
                rdx ^= rax;             //xor rdx, rax
                rax = 0;                //and rax, 0xFFFFFFFFC0000000
                rax = rotl64(rax, 0x10);               //rol rax, 0x10
                rax ^= r9;              //xor rax, r9
                rax = __bswap_64(rax);            //bswap rax
                rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x15);             //imul rdx, [rax+0x15]
                return rdx;
        }
        case 7:
        {
                rbx = GameGlobals::module_base;           //lea rbx, [0xFFFFFFFFFE034952]
                r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184129);              //mov r10, [0x00000000081B89F4]
                rcx = r11;              //mov rcx, r11
                rax = GameGlobals::module_base + 0xDEF0;          //lea rax, [0xFFFFFFFFFE042553]
                rcx ^= rax;             //xor rcx, rax
                rax = 0xF875422C3B24C08F;               //mov rax, 0xF875422C3B24C08F
                rax -= rcx;             //sub rax, rcx
                rdx += rax;             //add rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x16;           //shr rax, 0x16
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x2C;           //shr rax, 0x2C
                rdx ^= rax;             //xor rdx, rax
                rdx ^= rbx;             //xor rdx, rbx
                rax = 0x65FDE940447DEE2B;               //mov rax, 0x65FDE940447DEE2B
                rdx *= rax;             //imul rdx, rax
                rax = 0x39A26EAD2B76265B;               //mov rax, 0x39A26EAD2B76265B
                rdx += rax;             //add rdx, rax
                rax = 0;                //and rax, 0xFFFFFFFFC0000000
                rax = rotl64(rax, 0x10);               //rol rax, 0x10
                rax ^= r10;             //xor rax, r10
                rax = __bswap_64(rax);            //bswap rax
                rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x15);             //imul rdx, [rax+0x15]
                rax = rdx;              //mov rax, rdx
                rax >>= 0x1B;           //shr rax, 0x1B
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x36;           //shr rax, 0x36
                rdx ^= rax;             //xor rdx, rax
                return rdx;
        }
        case 8:
        {
                r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184129);              //mov r10, [0x00000000081B8551]
                rbx = GameGlobals::module_base;           //lea rbx, [0xFFFFFFFFFE034421]
                rax = 0xFFFFFFFFFFFF597D;               //mov rax, 0xFFFFFFFFFFFF597D
                rax -= r11;             //sub rax, r11
                rax -= rbx;             //sub rax, rbx
                rdx += rax;             //add rdx, rax
                rdx ^= r11;             //xor rdx, r11
                rax = 0xD0F09E7A8C7613B3;               //mov rax, 0xD0F09E7A8C7613B3
                rdx *= rax;             //imul rdx, rax
                rax = 0;                //and rax, 0xFFFFFFFFC0000000
                rax = rotl64(rax, 0x10);               //rol rax, 0x10
                rax ^= r10;             //xor rax, r10
                rax = __bswap_64(rax);            //bswap rax
                rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x15);             //imul rdx, [rax+0x15]
                rax = rdx;              //mov rax, rdx
                rax >>= 0x1F;           //shr rax, 0x1F
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x3E;           //shr rax, 0x3E
                rdx ^= rax;             //xor rdx, rax
                rax = 0x256B3436B62B89E5;               //mov rax, 0x256B3436B62B89E5
                rdx -= rax;             //sub rdx, rax
                rdx += rbx;             //add rdx, rbx
                rcx = r11;              //mov rcx, r11
                rax = GameGlobals::module_base + 0x7B64B958;              //lea rax, [0x000000007967FAA4]
                rax = ~rax;             //not rax
                rcx = ~rcx;             //not rcx
                rcx += rax;             //add rcx, rax
                rdx ^= rcx;             //xor rdx, rcx
                return rdx;
        }
        case 9:
        {
                rbx = GameGlobals::module_base;           //lea rbx, [0xFFFFFFFFFE033FC5]
                r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184129);              //mov r10, [0x00000000081B8072]
                rax = rdx;              //mov rax, rdx
                rax >>= 0x4;            //shr rax, 0x04
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x8;            //shr rax, 0x08
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x10;           //shr rax, 0x10
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x20;           //shr rax, 0x20
                rdx ^= rax;             //xor rdx, rax
                rax = 0x54E648D07B6D0B80;               //mov rax, 0x54E648D07B6D0B80
                rdx += rax;             //add rdx, rax
                rax = 0;                //and rax, 0xFFFFFFFFC0000000
                rax = rotl64(rax, 0x10);               //rol rax, 0x10
                rax ^= r10;             //xor rax, r10
                rax = __bswap_64(rax);            //bswap rax
                rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x15);             //imul rdx, [rax+0x15]
                rdx ^= rbx;             //xor rdx, rbx
                rax = rdx;              //mov rax, rdx
                rax >>= 0x16;           //shr rax, 0x16
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x2C;           //shr rax, 0x2C
                rax ^= r11;             //xor rax, r11
                rdx ^= rax;             //xor rdx, rax
                rdx -= r11;             //sub rdx, r11
                rax = 0xB0386AF6C89E01ED;               //mov rax, 0xB0386AF6C89E01ED
                rdx *= rax;             //imul rdx, rax
                return rdx;
        }
        case 10:
        {
                r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184129);              //mov r10, [0x00000000081B7B9F]
                rbx = GameGlobals::module_base;           //lea rbx, [0xFFFFFFFFFE033A6F]
                rax = 0x9332D19135BB918F;               //mov rax, 0x9332D19135BB918F
                rdx *= rax;             //imul rdx, rax
                rax = 0xFFFFFFFF8DA4B362;               //mov rax, 0xFFFFFFFF8DA4B362
                rax -= r11;             //sub rax, r11
                rax -= rbx;             //sub rax, rbx
                rdx += rax;             //add rdx, rax
                rax = 0;                //and rax, 0xFFFFFFFFC0000000
                rax = rotl64(rax, 0x10);               //rol rax, 0x10
                rax ^= r10;             //xor rax, r10
                rax = __bswap_64(rax);            //bswap rax
                rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x15);             //imul rdx, [rax+0x15]
                rdx ^= r11;             //xor rdx, r11
                rax = rdx;              //mov rax, rdx
                rax >>= 0x8;            //shr rax, 0x08
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x10;           //shr rax, 0x10
                rdx ^= rax;             //xor rdx, rax
                rcx = r11;              //mov rcx, r11
                rcx = ~rcx;             //not rcx
                rax = GameGlobals::module_base + 0xD1ED;          //lea rax, [0xFFFFFFFFFE040AB6]
                rcx *= rax;             //imul rcx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x20;           //shr rax, 0x20
                rcx ^= rax;             //xor rcx, rax
                rdx ^= rcx;             //xor rdx, rcx
                rcx = r11;              //mov rcx, r11
                rcx = ~rcx;             //not rcx
                rax = GameGlobals::module_base + 0x868;           //lea rax, [0xFFFFFFFFFE0341A1]
                rdx += rax;             //add rdx, rax
                rdx += rcx;             //add rdx, rcx
                rax = r11;              //mov rax, r11
                rax = ~rax;             //not rax
                rax -= rbx;             //sub rax, rbx
                rax -= 0x7CCC6306;              //sub rax, 0x7CCC6306
                rdx ^= rax;             //xor rdx, rax
                return rdx;
        }
        case 11:
        {
                r9 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184129);               //mov r9, [0x00000000081B764F]
                rax = rdx;              //mov rax, rdx
                rax >>= 0x12;           //shr rax, 0x12
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x24;           //shr rax, 0x24
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0xA;            //shr rax, 0x0A
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x14;           //shr rax, 0x14
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x28;           //shr rax, 0x28
                rdx ^= rax;             //xor rdx, rax
                rax = 0x8CABBD467C0219D3;               //mov rax, 0x8CABBD467C0219D3
                rdx *= rax;             //imul rdx, rax
                rax = 0xAB98E88DE9C18818;               //mov rax, 0xAB98E88DE9C18818
                rdx ^= rax;             //xor rdx, rax
                rdx -= r11;             //sub rdx, r11
                rax = r11;              //mov rax, r11
                uintptr_t RSP_0xFFFFFFFFFFFFFFEF;
                RSP_0xFFFFFFFFFFFFFFEF = GameGlobals::module_base + 0x8516;               //lea rax, [0xFFFFFFFFFE03BAD0] : RBP+0xFFFFFFFFFFFFFFEF
                rax *= RSP_0xFFFFFFFFFFFFFFEF;          //imul rax, [rbp-0x11]
                rdx += rax;             //add rdx, rax
                rax = 0;                //and rax, 0xFFFFFFFFC0000000
                rax = rotl64(rax, 0x10);               //rol rax, 0x10
                rax ^= r9;              //xor rax, r9
                rax = __bswap_64(rax);            //bswap rax
                rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x15);             //imul rdx, [rax+0x15]
                rax = 0x596E42B1953FE5C1;               //mov rax, 0x596E42B1953FE5C1
                rdx += rax;             //add rdx, rax
                return rdx;
        }
        case 12:
        {
                r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184129);              //mov r10, [0x00000000081B710E]
                rdx ^= r11;             //xor rdx, r11
                rax = GameGlobals::module_base + 0x1BAF;          //lea rax, [0xFFFFFFFFFE034819]
                rdx ^= rax;             //xor rdx, rax
                rdx -= r11;             //sub rdx, r11
                uintptr_t RSP_0xFFFFFFFFFFFFFF9F;
                RSP_0xFFFFFFFFFFFFFF9F = GameGlobals::module_base + 0x259F56F6;           //lea rax, [0x0000000023A286D4] : RBP+0xFFFFFFFFFFFFFF9F
                rdx += RSP_0xFFFFFFFFFFFFFF9F;          //add rdx, [rbp-0x61]
                rax = 0;                //and rax, 0xFFFFFFFFC0000000
                rax = rotl64(rax, 0x10);               //rol rax, 0x10
                rax ^= r10;             //xor rax, r10
                rax = __bswap_64(rax);            //bswap rax
                rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x15);             //imul rdx, [rax+0x15]
                rax = 0x294D76F27D0F6D85;               //mov rax, 0x294D76F27D0F6D85
                rdx -= rax;             //sub rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x8;            //shr rax, 0x08
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x10;           //shr rax, 0x10
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x20;           //shr rax, 0x20
                rdx ^= rax;             //xor rdx, rax
                rax = 0x40AE2552A77DAFE6;               //mov rax, 0x40AE2552A77DAFE6
                rdx -= r11;             //sub rdx, r11
                rdx ^= rax;             //xor rdx, rax
                rax = 0x6425FC1CEAFDBD3B;               //mov rax, 0x6425FC1CEAFDBD3B
                rdx *= rax;             //imul rdx, rax
                return rdx;
        }
        case 13:
        {
                r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184129);              //mov r10, [0x00000000081B6D16]
                rbx = GameGlobals::module_base;           //lea rbx, [0xFFFFFFFFFE032BE6]
                rdx += rbx;             //add rdx, rbx
                rax = rdx;              //mov rax, rdx
                rax >>= 0x10;           //shr rax, 0x10
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x20;           //shr rax, 0x20
                rdx ^= rax;             //xor rdx, rax
                rax = 0;                //and rax, 0xFFFFFFFFC0000000
                rax = rotl64(rax, 0x10);               //rol rax, 0x10
                rax ^= r10;             //xor rax, r10
                rax = __bswap_64(rax);            //bswap rax
                rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x15);             //imul rdx, [rax+0x15]
                rax = r11;              //mov rax, r11
                rax -= rbx;             //sub rax, rbx
                rdx += rax;             //add rdx, rax
                rax = 0x7AAF0F372FD53CD5;               //mov rax, 0x7AAF0F372FD53CD5
                rdx *= rax;             //imul rdx, rax
                rax = 0x4BE188FD7D45B824;               //mov rax, 0x4BE188FD7D45B824
                rdx -= rax;             //sub rdx, rax
                rdx ^= r11;             //xor rdx, r11
                rax = rdx;              //mov rax, rdx
                rax >>= 0x25;           //shr rax, 0x25
                rdx ^= rax;             //xor rdx, rax
                rax = 0x2F94247E3E6CDFF6;               //mov rax, 0x2F94247E3E6CDFF6
                rdx -= rax;             //sub rdx, rax
                return rdx;
        }
        case 14:
        {
                r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184129);              //mov r10, [0x00000000081B6889]
                rbx = GameGlobals::module_base;           //lea rbx, [0xFFFFFFFFFE03274E]
                rax = 0x59FC5D34C1D95075;               //mov rax, 0x59FC5D34C1D95075
                rdx += rax;             //add rdx, rax
                rax = 0x113F93E895C764EB;               //mov rax, 0x113F93E895C764EB
                rdx *= rax;             //imul rdx, rax
                rax = GameGlobals::module_base + 0x3BFCF952;              //lea rax, [0x000000003A001DA5]
                rax = ~rax;             //not rax
                rax ^= r11;             //xor rax, r11
                rdx -= rax;             //sub rdx, rax
                rax = 0;                //and rax, 0xFFFFFFFFC0000000
                rax = rotl64(rax, 0x10);               //rol rax, 0x10
                rax ^= r10;             //xor rax, r10
                rax = __bswap_64(rax);            //bswap rax
                rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x15);             //imul rdx, [rax+0x15]
                rdx -= rbx;             //sub rdx, rbx
                rax = rdx;              //mov rax, rdx
                rax >>= 0x23;           //shr rax, 0x23
                rdx ^= rax;             //xor rdx, rax
                uintptr_t RSP_0xFFFFFFFFFFFFFF9F;
                RSP_0xFFFFFFFFFFFFFF9F = 0x3D4F5BB3C70BE95B;            //mov rax, 0x3D4F5BB3C70BE95B : RBP+0xFFFFFFFFFFFFFF9F
                rdx *= RSP_0xFFFFFFFFFFFFFF9F;          //imul rdx, [rbp-0x61]
                rax = r11;              //mov rax, r11
                uintptr_t RSP_0xFFFFFFFFFFFFFFA7;
                RSP_0xFFFFFFFFFFFFFFA7 = GameGlobals::module_base + 0x9DA7;               //lea rax, [0xFFFFFFFFFE03C500] : RBP+0xFFFFFFFFFFFFFFA7
                rax ^= RSP_0xFFFFFFFFFFFFFFA7;          //xor rax, [rbp-0x59]
                rdx -= rax;             //sub rdx, rax
                return rdx;
        }
        case 15:
        {
                rbx = GameGlobals::module_base;           //lea rbx, [0xFFFFFFFFFE0322D0]
                r9 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184129);               //mov r9, [0x00000000081B6389]
                rax = rdx;              //mov rax, rdx
                rax >>= 0x13;           //shr rax, 0x13
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x26;           //shr rax, 0x26
                rdx ^= rax;             //xor rdx, rax
                rax = 0x63FD32E967945525;               //mov rax, 0x63FD32E967945525
                rdx -= rax;             //sub rdx, rax
                rdx += r11;             //add rdx, r11
                rax = 0xB85215B9839B7D9;                //mov rax, 0xB85215B9839B7D9
                rdx += rax;             //add rdx, rax
                rdx ^= rbx;             //xor rdx, rbx
                rax = 0;                //and rax, 0xFFFFFFFFC0000000
                rax = rotl64(rax, 0x10);               //rol rax, 0x10
                rax ^= r9;              //xor rax, r9
                rax = __bswap_64(rax);            //bswap rax
                rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x15);             //imul rdx, [rax+0x15]
                rax = rdx;              //mov rax, rdx
                rax >>= 0x5;            //shr rax, 0x05
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0xA;            //shr rax, 0x0A
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x14;           //shr rax, 0x14
                rdx ^= rax;             //xor rdx, rax
                rax = rdx;              //mov rax, rdx
                rax >>= 0x28;           //shr rax, 0x28
                rdx ^= rax;             //xor rdx, rax
                rax = 0xE52EBF353AE32CDB;               //mov rax, 0xE52EBF353AE32CDB
                rdx *= rax;             //imul rdx, rax
                return rdx;
        }
        }
}
uintptr_t GameDecrypts::decrypt_bone_base()
{
        const uint64_t mb = GameGlobals::module_base;
        uint64_t rax = mb, rbx = mb, rcx = mb, rdx = mb, rdi = mb, rsi = mb, r8 = mb, r9 = mb, r10 = mb, r11 = mb, r12 = mb, r13 = mb, r14 = mb, r15 = mb;
        rax = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xDA89AC8);
        if(!rax)
                return rax;
        rbx = GameGlobals::process_peb;              //mov rbx, gs:[rcx]
        rcx = rbx;              //mov rcx, rbx
        rcx >>= 0x1C;           //shr rcx, 0x1C
        rcx &= 0xF;
        switch(rcx) {
        case 0:
        {
                r9 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184221);               //mov r9, [0x0000000007F81643]
                rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
                rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
                rcx ^= r9;              //xor rcx, r9
                rcx = ~rcx;             //not rcx
                rax *= k_memory.ReadNb<uintptr_t>(rcx + 0x13);             //imul rax, [rcx+0x13]
                rax += rbx;             //add rax, rbx
                rcx = rax;              //mov rcx, rax
                rax >>= 0x13;           //shr rax, 0x13
                rcx ^= rax;             //xor rcx, rax
                rax = rcx;              //mov rax, rcx
                rax >>= 0x26;           //shr rax, 0x26
                rax ^= rcx;             //xor rax, rcx
                rax -= rbx;             //sub rax, rbx
                rcx = 0xD5A6F9222EC0CD8B;               //mov rcx, 0xD5A6F9222EC0CD8B
                rax *= rcx;             //imul rax, rcx
                rcx = 0xBB2862E8C0DD851B;               //mov rcx, 0xBB2862E8C0DD851B
                rax *= rcx;             //imul rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x4;            //shr rcx, 0x04
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x8;            //shr rcx, 0x08
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x10;           //shr rcx, 0x10
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x20;           //shr rcx, 0x20
                rax ^= rcx;             //xor rax, rcx
                return rax;
        }
        case 1:
        {
                r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184221);              //mov r10, [0x0000000007F811F4]
                rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
                rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
                rcx ^= r10;             //xor rcx, r10
                rcx = ~rcx;             //not rcx
                rax *= k_memory.ReadNb<uintptr_t>(rcx + 0x13);             //imul rax, [rcx+0x13]
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x1E;           //shr rcx, 0x1E
                rax ^= rcx;             //xor rax, rcx
                rdx = GameGlobals::module_base + 0x6179D5AB;              //lea rdx, [0x000000005F59A35C]
                rdx -= rbx;             //sub rdx, rbx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x3C;           //shr rcx, 0x3C
                rdx ^= rcx;             //xor rdx, rcx
                rax ^= rdx;             //xor rax, rdx
                rax += rbx;             //add rax, rbx
                rcx = 0xC430FCF5AB246D6;                //mov rcx, 0xC430FCF5AB246D6
                rax += rcx;             //add rax, rcx
                rcx = 0x8A220291A10CAF87;               //mov rcx, 0x8A220291A10CAF87
                rax *= rcx;             //imul rax, rcx
                rcx = 0x7FAA38A95F85A6FD;               //mov rcx, 0x7FAA38A95F85A6FD
                rax ^= rcx;             //xor rax, rcx
                rcx = GameGlobals::module_base;           //lea rcx, [0xFFFFFFFFFDDFCF55]
                rax -= rcx;             //sub rax, rcx
                return rax;
        }
        case 2:
        {
                r14 = GameGlobals::module_base + 0x2281;          //lea r14, [0xFFFFFFFFFDDFEE40]
                r13 = GameGlobals::module_base + 0x66A0AFC0;              //lea r13, [0x0000000064807B70]
                r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184221);              //mov r10, [0x0000000007F80D85]
                rdx = r13;              //mov rdx, r13
                rdx = ~rdx;             //not rdx
                rdx *= rbx;             //imul rdx, rbx
                rcx = rbx;              //mov rcx, rbx
                rcx = ~rcx;             //not rcx
                rdx += rcx;             //add rdx, rcx
                rcx = GameGlobals::module_base + 0xBA28;          //lea rcx, [0xFFFFFFFFFDE082E6]
                rax += rcx;             //add rax, rcx
                rax += rdx;             //add rax, rdx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x13;           //shr rcx, 0x13
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x26;           //shr rcx, 0x26
                rax ^= rcx;             //xor rax, rcx
                rcx = r14;              //mov rcx, r14
                rcx ^= rbx;             //xor rcx, rbx
                rax += rcx;             //add rax, rcx
                rcx = 0x975CC895B7E831F1;               //mov rcx, 0x975CC895B7E831F1
                rax ^= rcx;             //xor rax, rcx
                rcx = 0x3B97C5DC626E056F;               //mov rcx, 0x3B97C5DC626E056F
                rax *= rcx;             //imul rax, rcx
                rcx = 0x5534067E232C6632;               //mov rcx, 0x5534067E232C6632
                rax += rcx;             //add rax, rcx
                rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
                rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
                rcx ^= r10;             //xor rcx, r10
                rcx = ~rcx;             //not rcx
                rax *= k_memory.ReadNb<uintptr_t>(rcx + 0x13);             //imul rax, [rcx+0x13]
                return rax;
        }
        case 3:
        {
                r13 = GameGlobals::module_base + 0x50F8B6F5;              //lea r13, [0x000000004ED87D3D]
                r9 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184221);               //mov r9, [0x0000000007F8079F]
                rcx = r13;              //mov rcx, r13
                rcx = ~rcx;             //not rcx
                rcx ^= rbx;             //xor rcx, rbx
                rax += rcx;             //add rax, rcx
                rcx = GameGlobals::module_base;           //lea rcx, [0xFFFFFFFFFDDFC195]
                rax += rcx;             //add rax, rcx
                rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
                rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
                rcx ^= r9;              //xor rcx, r9
                rcx = ~rcx;             //not rcx
                rax *= k_memory.ReadNb<uintptr_t>(rcx + 0x13);             //imul rax, [rcx+0x13]
                rcx = 0x60F6A79B0C8456B1;               //mov rcx, 0x60F6A79B0C8456B1
                rax += rcx;             //add rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x1B;           //shr rcx, 0x1B
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x36;           //shr rcx, 0x36
                rax ^= rcx;             //xor rax, rcx
                rcx = 0x648FCA6FE7D44377;               //mov rcx, 0x648FCA6FE7D44377
                rax -= rcx;             //sub rax, rcx
                rcx = 0xC462FCF18E2C2995;               //mov rcx, 0xC462FCF18E2C2995
                rax *= rcx;             //imul rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x16;           //shr rcx, 0x16
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x2C;           //shr rcx, 0x2C
                rax ^= rcx;             //xor rax, rcx
                return rax;
        }
        case 4:
        {
                r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184221);              //mov r10, [0x0000000007F80224]
                rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
                rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
                rcx ^= r10;             //xor rcx, r10
                rcx = ~rcx;             //not rcx
                rax *= k_memory.ReadNb<uintptr_t>(rcx + 0x13);             //imul rax, [rcx+0x13]
                rax -= rbx;             //sub rax, rbx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x18;           //shr rcx, 0x18
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x30;           //shr rcx, 0x30
                rax ^= rcx;             //xor rax, rcx
                rcx = 0xB1A93FB4C084CAB9;               //mov rcx, 0xB1A93FB4C084CAB9
                rax *= rcx;             //imul rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x24;           //shr rcx, 0x24
                rcx ^= rax;             //xor rcx, rax
                rax = 0x352F8A796F79706B;               //mov rax, 0x352F8A796F79706B
                rcx ^= rax;             //xor rcx, rax
                rax = GameGlobals::module_base;           //lea rax, [0xFFFFFFFFFDDFBC59]
                rcx -= rax;             //sub rcx, rax
                rax = rbx + 0xffffffffa5917e54;                 //lea rax, [rbx-0x5A6E81AC]
                rax += rcx;             //add rax, rcx
                rcx = 0xA920BAB7A21DDE47;               //mov rcx, 0xA920BAB7A21DDE47
                rax *= rcx;             //imul rax, rcx
                return rax;
        }
        case 5:
        {
                r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184221);              //mov r10, [0x0000000007F7FDA4]
                rdx = GameGlobals::module_base + 0x661;           //lea rdx, [0xFFFFFFFFFDDFC174]
                uintptr_t RSP_0x78;
                RSP_0x78 = 0x19A86082B9386E61;          //mov rcx, 0x19A86082B9386E61 : RSP+0x78
                rax ^= RSP_0x78;                //xor rax, [rsp+0x78]
                rcx = rbx;              //mov rcx, rbx
                rcx = ~rcx;             //not rcx
                uintptr_t RSP_0x30;
                RSP_0x30 = GameGlobals::module_base + 0x89B8;             //lea rcx, [0xFFFFFFFFFDE04502] : RSP+0x30
                rcx ^= RSP_0x30;                //xor rcx, [rsp+0x30]
                rax -= rcx;             //sub rax, rcx
                rcx = 0x6CF5D40C805C3929;               //mov rcx, 0x6CF5D40C805C3929
                rax *= rcx;             //imul rax, rcx
                rax -= rbx;             //sub rax, rbx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x23;           //shr rcx, 0x23
                rax ^= rcx;             //xor rax, rcx
                rcx = 0xEA2BDCA216FA84E;                //mov rcx, 0xEA2BDCA216FA84E
                rax += rcx;             //add rax, rcx
                rcx = rdx;              //mov rcx, rdx
                rcx = ~rcx;             //not rcx
                rcx -= rbx;             //sub rcx, rbx
                rax += rcx;             //add rax, rcx
                rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
                rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
                rcx ^= r10;             //xor rcx, r10
                rcx = ~rcx;             //not rcx
                rax *= k_memory.ReadNb<uintptr_t>(rcx + 0x13);             //imul rax, [rcx+0x13]
                return rax;
        }
        case 6:
        {
                r9 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184221);               //mov r9, [0x0000000007F7F885]
                rcx = 0x143119596E0AB6F4;               //mov rcx, 0x143119596E0AB6F4
                rcx -= rbx;             //sub rcx, rbx
                rax += rcx;             //add rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x14;           //shr rcx, 0x14
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x28;           //shr rcx, 0x28
                rax ^= rcx;             //xor rax, rcx
                rcx = GameGlobals::module_base;           //lea rcx, [0xFFFFFFFFFDDFB4B1]
                rax -= rcx;             //sub rax, rcx
                rcx = 0xD0FF53657C7A437;                //mov rcx, 0xD0FF53657C7A437
                rax += rcx;             //add rax, rcx
                rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
                rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
                rcx ^= r9;              //xor rcx, r9
                rcx = ~rcx;             //not rcx
                rax *= k_memory.ReadNb<uintptr_t>(rcx + 0x13);             //imul rax, [rcx+0x13]
                rcx = 0x1946435536018835;               //mov rcx, 0x1946435536018835
                rax *= rcx;             //imul rax, rcx
                rax -= rbx;             //sub rax, rbx
                return rax;
        }
        case 7:
        {
                r11 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184221);              //mov r11, [0x0000000007F7F474]
                rdx = GameGlobals::module_base + 0x32D6EFEE;              //lea rdx, [0x0000000030B6A1E7]
                r8 = 0;                 //and r8, 0xFFFFFFFFC0000000
                r8 = rotl64(r8, 0x10);                 //rol r8, 0x10
                r8 ^= r11;              //xor r8, r11
                rcx = rbx;              //mov rcx, rbx
                rcx = ~rcx;             //not rcx
                r8 = ~r8;               //not r8
                rax += rcx;             //add rax, rcx
                rax += rdx;             //add rax, rdx
                rax ^= rbx;             //xor rax, rbx
                rax *= k_memory.ReadNb<uintptr_t>(r8 + 0x13);              //imul rax, [r8+0x13]
                rcx = 0x6B8832A948DD0921;               //mov rcx, 0x6B8832A948DD0921
                rax += rcx;             //add rax, rcx
                rcx = 0x9D382E284DCFD7C7;               //mov rcx, 0x9D382E284DCFD7C7
                rax *= rcx;             //imul rax, rcx
                rcx = 0x4A2F2EC6D9595386;               //mov rcx, 0x4A2F2EC6D9595386
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x7;            //shr rcx, 0x07
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0xE;            //shr rcx, 0x0E
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x1C;           //shr rcx, 0x1C
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x38;           //shr rcx, 0x38
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x15;           //shr rcx, 0x15
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x2A;           //shr rcx, 0x2A
                rax ^= rcx;             //xor rax, rcx
                return rax;
        }
        case 8:
        {
                r14 = GameGlobals::module_base + 0x98C7;          //lea r14, [0xFFFFFFFFFDE04557]
                r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184221);              //mov r10, [0x0000000007F7EE53]
                rcx = GameGlobals::module_base;           //lea rcx, [0xFFFFFFFFFDDFA8A0]
                rax ^= rcx;             //xor rax, rcx
                rcx = 0xB9B101CE6C8E2F91;               //mov rcx, 0xB9B101CE6C8E2F91
                rax *= rcx;             //imul rax, rcx
                rcx = GameGlobals::module_base;           //lea rcx, [0xFFFFFFFFFDDFAB97]
                rax -= rcx;             //sub rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0xC;            //shr rcx, 0x0C
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x18;           //shr rcx, 0x18
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x30;           //shr rcx, 0x30
                rax ^= rcx;             //xor rax, rcx
                rax -= rbx;             //sub rax, rbx
                rcx = 0x4F54898D891371A4;               //mov rcx, 0x4F54898D891371A4
                rax += rcx;             //add rax, rcx
                rdx = 0;                //and rdx, 0xFFFFFFFFC0000000
                rcx = r14;              //mov rcx, r14
                rdx = rotl64(rdx, 0x10);               //rol rdx, 0x10
                rcx ^= rbx;             //xor rcx, rbx
                rax -= rcx;             //sub rax, rcx
                rdx ^= r10;             //xor rdx, r10
                rdx = ~rdx;             //not rdx
                rax *= k_memory.ReadNb<uintptr_t>(rdx + 0x13);             //imul rax, [rdx+0x13]
                return rax;
        }
        case 9:
        {
                r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184221);              //mov r10, [0x0000000007F7E9F7]
                rcx = 0x5C495DB1FF8A0C7D;               //mov rcx, 0x5C495DB1FF8A0C7D
                rax *= rcx;             //imul rax, rcx
                rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
                rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
                rcx ^= r10;             //xor rcx, r10
                rcx = ~rcx;             //not rcx
                rax *= k_memory.ReadNb<uintptr_t>(rcx + 0x13);             //imul rax, [rcx+0x13]
                rcx = GameGlobals::module_base;           //lea rcx, [0xFFFFFFFFFDDFA752]
                rax -= rcx;             //sub rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x13;           //shr rcx, 0x13
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x26;           //shr rcx, 0x26
                rax ^= rcx;             //xor rax, rcx
                rcx = GameGlobals::module_base;           //lea rcx, [0xFFFFFFFFFDDFA511]
                rax += rcx;             //add rax, rcx
                rdx = GameGlobals::module_base;           //lea rdx, [0xFFFFFFFFFDDFA4F8]
                rdx += rbx;             //add rdx, rbx
                rcx = 0x931F45DADBA6534A;               //mov rcx, 0x931F45DADBA6534A
                rax += rcx;             //add rax, rcx
                rax += rdx;             //add rax, rdx
                rcx = 0x7254D9C4F5E0407F;               //mov rcx, 0x7254D9C4F5E0407F
                rax *= rcx;             //imul rax, rcx
                return rax;
        }
        case 10:
        {
                r9 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184221);               //mov r9, [0x0000000007F7E578]
                rcx = 0x16092956D42CB466;               //mov rcx, 0x16092956D42CB466
                rax += rcx;             //add rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x28;           //shr rcx, 0x28
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x22;           //shr rcx, 0x22
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0xB;            //shr rcx, 0x0B
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x16;           //shr rcx, 0x16
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x2C;           //shr rcx, 0x2C
                rax ^= rcx;             //xor rax, rcx
                rcx = 0xC2E2E61ED49F5991;               //mov rcx, 0xC2E2E61ED49F5991
                rax *= rcx;             //imul rax, rcx
                rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
                rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
                rcx ^= r9;              //xor rcx, r9
                rcx = ~rcx;             //not rcx
                rax *= k_memory.ReadNb<uintptr_t>(rcx + 0x13);             //imul rax, [rcx+0x13]
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x4;            //shr rcx, 0x04
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x8;            //shr rcx, 0x08
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x10;           //shr rcx, 0x10
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x20;           //shr rcx, 0x20
                rax ^= rcx;             //xor rax, rcx
                rax -= rbx;             //sub rax, rbx
                return rax;
        }
        case 11:
        {
                r13 = GameGlobals::module_base + 0x3E6FD0B3;              //lea r13, [0x000000003C4F6EF1]
                r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184221);              //mov r10, [0x0000000007F7DFAC]
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x1B;           //shr rcx, 0x1B
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x36;           //shr rcx, 0x36
                rax ^= rcx;             //xor rax, rcx
                r14 = GameGlobals::module_base;           //lea r14, [0xFFFFFFFFFDDF9881]
                rax += r14;             //add rax, r14
                r14 = GameGlobals::module_base + 0x27799030;              //lea r14, [0x000000002559289A]
                rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
                rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
                rcx ^= r10;             //xor rcx, r10
                rcx = ~rcx;             //not rcx
                rax *= k_memory.ReadNb<uintptr_t>(rcx + 0x13);             //imul rax, [rcx+0x13]
                rcx = 0x6E5FECB626B1C472;               //mov rcx, 0x6E5FECB626B1C472
                rax += rcx;             //add rax, rcx
                rdx = r13;              //mov rdx, r13
                rdx = ~rdx;             //not rdx
                rdx ^= rbx;             //xor rdx, rbx
                rcx = 0xF5121CBF37E46BBB;               //mov rcx, 0xF5121CBF37E46BBB
                rax += rcx;             //add rax, rcx
                rax += rdx;             //add rax, rdx
                rcx = r14;              //mov rcx, r14
                rcx ^= rbx;             //xor rcx, rbx
                rax -= rcx;             //sub rax, rcx
                rcx = 0xD2D6E8735A76DE2D;               //mov rcx, 0xD2D6E8735A76DE2D
                rax *= rcx;             //imul rax, rcx
                return rax;
        }
        case 12:
        {
                rdx = GameGlobals::module_base + 0xF737;          //lea rdx, [0xFFFFFFFFFDE08F62]
                r13 = GameGlobals::module_base + 0x1124573F;              //lea r13, [0x000000000F03EF4C]
                r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184221);              //mov r10, [0x0000000007F7D98E]
                rax += rbx;             //add rax, rbx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x10;           //shr rcx, 0x10
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x20;           //shr rcx, 0x20
                rax ^= rcx;             //xor rax, rcx
                uintptr_t RSP_0x50;
                RSP_0x50 = 0x636BE495B0FA383E;          //mov rcx, 0x636BE495B0FA383E : RSP+0x50
                rax ^= RSP_0x50;                //xor rax, [rsp+0x50]
                rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
                rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
                rcx ^= r10;             //xor rcx, r10
                rcx = ~rcx;             //not rcx
                rax *= k_memory.ReadNb<uintptr_t>(rcx + 0x13);             //imul rax, [rcx+0x13]
                rcx = r13;              //mov rcx, r13
                rcx ^= rbx;             //xor rcx, rbx
                rax ^= rcx;             //xor rax, rcx
                rcx = 0x8812EF99851F0715;               //mov rcx, 0x8812EF99851F0715
                rax *= rcx;             //imul rax, rcx
                rcx = rbx;              //mov rcx, rbx
                rcx = ~rcx;             //not rcx
                rcx += rdx;             //add rcx, rdx
                rax ^= rcx;             //xor rax, rcx
                rcx = GameGlobals::module_base + 0xB69;           //lea rcx, [0xFFFFFFFFFDDFA1C3]
                rcx -= rbx;             //sub rcx, rbx
                rax += rcx;             //add rax, rcx
                return rax;
        }
        case 13:
        {
                r13 = GameGlobals::module_base + 0x5EF7;          //lea r13, [0xFFFFFFFFFDDFF21B]
                r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184221);              //mov r10, [0x0000000007F7D4EB]
                rcx = rax;              //mov rcx, rax
                rcx >>= 0xA;            //shr rcx, 0x0A
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x14;           //shr rcx, 0x14
                rax ^= rcx;             //xor rax, rcx
                rdx = rbx;              //mov rdx, rbx
                rcx = rax;              //mov rcx, rax
                rdx = ~rdx;             //not rdx
                rcx >>= 0x28;           //shr rcx, 0x28
                rdx ^= r13;             //xor rdx, r13
                rax ^= rcx;             //xor rax, rcx
                rax += rdx;             //add rax, rdx
                rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
                rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
                rcx ^= r10;             //xor rcx, r10
                rcx = ~rcx;             //not rcx
                rax *= k_memory.ReadNb<uintptr_t>(rcx + 0x13);             //imul rax, [rcx+0x13]
                rcx = 0x736E085CD239F4CB;               //mov rcx, 0x736E085CD239F4CB
                rax *= rcx;             //imul rax, rcx
                rax -= rbx;             //sub rax, rbx
                rcx = GameGlobals::module_base;           //lea rcx, [0xFFFFFFFFFDDF8E32]
                rax ^= rcx;             //xor rax, rcx
                rcx = 0x7DA51A97E3053243;               //mov rcx, 0x7DA51A97E3053243
                rax *= rcx;             //imul rax, rcx
                rcx = 0x785CF31817D043AC;               //mov rcx, 0x785CF31817D043AC
                rax ^= rcx;             //xor rax, rcx
                return rax;
        }
        case 14:
        {
                r13 = GameGlobals::module_base + 0x16A6;          //lea r13, [0xFFFFFFFFFDDFA3F5]
                r14 = GameGlobals::module_base + 0xF57E;          //lea r14, [0xFFFFFFFFFDE082BE]
                r9 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184221);               //mov r9, [0x0000000007F7CF10]
                rcx = rbx;              //mov rcx, rbx
                rcx = ~rcx;             //not rcx
                rcx *= r14;             //imul rcx, r14
                rax += rcx;             //add rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x1A;           //shr rcx, 0x1A
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x34;           //shr rcx, 0x34
                rax ^= rcx;             //xor rax, rcx
                r11 = 0xBE40C084BA769FA;                //mov r11, 0xBE40C084BA769FA
                rcx = r13;              //mov rcx, r13
                rcx *= rbx;             //imul rcx, rbx
                rcx += r11;             //add rcx, r11
                rax += rcx;             //add rax, rcx
                rcx = 0xB92AAB45027C43E2;               //mov rcx, 0xB92AAB45027C43E2
                rax ^= rcx;             //xor rax, rcx
                rcx = GameGlobals::module_base + 0x4FED906B;              //lea rcx, [0x000000004DCD1AB1]
                rcx += rbx;             //add rcx, rbx
                rax += rcx;             //add rax, rcx
                rcx = 0xBCCB1C832C79BF0B;               //mov rcx, 0xBCCB1C832C79BF0B
                rax *= rcx;             //imul rax, rcx
                rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
                rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
                rcx ^= r9;              //xor rcx, r9
                rcx = ~rcx;             //not rcx
                rax *= k_memory.ReadNb<uintptr_t>(rcx + 0x13);             //imul rax, [rcx+0x13]
                return rax;
        }
        case 15:
        {
                r14 = GameGlobals::module_base + 0x31081C40;              //lea r14, [0x000000002EE7A45C]
                r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xA184221);              //mov r10, [0x0000000007F7C9DF]
                rdx = 0;                //and rdx, 0xFFFFFFFFC0000000
                rdx = rotl64(rdx, 0x10);               //rol rdx, 0x10
                rcx = rax;              //mov rcx, rax
                rdx ^= r10;             //xor rdx, r10
                rcx >>= 0x20;           //shr rcx, 0x20
                rdx = ~rdx;             //not rdx
                rax ^= rcx;             //xor rax, rcx
                rcx = r14;              //mov rcx, r14
                rcx ^= rbx;             //xor rcx, rbx
                rax *= k_memory.ReadNb<uintptr_t>(rdx + 0x13);             //imul rax, [rdx+0x13]
                rax -= rcx;             //sub rax, rcx
                rcx = 0xFC32828FC4E7EFD1;               //mov rcx, 0xFC32828FC4E7EFD1
                rax *= rcx;             //imul rax, rcx
                rax -= rbx;             //sub rax, rbx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x3;            //shr rcx, 0x03
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x6;            //shr rcx, 0x06
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0xC;            //shr rcx, 0x0C
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x18;           //shr rcx, 0x18
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x30;           //shr rcx, 0x30
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x8;            //shr rcx, 0x08
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x10;           //shr rcx, 0x10
                rax ^= rcx;             //xor rax, rcx
                rcx = rax;              //mov rcx, rax
                rcx >>= 0x20;           //shr rcx, 0x20
                rax ^= rcx;             //xor rax, rcx
                rcx = 0x8D793715ED015397;               //mov rcx, 0x8D793715ED015397
                rax *= rcx;             //imul rax, rcx
                return rax;
        }
        }
}
