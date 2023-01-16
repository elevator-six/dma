#include "Game.h"
#include <thread>
#include <stdint.h>

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
	uint64_t c_bone	      = 0;
	uint64_t client_base  = 0;

        uintptr_t currentvisoffset = 0;
        uintptr_t last_visible_offset = 0;

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

	if (v_transform.z < 0.01f)
			return false;
	screen_pos->x = ((refdef.width / 2) * (1 - (v_transform.x / refdef.view.tan_half_fov.x / v_transform.z)));
	screen_pos->y = ((refdef.height / 2) * (1 - (v_transform.y / refdef.view.tan_half_fov.y / v_transform.z)));

	if (screen_pos->x < 1 || screen_pos->y < 1 || (screen_pos->x > refdef.width) || (screen_pos->y > refdef.height)) {
			return false;
	}

	return true;
}

Vector get_bone_base_pos()
{
        return k_memory.ReadNb<Vector>(GameGlobals::client + offsets::bone::bone_base);
}

Vector GameFunctions::get_bone_position(const Vector& base_pos, const int bone)
{
        const uintptr_t bone_ptr = k_memory.ReadNb<uintptr_t>(GameGlobals::c_bone + (bone * offsets::bone::size) + offsets::bone::offset);

        Vector pos = k_memory.ReadNb<Vector>(bone_ptr + ((uint64_t)bone * 0x20) + 0x10);
        pos.x += base_pos.x;
        pos.y += base_pos.y;
        pos.z += base_pos.z;
        return pos;
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
	uint64_t baseAddr = GameGlobals::module_base;

	k_memory.Read<int>(GameGlobals::module_base + (offsets::ref_def_ptr + 0x00), crypt.ref0);
	k_memory.Read<int>(GameGlobals::module_base + (offsets::ref_def_ptr + 0x04), crypt.ref1);
	k_memory.Read<int>(GameGlobals::module_base + (offsets::ref_def_ptr + 0x08), crypt.ref2);

                // REF0: 2052724712
                // REF1: -683526162
                // REF2: 1849860978

	eint64_t offset1 = baseAddr + offsets::ref_def_ptr;
	eint64_t offset2 = baseAddr + offsets::ref_def_ptr + 0x4;

	eint64_t bit1 = xor_bit((eint64_t)(crypt.ref2), offset1);
	eint64_t bit2 = xor_bit((eint64_t)(crypt.ref2), offset2);

	eint64_t add_l = bit1 + 2;
	eint64_t add_u = bit2 + 2;

	eint64_t final_l = bit1 * add_l;
	eint64_t final_u = bit2 * add_u;

	DWORD l_bit = (uint32_t)(crypt.ref0) ^ (uint32_t)(final_l);
	eint64_t u_bit = (uint32_t)(crypt.ref1) ^ (uint32_t)(final_u);

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

uint16_t player_t::team_id()
{
	uint16_t ret;
	k_memory.Read<uint16_t>(address + offsets::player::team, ret);
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

uintptr_t GameDecrypts::decrypt_client_info()
{
    const uint64_t mb = GameGlobals::module_base;
    uint64_t rax = mb, rbx = mb, rcx = mb, rdx = mb, rdi = mb, rsi = mb, r8 = mb, r9 = mb, r10 = mb, r11 = mb, r12 = mb, r13 = mb, r14 = mb, r15 = mb;
    rbx = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x12F837B8);
    if (!rbx)
        return rbx;
    r8 = ~GameGlobals::process_peb;               //mov r8, gs:[rax]
    rbx ^= r8;              //xor rbx, r8
    rax = rbx;              //mov rax, rbx
    rax >>= 0x16;           //shr rax, 0x16
    rbx ^= rax;             //xor rbx, rax
    rax = rbx;              //mov rax, rbx
    rax >>= 0x2C;           //shr rax, 0x2C
    rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
    rbx ^= rax;             //xor rbx, rax
    rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
    rcx ^= k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D0D6);             //xor rcx, [0x0000000008084B29]
    rax = 0xC6E5078761011A59;               //mov rax, 0xC6E5078761011A59
    rbx *= rax;             //imul rbx, rax
    rax = GameGlobals::module_base;           //lea rax, [0xFFFFFFFFFE0F7A3E]
    rcx = ~rcx;             //not rcx
    rbx += rax;             //add rbx, rax
    rax = GameGlobals::module_base + 0x1673;          //lea rax, [0xFFFFFFFFFE0F90A4]
    rax = ~rax;             //not rax
    rax += r8;              //add rax, r8
    rbx *= k_memory.ReadNb<uintptr_t>(rcx + 0x17);             //imul rbx, [rcx+0x17]
    rbx ^= rax;             //xor rbx, rax
    return rbx;
}
uintptr_t GameDecrypts::decrypt_client_base()
{
    const uint64_t mb = GameGlobals::module_base;
    uint64_t rax = mb, rbx = mb, rcx = mb, rdx = mb, rdi = mb, rsi = mb, r8 = mb, r9 = mb, r10 = mb, r11 = mb, r12 = mb, r13 = mb, r14 = mb, r15 = mb;
    rdx = k_memory.ReadNb<uintptr_t>(GameGlobals::client + 0x10e670);
    if (!rdx)
        return rdx;
    r11 = GameGlobals::process_peb;              //mov r11, gs:[rax]
    rax = r11;              //mov rax, r11
    rax <<= 0x1D;           //shl rax, 0x1D
    rax = __bswap_64(rax);            //bswap rax
    rax &= 0xF;
    switch (rax) {
    case 0:
    {
        r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D0F8);              //mov r10, [0x0000000007B63EFE]
        rax = rdx;              //mov rax, rdx
        rax >>= 0x15;           //shr rax, 0x15
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x2A;           //shr rax, 0x2A
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x14;           //shr rax, 0x14
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x28;           //shr rax, 0x28
        rdx ^= rax;             //xor rdx, rax
        rax = 0x2C30C13E7F5B89D3;               //mov rax, 0x2C30C13E7F5B89D3
        rdx -= rax;             //sub rdx, rax
        rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
        rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
        rcx ^= r10;             //xor rcx, r10
        rax = r11;              //mov rax, r11
        rax -= GameGlobals::module_base;          //sub rax, [rbp-0x30] -- didn't find trace -> use base
        rcx = ~rcx;             //not rcx
        rax += 0xFFFFFFFFA4BF03FE;              //add rax, 0xFFFFFFFFA4BF03FE
        rdx += rax;             //add rdx, rax
        rdx *= k_memory.ReadNb<uintptr_t>(rcx + 0x9);              //imul rdx, [rcx+0x09]
        rax = 0x33AA95033B659685;               //mov rax, 0x33AA95033B659685
        rdx *= rax;             //imul rdx, rax
        rdx -= r11;             //sub rdx, r11
        rax = 0xC3014AD9BBCF2773;               //mov rax, 0xC3014AD9BBCF2773
        rdx *= rax;             //imul rdx, rax
        return rdx;
    }
    case 1:
    {
        r13 = GameGlobals::module_base + 0xCFD0;          //lea r13, [0xFFFFFFFFFDBE3940]
        r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D0F8);              //mov r10, [0x0000000007B63A1F]
        rax = 0xD444113F4CC2BBAB;               //mov rax, 0xD444113F4CC2BBAB
        rdx *= rax;             //imul rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x27;           //shr rax, 0x27
        rdx ^= rax;             //xor rdx, rax
        rax = GameGlobals::module_base;           //lea rax, [0xFFFFFFFFFDBD68BA]
        rdx ^= rax;             //xor rdx, rax
        rax = 0xDD8F2C108EBF85C7;               //mov rax, 0xDD8F2C108EBF85C7
        rdx ^= rax;             //xor rdx, rax
        rax = 0;                //and rax, 0xFFFFFFFFC0000000
        rax = rotl64(rax, 0x10);               //rol rax, 0x10
        rax ^= r10;             //xor rax, r10
        rax = ~rax;             //not rax
        rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x9);              //imul rdx, [rax+0x09]
        rax = GameGlobals::module_base;           //lea rax, [0xFFFFFFFFFDBD67BF]
        rdx += rax;             //add rdx, rax
        rax = 0x91B097F7D53A92A5;               //mov rax, 0x91B097F7D53A92A5
        rdx ^= r11;             //xor rdx, r11
        rdx ^= r13;             //xor rdx, r13
        rdx *= rax;             //imul rdx, rax
        return rdx;
    }
    case 2:
    {
        r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D0F8);              //mov r10, [0x0000000007B636AA]
        rcx = r11;              //mov rcx, r11
        rax = GameGlobals::module_base + 0x7DDBB9C0;              //lea rax, [0x000000007B991B9F]
        rcx = ~rcx;             //not rcx
        rax = ~rax;             //not rax
        rcx *= rax;             //imul rcx, rax
        rdx ^= rcx;             //xor rdx, rcx
        rcx = GameGlobals::module_base + 0xBB0A;          //lea rcx, [0xFFFFFFFFFDBE205C]
        rax = GameGlobals::module_base;           //lea rax, [0xFFFFFFFFFDBD646A]
        rdx += rax;             //add rdx, rax
        rax = GameGlobals::module_base;           //lea rax, [0xFFFFFFFFFDBD63C8]
        rdx -= rax;             //sub rdx, rax
        rax = 0;                //and rax, 0xFFFFFFFFC0000000
        rax = rotl64(rax, 0x10);               //rol rax, 0x10
        rax ^= r10;             //xor rax, r10
        rax = ~rax;             //not rax
        rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x9);              //imul rdx, [rax+0x09]
        rax = 0x1BF6889B67DFE5CF;               //mov rax, 0x1BF6889B67DFE5CF
        rdx *= rax;             //imul rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0xA;            //shr rax, 0x0A
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x14;           //shr rax, 0x14
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x28;           //shr rax, 0x28
        rdx ^= rax;             //xor rdx, rax
        rdx += r11;             //add rdx, r11
        rax = r11;              //mov rax, r11
        rax ^= rcx;             //xor rax, rcx
        rdx -= rax;             //sub rdx, rax
        return rdx;
    }
    case 3:
    {
        r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D0F8);              //mov r10, [0x0000000007B6314D]
        rax = 0;                //and rax, 0xFFFFFFFFC0000000
        rax = rotl64(rax, 0x10);               //rol rax, 0x10
        rax ^= r10;             //xor rax, r10
        rax = ~rax;             //not rax
        rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x9);              //imul rdx, [rax+0x09]
        rax = 0x28984D4C9C081C9D;               //mov rax, 0x28984D4C9C081C9D
        rdx *= rax;             //imul rdx, rax
        rax = GameGlobals::module_base + 0x5DF1119C;              //lea rax, [0x000000005BAE6F8F]
        rax = ~rax;             //not rax
        rax -= r11;             //sub rax, r11
        rdx ^= rax;             //xor rdx, rax
        rax = 0x85D733B4D1593C82;               //mov rax, 0x85D733B4D1593C82
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0xF;            //shr rax, 0x0F
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x1E;           //shr rax, 0x1E
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x3C;           //shr rax, 0x3C
        rdx ^= rax;             //xor rdx, rax
        rax = r11;              //mov rax, r11
        rax = ~rax;             //not rax
        rdx ^= rax;             //xor rdx, rax
        rax = GameGlobals::module_base + 0x1F00BB30;              //lea rax, [0x000000001CBE17EB]
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x9;            //shr rax, 0x09
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x12;           //shr rax, 0x12
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x24;           //shr rax, 0x24
        rdx ^= rax;             //xor rdx, rax
        rax = 0x674411AA461BD822;               //mov rax, 0x674411AA461BD822
        rdx += rax;             //add rdx, rax
        return rdx;
    }
    case 4:
    {
        r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D0F8);              //mov r10, [0x0000000007B62C25]
        rax = 0;                //and rax, 0xFFFFFFFFC0000000
        rax = rotl64(rax, 0x10);               //rol rax, 0x10
        rax ^= r10;             //xor rax, r10
        rax = ~rax;             //not rax
        rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x9);              //imul rdx, [rax+0x09]
        rax = 0xF1BFE64DAFFE87EF;               //mov rax, 0xF1BFE64DAFFE87EF
        rdx *= rax;             //imul rdx, rax
        rdx -= r11;             //sub rdx, r11
        rax = 0xFFFFFFFFFFFF6A4E;               //mov rax, 0xFFFFFFFFFFFF6A4E
        rax -= r11;             //sub rax, r11
        rax -= GameGlobals::module_base;          //sub rax, [rbp-0x30] -- didn't find trace -> use base
        rdx += rax;             //add rdx, rax
        rax = 0xE4E9D2229EAB4DBF;               //mov rax, 0xE4E9D2229EAB4DBF
        rdx *= rax;             //imul rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0xA;            //shr rax, 0x0A
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x14;           //shr rax, 0x14
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x28;           //shr rax, 0x28
        rdx ^= rax;             //xor rdx, rax
        rax = 0x50E05FCA285FA110;               //mov rax, 0x50E05FCA285FA110
        rdx += rax;             //add rdx, rax
        rax = GameGlobals::module_base;           //lea rax, [0xFFFFFFFFFDBD5967]
        rax += 0x3377F8A0;              //add rax, 0x3377F8A0
        rax += r11;             //add rax, r11
        rdx ^= rax;             //xor rdx, rax
        return rdx;
    }
    case 5:
    {
        r13 = GameGlobals::module_base + 0x70F9514E;              //lea r13, [0x000000006EB6A892]
        r9 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D0F8);               //mov r9, [0x0000000007B627E9]
        rax = GameGlobals::module_base;           //lea rax, [0xFFFFFFFFFDBD54B8]
        rdx += rax;             //add rdx, rax
        rax = r11;              //mov rax, r11
        rax ^= r13;             //xor rax, r13
        rdx += rax;             //add rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x26;           //shr rax, 0x26
        rdx ^= rax;             //xor rdx, rax
        rax = 0xE4484A1C417292DD;               //mov rax, 0xE4484A1C417292DD
        rdx ^= rax;             //xor rdx, rax
        rax = 0;                //and rax, 0xFFFFFFFFC0000000
        rax = rotl64(rax, 0x10);               //rol rax, 0x10
        rax ^= r9;              //xor rax, r9
        rax = ~rax;             //not rax
        rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x9);              //imul rdx, [rax+0x09]
        rax = 0xD36EF0EB7300989B;               //mov rax, 0xD36EF0EB7300989B
        rdx *= rax;             //imul rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x1A;           //shr rax, 0x1A
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x34;           //shr rax, 0x34
        rdx ^= rax;             //xor rdx, rax
        rax = 0x30B43F4CE9BD88E3;               //mov rax, 0x30B43F4CE9BD88E3
        rdx ^= rax;             //xor rdx, rax
        return rdx;
    }
    case 6:
    {
        r13 = GameGlobals::module_base + 0xC561;          //lea r13, [0xFFFFFFFFFDBE1801]
        r9 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D0F8);               //mov r9, [0x0000000007B6234A]
        rax = GameGlobals::module_base + 0x48D32CC2;              //lea rax, [0x0000000046907D61]
        rax -= r11;             //sub rax, r11
        rdx += rax;             //add rdx, rax
        rax = 0;                //and rax, 0xFFFFFFFFC0000000
        rax = rotl64(rax, 0x10);               //rol rax, 0x10
        rax ^= r9;              //xor rax, r9
        rax = ~rax;             //not rax
        rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x9);              //imul rdx, [rax+0x09]
        rdx ^= r11;             //xor rdx, r11
        rdx ^= r13;             //xor rdx, r13
        rax = 0x6D79589F1C7242EA;               //mov rax, 0x6D79589F1C7242EA
        rdx ^= rax;             //xor rdx, rax
        rax = 0x3EDD229AD3FC630F;               //mov rax, 0x3EDD229AD3FC630F
        rdx *= rax;             //imul rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x11;           //shr rax, 0x11
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x22;           //shr rax, 0x22
        rdx ^= rax;             //xor rdx, rax
        rdx ^= r11;             //xor rdx, r11
        rax = 0xFEC4FCF701BC6CA6;               //mov rax, 0xFEC4FCF701BC6CA6
        rdx ^= rax;             //xor rdx, rax
        return rdx;
    }
    case 7:
    {
        r9 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D0F8);               //mov r9, [0x0000000007B61E34]
        rax = 0xA2AB2B673BC3B75D;               //mov rax, 0xA2AB2B673BC3B75D
        rdx *= rax;             //imul rdx, rax
        rax = GameGlobals::module_base;           //lea rax, [0xFFFFFFFFFDBD4B5E]
        rdx += rax;             //add rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x25;           //shr rax, 0x25
        rdx ^= rax;             //xor rdx, rax
        rax = GameGlobals::module_base;           //lea rax, [0xFFFFFFFFFDBD4C08]
        rdx -= rax;             //sub rdx, rax
        rax = 0;                //and rax, 0xFFFFFFFFC0000000
        rax = rotl64(rax, 0x10);               //rol rax, 0x10
        rax ^= r9;              //xor rax, r9
        rax = ~rax;             //not rax
        rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x9);              //imul rdx, [rax+0x09]
        rax = 0x73626C97888D190C;               //mov rax, 0x73626C97888D190C
        rdx -= rax;             //sub rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x21;           //shr rax, 0x21
        rdx ^= rax;             //xor rdx, rax
        rdx -= r11;             //sub rdx, r11
        return rdx;
    }
    case 8:
    {
        r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D0F8);              //mov r10, [0x0000000007B619B2]
        r13 = GameGlobals::module_base + 0x3C254912;              //lea r13, [0x0000000039E291B4]
        rax = 0x811B5B1F21D90EC7;               //mov rax, 0x811B5B1F21D90EC7
        rdx *= rax;             //imul rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x16;           //shr rax, 0x16
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x2C;           //shr rax, 0x2C
        rdx ^= rax;             //xor rdx, rax
        rdx ^= r11;             //xor rdx, r11
        rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
        rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
        rcx ^= r10;             //xor rcx, r10
        rcx = ~rcx;             //not rcx
        rdx *= k_memory.ReadNb<uintptr_t>(rcx + 0x9);              //imul rdx, [rcx+0x09]
        rax = GameGlobals::module_base;           //lea rax, [0xFFFFFFFFFDBD4515]
        rax += 0x3347DCA4;              //add rax, 0x3347DCA4
        rax += r11;             //add rax, r11
        rdx += rax;             //add rdx, rax
        rax = 0xE47021E0D30D5A4A;               //mov rax, 0xE47021E0D30D5A4A
        rdx ^= rax;             //xor rdx, rax
        rax = r13;              //mov rax, r13
        rax -= r11;             //sub rax, r11
        rdx ^= rax;             //xor rdx, rax
        rax = 0x334DB6514E4F48F4;               //mov rax, 0x334DB6514E4F48F4
        rdx += rax;             //add rdx, rax
        return rdx;
    }
    case 9:
    {
        r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D0F8);              //mov r10, [0x0000000007B6155C]
        rcx = GameGlobals::module_base + 0x491E12F3;              //lea rcx, [0x0000000046DB553C]
        rcx = ~rcx;             //not rcx
        rcx ^= r11;             //xor rcx, r11
        rax = 0xB4D57585A0A1682C;               //mov rax, 0xB4D57585A0A1682C
        rdx += rax;             //add rdx, rax
        rdx += rcx;             //add rdx, rcx
        rcx = GameGlobals::module_base;           //lea rcx, [0xFFFFFFFFFDBD42E4]
        rcx += 0x2A8D12FF;              //add rcx, 0x2A8D12FF
        rcx += r11;             //add rcx, r11
        rcx ^= rdx;             //xor rcx, rdx
        rdx = GameGlobals::module_base + 0x40540190;              //lea rdx, [0x000000003E114453]
        rdx = ~rdx;             //not rdx
        rdx ^= r11;             //xor rdx, r11
        rdx += rcx;             //add rdx, rcx
        r13 = 0x6A008CE4E907D185;               //mov r13, 0x6A008CE4E907D185
        rdx += r13;             //add rdx, r13
        rax = 0;                //and rax, 0xFFFFFFFFC0000000
        rax = rotl64(rax, 0x10);               //rol rax, 0x10
        rax ^= r10;             //xor rax, r10
        rax = ~rax;             //not rax
        rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x9);              //imul rdx, [rax+0x09]
        rax = rdx;              //mov rax, rdx
        rax >>= 0x1B;           //shr rax, 0x1B
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x36;           //shr rax, 0x36
        rdx ^= rax;             //xor rdx, rax
        rax = 0x6D6C2C342FB9F435;               //mov rax, 0x6D6C2C342FB9F435
        rdx *= rax;             //imul rdx, rax
        return rdx;
    }
    case 10:
    {
        r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D0F8);              //mov r10, [0x0000000007B610A6]
        r13 = GameGlobals::module_base + 0x6265F364;              //lea r13, [0x00000000602332FF]
        rax = 0x64A411E2189B152D;               //mov rax, 0x64A411E2189B152D
        rdx -= rax;             //sub rdx, rax
        rdx -= r11;             //sub rdx, r11
        rax = GameGlobals::module_base;           //lea rax, [0xFFFFFFFFFDBD3C17]
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x23;           //shr rax, 0x23
        rdx ^= rax;             //xor rdx, rax
        rdx += r11;             //add rdx, r11
        rax = r13;              //mov rax, r13
        rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
        rax = ~rax;             //not rax
        rax *= r11;             //imul rax, r11
        rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
        rdx ^= rax;             //xor rdx, rax
        rcx ^= r10;             //xor rcx, r10
        rcx = ~rcx;             //not rcx
        rdx *= k_memory.ReadNb<uintptr_t>(rcx + 0x9);              //imul rdx, [rcx+0x09]
        rax = 0x187115F18D64F3C7;               //mov rax, 0x187115F18D64F3C7
        rdx *= rax;             //imul rdx, rax
        return rdx;
    }
    case 11:
    {
        r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D0F8);              //mov r10, [0x0000000007B60CE9]
        rax = 0x32D3AE43F3324FCB;               //mov rax, 0x32D3AE43F3324FCB
        rdx *= rax;             //imul rdx, rax
        rax = 0;                //and rax, 0xFFFFFFFFC0000000
        rax = rotl64(rax, 0x10);               //rol rax, 0x10
        rax ^= r10;             //xor rax, r10
        rax = ~rax;             //not rax
        rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x9);              //imul rdx, [rax+0x09]
        rax = 0xCDA8F30157E3D24A;               //mov rax, 0xCDA8F30157E3D24A
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x13;           //shr rax, 0x13
        rdx ^= rax;             //xor rdx, rax
        rcx = rdx;              //mov rcx, rdx
        rcx >>= 0x26;           //shr rcx, 0x26
        rcx ^= rdx;             //xor rcx, rdx
        rdx = r11;              //mov rdx, r11
        rax = GameGlobals::module_base;           //lea rax, [0xFFFFFFFFFDBD37E4]
        rdx = ~rdx;             //not rdx
        rdx += rcx;             //add rdx, rcx
        rdx -= rax;             //sub rdx, rax
        rdx -= 0xC88E;          //sub rdx, 0xC88E
        rax = GameGlobals::module_base + 0x36D6;          //lea rax, [0xFFFFFFFFFDBD6FF9]
        rax = ~rax;             //not rax
        rax -= r11;             //sub rax, r11
        rdx ^= rax;             //xor rdx, rax
        rax = 0xDC621CFA0892045F;               //mov rax, 0xDC621CFA0892045F
        rdx *= rax;             //imul rdx, rax
        rdx ^= r11;             //xor rdx, r11
        return rdx;
    }
    case 12:
    {
        r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D0F8);              //mov r10, [0x0000000007B60832]
        rax = GameGlobals::module_base;           //lea rax, [0xFFFFFFFFFDBD3654]
        rdx -= rax;             //sub rdx, rax
        rdx -= r11;             //sub rdx, r11
        rax = 0;                //and rax, 0xFFFFFFFFC0000000
        rax = rotl64(rax, 0x10);               //rol rax, 0x10
        rax ^= r10;             //xor rax, r10
        rax = ~rax;             //not rax
        rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x9);              //imul rdx, [rax+0x09]
        rax = GameGlobals::module_base + 0x1A2D02AD;              //lea rax, [0x0000000017EA3731]
        rax += r11;             //add rax, r11
        rdx ^= rax;             //xor rdx, rax
        rax = 0x599A096D5E0BAB36;               //mov rax, 0x599A096D5E0BAB36
        rdx -= rax;             //sub rdx, rax
        rcx = r11;              //mov rcx, r11
        rcx = ~rcx;             //not rcx
        rax = GameGlobals::module_base + 0x6897;          //lea rax, [0xFFFFFFFFFDBD9DF7]
        rdx += rax;             //add rdx, rax
        rdx += rcx;             //add rdx, rcx
        rax = 0x1FBF7293109CBA8F;               //mov rax, 0x1FBF7293109CBA8F
        rdx *= rax;             //imul rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0xC;            //shr rax, 0x0C
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x18;           //shr rax, 0x18
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x30;           //shr rax, 0x30
        rdx ^= rax;             //xor rdx, rax
        return rdx;
    }
    case 13:
    {
        r9 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D0F8);               //mov r9, [0x0000000007B602EB]
        rax = rdx;              //mov rax, rdx
        rax >>= 0x25;           //shr rax, 0x25
        rdx ^= rax;             //xor rdx, rax
        rax = GameGlobals::module_base + 0xB1E0;          //lea rax, [0xFFFFFFFFFDBDE35E]
        rax *= r11;             //imul rax, r11
        rdx += rax;             //add rdx, rax
        rax = GameGlobals::module_base;           //lea rax, [0xFFFFFFFFFDBD2E59]
        rdx += rax;             //add rdx, rax
        rax = 0x363A26220DA9C56F;               //mov rax, 0x363A26220DA9C56F
        rdx *= rax;             //imul rdx, rax
        rax = r11;              //mov rax, r11
        uintptr_t RSP_0xFFFFFFFFFFFFFFC0;
        RSP_0xFFFFFFFFFFFFFFC0 = 0x808EE7EEF6C6B5B9;            //mov rax, 0x808EE7EEF6C6B5B9 : RBP+0xFFFFFFFFFFFFFFC0
        rax *= RSP_0xFFFFFFFFFFFFFFC0;          //imul rax, [rbp-0x40]
        rdx += rax;             //add rdx, rax
        rax = 0;                //and rax, 0xFFFFFFFFC0000000
        rax = rotl64(rax, 0x10);               //rol rax, 0x10
        rax ^= r9;              //xor rax, r9
        rax = ~rax;             //not rax
        rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x9);              //imul rdx, [rax+0x09]
        rax = 0x14555FE768695E77;               //mov rax, 0x14555FE768695E77
        rdx *= rax;             //imul rdx, rax
        return rdx;
    }
    case 14:
    {
        r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D0F8);              //mov r10, [0x0000000007B5FE2B]
        rax = 0x4C930750EBF2D3CD;               //mov rax, 0x4C930750EBF2D3CD
        rdx *= rax;             //imul rdx, rax
        rax = r11;              //mov rax, r11
        rax -= GameGlobals::module_base;          //sub rax, [rbp-0x30] -- didn't find trace -> use base
        rax += 0xFFFFFFFFFFFF4CB1;              //add rax, 0xFFFFFFFFFFFF4CB1
        rdx += rax;             //add rdx, rax
        rax = 0;                //and rax, 0xFFFFFFFFC0000000
        rax = rotl64(rax, 0x10);               //rol rax, 0x10
        rax ^= r10;             //xor rax, r10
        rax = ~rax;             //not rax
        rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x9);              //imul rdx, [rax+0x09]
        rax = r11;              //mov rax, r11
        rax -= GameGlobals::module_base;          //sub rax, [rbp-0x30] -- didn't find trace -> use base
        rax += 0xFFFFFFFFC75FBE0F;              //add rax, 0xFFFFFFFFC75FBE0F
        rdx += rax;             //add rdx, rax
        rax = 0x2190C1218FC010D6;               //mov rax, 0x2190C1218FC010D6
        rdx += rax;             //add rdx, rax
        rax = GameGlobals::module_base + 0x3289116D;              //lea rax, [0x0000000030463BB4]
        rax ^= r11;             //xor rax, r11
        rdx ^= rax;             //xor rdx, rax
        rax = 0xB284BC172DF02AE3;               //mov rax, 0xB284BC172DF02AE3
        rdx *= rax;             //imul rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x24;           //shr rax, 0x24
        rdx ^= rax;             //xor rdx, rax
        return rdx;
    }
    case 15:
    {
        r9 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D0F8);               //mov r9, [0x0000000007B5F9E6]
        rax = rdx;              //mov rax, rdx
        rax >>= 0x22;           //shr rax, 0x22
        rdx ^= rax;             //xor rdx, rax
        rax = 0x641C31DFE01E2261;               //mov rax, 0x641C31DFE01E2261
        rdx *= rax;             //imul rdx, rax
        rdx ^= r11;             //xor rdx, r11
        rax = 0x555A8E40DA73B53A;               //mov rax, 0x555A8E40DA73B53A
        rdx ^= rax;             //xor rdx, rax
        rax = rdx;              //mov rax, rdx
        rax >>= 0x25;           //shr rax, 0x25
        rdx ^= rax;             //xor rdx, rax
        rax = 0;                //and rax, 0xFFFFFFFFC0000000
        rax = rotl64(rax, 0x10);               //rol rax, 0x10
        rax ^= r9;              //xor rax, r9
        rax = ~rax;             //not rax
        rdx *= k_memory.ReadNb<uintptr_t>(rax + 0x9);              //imul rdx, [rax+0x09]
        rax = GameGlobals::module_base + 0x3C1A;          //lea rax, [0xFFFFFFFFFDBD6244]
        rax ^= r11;             //xor rax, r11
        rdx -= rax;             //sub rdx, rax
        rax = 0xA625682AA87A90F9;               //mov rax, 0xA625682AA87A90F9
        rdx *= rax;             //imul rdx, rax
        return rdx;
    }
    }
}
uintptr_t GameDecrypts::decrypt_bone_base()
{
    const uint64_t mb = GameGlobals::module_base;
    uint64_t rax = mb, rbx = mb, rcx = mb, rdx = mb, rdi = mb, rsi = mb, r8 = mb, r9 = mb, r10 = mb, r11 = mb, r12 = mb, r13 = mb, r14 = mb, r15 = mb;
    rax = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0xD94B9F8);
    if (!rax)
        return rax;
    rbx = ~GameGlobals::process_peb;              //mov rbx, gs:[rcx]
    rcx = rbx;              //mov rcx, rbx
    rcx = 0x0//_rotr64(rcx, 0x17);               //ror rcx, 0x17
    rcx &= 0xF;
    switch (rcx) {
    case 0:
    {
        r13 = GameGlobals::module_base + 0x52C1DDD8;              //lea r13, [0x0000000050AFBE67]
        r12 = GameGlobals::module_base + 0x16C3B815;              //lea r12, [0x0000000014B19895]
        r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D231);              //mov r10, [0x0000000007E6B256]
        rdx = rbx;              //mov rdx, rbx
        rdx = ~rdx;             //not rdx
        rcx = r13;              //mov rcx, r13
        rcx = ~rcx;             //not rcx
        rcx ^= rdx;             //xor rcx, rdx
        rcx ^= rax;             //xor rcx, rax
        rax = GameGlobals::module_base + 0x6997CC2D;              //lea rax, [0x000000006785A9C0]
        rdx ^= rax;             //xor rdx, rax
        rax = r12;              //mov rax, r12
        rdx += rbx;             //add rdx, rbx
        rax = ~rax;             //not rax
        rdx += rcx;             //add rdx, rcx
        rax += rdx;             //add rax, rdx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x5;            //shr rcx, 0x05
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0xA;            //shr rcx, 0x0A
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x14;           //shr rcx, 0x14
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x28;           //shr rcx, 0x28
        rax ^= rcx;             //xor rax, rcx
        rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
        rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
        rcx ^= r10;             //xor rcx, r10
        rcx = __bswap_64(rcx);            //bswap rcx
        rax *= k_memory.ReadNb<uintptr_t>(rcx + 0xf);              //imul rax, [rcx+0x0F]
        rcx = 0x835BFBE2D10CB345;               //mov rcx, 0x835BFBE2D10CB345
        rax *= rcx;             //imul rax, rcx
        rcx = 0x4F9BF1A84D23C337;               //mov rcx, 0x4F9BF1A84D23C337
        rax ^= rcx;             //xor rax, rcx
        rcx = 0x568CB7F53AC8E982;               //mov rcx, 0x568CB7F53AC8E982
        rax -= rcx;             //sub rax, rcx
        return rax;
    }
    case 1:
    {
        r12 = GameGlobals::module_base + 0x27AB;          //lea r12, [0xFFFFFFFFFDEE0343]
        r11 = GameGlobals::module_base + 0x1A54EB9B;              //lea r11, [0x000000001842C724]
        r9 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D231);               //mov r9, [0x0000000007E6AD27]
        rcx = 0xE9A4FD52556D5389;               //mov rcx, 0xE9A4FD52556D5389
        rax ^= rcx;             //xor rax, rcx
        uintptr_t RSP_0x70;
        RSP_0x70 = 0x3928A67D3DA98E7B;          //mov rcx, 0x3928A67D3DA98E7B : RSP+0x70
        rax ^= RSP_0x70;                //xor rax, [rsp+0x70]
        rcx = r12;              //mov rcx, r12
        rcx ^= rbx;             //xor rcx, rbx
        rax -= rcx;             //sub rax, rcx
        rcx = r11;              //mov rcx, r11
        rcx ^= rbx;             //xor rcx, rbx
        rax += rcx;             //add rax, rcx
        rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
        rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
        rcx ^= r9;              //xor rcx, r9
        rcx = __bswap_64(rcx);            //bswap rcx
        rax *= k_memory.ReadNb<uintptr_t>(rcx + 0xf);              //imul rax, [rcx+0x0F]
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x14;           //shr rcx, 0x14
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x28;           //shr rcx, 0x28
        rax ^= rcx;             //xor rax, rcx
        rcx = 0xA6D60CC4598E6717;               //mov rcx, 0xA6D60CC4598E6717
        rax *= rcx;             //imul rax, rcx
        rax -= rbx;             //sub rax, rbx
        return rax;
    }
    case 2:
    {
        r11 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D231);              //mov r11, [0x0000000007E6A7EF]
        r8 = 0;                 //and r8, 0xFFFFFFFFC0000000
        rcx = GameGlobals::module_base + 0x3826FFC6;              //lea rcx, [0x000000003614D158]
        r8 = rotl64(r8, 0x10);                 //rol r8, 0x10
        rdx = rbx;              //mov rdx, rbx
        r8 ^= r11;              //xor r8, r11
        rdx = ~rdx;             //not rdx
        rdx *= rcx;             //imul rdx, rcx
        rcx = rax;              //mov rcx, rax
        rax = 0xEE10C122A54DF805;               //mov rax, 0xEE10C122A54DF805
        rax *= rcx;             //imul rax, rcx
        r8 = __bswap_64(r8);              //bswap r8
        rax += rdx;             //add rax, rdx
        rax *= k_memory.ReadNb<uintptr_t>(r8 + 0xf);               //imul rax, [r8+0x0F]
        rax += rbx;             //add rax, rbx
        rcx = GameGlobals::module_base + 0x5AB5;          //lea rcx, [0xFFFFFFFFFDEE2C7B]
        rcx = ~rcx;             //not rcx
        rcx *= rbx;             //imul rcx, rbx
        rax += rcx;             //add rax, rcx
        rcx = rbx;              //mov rcx, rbx
        rcx = ~rcx;             //not rcx
        uintptr_t RSP_0x70;
        RSP_0x70 = GameGlobals::module_base + 0x5484E494;                 //lea rcx, [0x000000005272BAB1] : RSP+0x70
        rcx += RSP_0x70;                //add rcx, [rsp+0x70]
        rax ^= rcx;             //xor rax, rcx
        rcx = 0x7F6F9B0EDA6F5A70;               //mov rcx, 0x7F6F9B0EDA6F5A70
        rax -= rcx;             //sub rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0xF;            //shr rcx, 0x0F
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x1E;           //shr rcx, 0x1E
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x3C;           //shr rcx, 0x3C
        rax ^= rcx;             //xor rax, rcx
        return rax;
    }
    case 3:
    {
        r12 = GameGlobals::module_base + 0xF7F6;          //lea r12, [0xFFFFFFFFFDEEC92F]
        r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D231);              //mov r10, [0x0000000007E6A2CF]
        rdx = r12;              //mov rdx, r12
        rdx = ~rdx;             //not rdx
        rcx = rbx;              //mov rcx, rbx
        rcx = ~rcx;             //not rcx
        rdx *= rcx;             //imul rdx, rcx
        rcx = rax;              //mov rcx, rax
        rax = 0xD56824600D86D151;               //mov rax, 0xD56824600D86D151
        rax *= rcx;             //imul rax, rcx
        rax += rdx;             //add rax, rdx
        rax -= rbx;             //sub rax, rbx
        rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
        rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
        rcx ^= r10;             //xor rcx, r10
        rcx = __bswap_64(rcx);            //bswap rcx
        rax *= k_memory.ReadNb<uintptr_t>(rcx + 0xf);              //imul rax, [rcx+0x0F]
        rax += rbx;             //add rax, rbx
        rcx = 0x1C77E11806051E91;               //mov rcx, 0x1C77E11806051E91
        rax *= rcx;             //imul rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x18;           //shr rcx, 0x18
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x30;           //shr rcx, 0x30
        rax ^= rcx;             //xor rax, rcx
        rcx = 0x122A28FE2BC7A38D;               //mov rcx, 0x122A28FE2BC7A38D
        rax += rcx;             //add rax, rcx
        return rax;
    }
    case 4:
    {
        r12 = GameGlobals::module_base + 0xD21B;          //lea r12, [0xFFFFFFFFFDEE9F41]
        r9 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D231);               //mov r9, [0x0000000007E69EA9]
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x25;           //shr rcx, 0x25
        rax ^= rcx;             //xor rax, rcx
        rcx = 0x4B8F762063239207;               //mov rcx, 0x4B8F762063239207
        rax *= rcx;             //imul rax, rcx
        rcx = 0x771D6822F6CA5AC8;               //mov rcx, 0x771D6822F6CA5AC8
        rax -= rcx;             //sub rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x1C;           //shr rcx, 0x1C
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x38;           //shr rcx, 0x38
        rax ^= rcx;             //xor rax, rcx
        rcx = rbx;              //mov rcx, rbx
        rcx = ~rcx;             //not rcx
        rcx ^= r12;             //xor rcx, r12
        rax ^= rcx;             //xor rax, rcx
        rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
        rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
        rcx ^= r9;              //xor rcx, r9
        rcx = __bswap_64(rcx);            //bswap rcx
        rcx = k_memory.ReadNb<uintptr_t>(rcx + 0xf);               //mov rcx, [rcx+0x0F]
        uintptr_t RSP_0x70;
        RSP_0x70 = 0x99A933A3C8CC6BE5;          //mov rcx, 0x99A933A3C8CC6BE5 : RSP+0x70
        rcx *= RSP_0x70;                //imul rcx, [rsp+0x70]
        rax *= rcx;             //imul rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x27;           //shr rcx, 0x27
        rax ^= rcx;             //xor rax, rcx
        return rax;
    }
    case 5:
    {
        r13 = GameGlobals::module_base + 0x3687;          //lea r13, [0xFFFFFFFFFDEDFE62]
        r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D231);              //mov r10, [0x0000000007E699B2]
        rdx = GameGlobals::module_base + 0xD3AB;          //lea rdx, [0xFFFFFFFFFDEE9AE9]
        rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
        rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
        rcx ^= r10;             //xor rcx, r10
        rcx = __bswap_64(rcx);            //bswap rcx
        rax *= k_memory.ReadNb<uintptr_t>(rcx + 0xf);              //imul rax, [rcx+0x0F]
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x2;            //shr rcx, 0x02
        rax ^= rcx;             //xor rax, rcx
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
        rcx ^= rax;             //xor rcx, rax
        rax = rdx;              //mov rax, rdx
        rax = ~rax;             //not rax
        rcx += rbx;             //add rcx, rbx
        rax += rcx;             //add rax, rcx
        rcx = 0xA879BD0E7292EA61;               //mov rcx, 0xA879BD0E7292EA61
        rax *= rcx;             //imul rax, rcx
        rax += r13;             //add rax, r13
        rdx = rbx;              //mov rdx, rbx
        rdx = ~rdx;             //not rdx
        rax += rdx;             //add rax, rdx
        rcx = 0x3DB099443BECCD4B;               //mov rcx, 0x3DB099443BECCD4B
        rax *= rcx;             //imul rax, rcx
        rcx = 0xB8C3FA1BAD2EF567;               //mov rcx, 0xB8C3FA1BAD2EF567
        rax ^= rcx;             //xor rax, rcx
        return rax;
    }
    case 6:
    {
        r12 = GameGlobals::module_base + 0x2B7C6109;              //lea r12, [0x00000000296A237A]
        r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D231);              //mov r10, [0x0000000007E69444]
        rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
        rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
        rcx ^= r10;             //xor rcx, r10
        rcx = __bswap_64(rcx);            //bswap rcx
        rax *= k_memory.ReadNb<uintptr_t>(rcx + 0xf);              //imul rax, [rcx+0x0F]
        rcx = 0xFB211AF637372C23;               //mov rcx, 0xFB211AF637372C23
        rax *= rcx;             //imul rax, rcx
        rcx = 0xA2B1CA440F9F5ABD;               //mov rcx, 0xA2B1CA440F9F5ABD
        rax ^= rcx;             //xor rax, rcx
        rcx = r12;              //mov rcx, r12
        rcx = ~rcx;             //not rcx
        rcx *= rbx;             //imul rcx, rbx
        rax += rcx;             //add rax, rcx
        rcx = 0x6C059F54E58DEF0A;               //mov rcx, 0x6C059F54E58DEF0A
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x5;            //shr rcx, 0x05
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0xA;            //shr rcx, 0x0A
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x14;           //shr rcx, 0x14
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x28;           //shr rcx, 0x28
        rax ^= rcx;             //xor rax, rcx
        rcx = GameGlobals::module_base + 0xAD51;          //lea rcx, [0xFFFFFFFFFDEE6AE1]
        rax -= rcx;             //sub rax, rcx
        rax += rbx;             //add rax, rbx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x24;           //shr rcx, 0x24
        rax ^= rcx;             //xor rax, rcx
        return rax;
    }
    case 7:
    {
        r12 = GameGlobals::module_base + 0x82B9;          //lea r12, [0xFFFFFFFFFDEE3FA0]
        r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D231);              //mov r10, [0x0000000007E68EBE]
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x1E;           //shr rcx, 0x1E
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x3C;           //shr rcx, 0x3C
        rax ^= rcx;             //xor rax, rcx
        rcx = 0x64AAC8482D478483;               //mov rcx, 0x64AAC8482D478483
        rax += rcx;             //add rax, rcx
        rcx = 0x95B14EB67A41FB2B;               //mov rcx, 0x95B14EB67A41FB2B
        rax ^= rcx;             //xor rax, rcx
        rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
        rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
        rcx ^= r10;             //xor rcx, r10
        rcx = __bswap_64(rcx);            //bswap rcx
        rax *= k_memory.ReadNb<uintptr_t>(rcx + 0xf);              //imul rax, [rcx+0x0F]
        rdx = r12;              //mov rdx, r12
        rdx = ~rdx;             //not rdx
        rdx *= rbx;             //imul rdx, rbx
        rcx = GameGlobals::module_base + 0x386638DF;              //lea rcx, [0x000000003653F3E6]
        rcx += rbx;             //add rcx, rbx
        rdx ^= rcx;             //xor rdx, rcx
        rax ^= rdx;             //xor rax, rdx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x1F;           //shr rcx, 0x1F
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x3E;           //shr rcx, 0x3E
        rax ^= rcx;             //xor rax, rcx
        rcx = 0x76F15788D38403BB;               //mov rcx, 0x76F15788D38403BB
        rax *= rcx;             //imul rax, rcx
        return rax;
    }
    case 8:
    {
        r12 = GameGlobals::module_base + 0xB3E3;          //lea r12, [0xFFFFFFFFFDEE6B71]
        r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D231);              //mov r10, [0x0000000007E68911]
        rax += r12;             //add rax, r12
        rax += rbx;             //add rax, rbx
        rcx = GameGlobals::module_base;           //lea rcx, [0xFFFFFFFFFDEDB58B]
        rax ^= rcx;             //xor rax, rcx
        rcx = 0xD7750AE6E9ABB7F7;               //mov rcx, 0xD7750AE6E9ABB7F7
        rax ^= rcx;             //xor rax, rcx
        rcx = 0x3CF2AB5FD00F9F1B;               //mov rcx, 0x3CF2AB5FD00F9F1B
        rax *= rcx;             //imul rax, rcx
        rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
        rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
        rcx ^= r10;             //xor rcx, r10
        rcx = __bswap_64(rcx);            //bswap rcx
        rax *= k_memory.ReadNb<uintptr_t>(rcx + 0xf);              //imul rax, [rcx+0x0F]
        rcx = rax;              //mov rcx, rax
        rcx >>= 0xB;            //shr rcx, 0x0B
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x16;           //shr rcx, 0x16
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x2C;           //shr rcx, 0x2C
        rax ^= rcx;             //xor rax, rcx
        rcx = 0xBEA9C8CB10FBC733;               //mov rcx, 0xBEA9C8CB10FBC733
        rax *= rcx;             //imul rax, rcx
        rax ^= rbx;             //xor rax, rbx
        return rax;
    }
    case 9:
    {
        r12 = GameGlobals::module_base + 0x1F78;          //lea r12, [0xFFFFFFFFFDEDD1EF]
        r9 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D231);               //mov r9, [0x0000000007E68420]
        rcx = GameGlobals::module_base;           //lea rcx, [0xFFFFFFFFFDEDAEFF]
        rax += rcx;             //add rax, rcx
        rax ^= rcx;             //xor rax, rcx
        rcx = 0xB1CDD184B3D55CBB;               //mov rcx, 0xB1CDD184B3D55CBB
        rax *= rcx;             //imul rax, rcx
        rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
        rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
        rcx ^= r9;              //xor rcx, r9
        rcx = __bswap_64(rcx);            //bswap rcx
        rax *= k_memory.ReadNb<uintptr_t>(rcx + 0xf);              //imul rax, [rcx+0x0F]
        rax ^= r12;             //xor rax, r12
        rax ^= rbx;             //xor rax, rbx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x19;           //shr rcx, 0x19
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x32;           //shr rcx, 0x32
        rax ^= rcx;             //xor rax, rcx
        rcx = 0x603AD918C7651D61;               //mov rcx, 0x603AD918C7651D61
        rax *= rcx;             //imul rax, rcx
        rcx = 0x43C2795B19DE41B2;               //mov rcx, 0x43C2795B19DE41B2
        rax ^= rcx;             //xor rax, rcx
        return rax;
    }
    case 10:
    {
        rdx = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D231);              //mov rdx, [0x0000000007E67FBE]
        r10 = GameGlobals::module_base;           //lea r10, [0xFFFFFFFFFDEDABB5]
        rcx = rbx;              //mov rcx, rbx
        rcx -= r10;             //sub rcx, r10
        rcx += 0xFFFFFFFF8A9AB6E4;              //add rcx, 0xFFFFFFFF8A9AB6E4
        rax += rcx;             //add rax, rcx
        rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
        rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
        rcx ^= rdx;             //xor rcx, rdx
        rcx = __bswap_64(rcx);            //bswap rcx
        rax *= k_memory.ReadNb<uintptr_t>(rcx + 0xf);              //imul rax, [rcx+0x0F]
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x10;           //shr rcx, 0x10
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x20;           //shr rcx, 0x20
        rax ^= rcx;             //xor rax, rcx
        rcx = 0xC84CCBF80B87E219;               //mov rcx, 0xC84CCBF80B87E219
        rax -= rbx;             //sub rax, rbx
        rax *= rcx;             //imul rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x17;           //shr rcx, 0x17
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x2E;           //shr rcx, 0x2E
        rax ^= rcx;             //xor rax, rcx
        rcx = GameGlobals::module_base;           //lea rcx, [0xFFFFFFFFFDEDABF7]
        rax += rcx;             //add rax, rcx
        return rax;
    }
    case 11:
    {
        r10 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D231);              //mov r10, [0x0000000007E67BB0]
        r13 = 0x64663144BA8008FF;               //mov r13, 0x64663144BA8008FF
        rax += r13;             //add rax, r13
        rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
        rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
        rcx ^= r10;             //xor rcx, r10
        rcx = __bswap_64(rcx);            //bswap rcx
        rax *= k_memory.ReadNb<uintptr_t>(rcx + 0xf);              //imul rax, [rcx+0x0F]
        rdx = GameGlobals::module_base + 0x556BEEFB;              //lea rdx, [0x00000000535997C2]
        rcx = rbx;              //mov rcx, rbx
        rdx = ~rdx;             //not rdx
        rdx ^= rbx;             //xor rdx, rbx
        rcx -= rdx;             //sub rcx, rdx
        rax += rcx;             //add rax, rcx
        rcx = GameGlobals::module_base;           //lea rcx, [0xFFFFFFFFFDEDA628]
        rax -= rcx;             //sub rax, rcx
        r11 = GameGlobals::module_base;           //lea r11, [0xFFFFFFFFFDEDA761]
        rcx = rbx;              //mov rcx, rbx
        rcx -= r11;             //sub rcx, r11
        rcx -= 0x56B4CD57;              //sub rcx, 0x56B4CD57
        rax ^= rcx;             //xor rax, rcx
        rcx = 0x7070109855058503;               //mov rcx, 0x7070109855058503
        rax *= rcx;             //imul rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0xF;            //shr rcx, 0x0F
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x1E;           //shr rcx, 0x1E
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x3C;           //shr rcx, 0x3C
        rax ^= rcx;             //xor rax, rcx
        return rax;
    }
    case 12:
    {
        r12 = GameGlobals::module_base + 0xE579;          //lea r12, [0xFFFFFFFFFDEE8A37]
        r9 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D231);               //mov r9, [0x0000000007E67691]
        rax -= rbx;             //sub rax, rbx
        rcx = 0xA0EE35B6E68BCC83;               //mov rcx, 0xA0EE35B6E68BCC83
        rax *= rcx;             //imul rax, rcx
        rcx = 0x7D66A9567BE39BC8;               //mov rcx, 0x7D66A9567BE39BC8
        rax -= rcx;             //sub rax, rcx
        rcx = r12;              //mov rcx, r12
        rcx = ~rcx;             //not rcx
        rcx *= rbx;             //imul rcx, rbx
        rax += rcx;             //add rax, rcx
        r11 = GameGlobals::module_base;           //lea r11, [0xFFFFFFFFFDEDA168]
        rax -= r11;             //sub rax, r11
        rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
        rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
        rcx ^= r9;              //xor rcx, r9
        rcx = __bswap_64(rcx);            //bswap rcx
        rax *= k_memory.ReadNb<uintptr_t>(rcx + 0xf);              //imul rax, [rcx+0x0F]
        rcx = 0x3BB791C95F8DCF1;                //mov rcx, 0x3BB791C95F8DCF1
        rax += rcx;             //add rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x13;           //shr rcx, 0x13
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x26;           //shr rcx, 0x26
        rax ^= rcx;             //xor rax, rcx
        return rax;
    }
    case 13:
    {
        r11 = GameGlobals::module_base + 0x30EDECA2;              //lea r11, [0x000000002EDB8C55]
        r9 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D231);               //mov r9, [0x0000000007E67182]
        rax ^= rbx;             //xor rax, rbx
        rcx = 0x8B084B922A7798F9;               //mov rcx, 0x8B084B922A7798F9
        rax *= rcx;             //imul rax, rcx
        rcx = r11;              //mov rcx, r11
        rcx = ~rcx;             //not rcx
        rcx *= rbx;             //imul rcx, rbx
        rax ^= rcx;             //xor rax, rcx
        rax ^= rbx;             //xor rax, rbx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0xA;            //shr rcx, 0x0A
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x14;           //shr rcx, 0x14
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x28;           //shr rcx, 0x28
        rax ^= rcx;             //xor rax, rcx
        rcx = GameGlobals::module_base;           //lea rcx, [0xFFFFFFFFFDED9B99]
        rax ^= rcx;             //xor rax, rcx
        r10 = GameGlobals::module_base;           //lea r10, [0xFFFFFFFFFDED9DC5]
        rax += r10;             //add rax, r10
        rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
        rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
        rcx ^= r9;              //xor rcx, r9
        rcx = __bswap_64(rcx);            //bswap rcx
        rax *= k_memory.ReadNb<uintptr_t>(rcx + 0xf);              //imul rax, [rcx+0x0F]
        return rax;
    }
    case 14:
    {
        r9 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D231);               //mov r9, [0x0000000007E66BE3]
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x21;           //shr rcx, 0x21
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x10;           //shr rcx, 0x10
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x20;           //shr rcx, 0x20
        rax ^= rcx;             //xor rax, rcx
        rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
        rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
        rcx ^= r9;              //xor rcx, r9
        rcx = __bswap_64(rcx);            //bswap rcx
        rax *= k_memory.ReadNb<uintptr_t>(rcx + 0xf);              //imul rax, [rcx+0x0F]
        rcx = GameGlobals::module_base;           //lea rcx, [0xFFFFFFFFFDED96A4]
        rax += rcx;             //add rax, rcx
        rcx = 0x5A255F969234536B;               //mov rcx, 0x5A255F969234536B
        rax *= rcx;             //imul rax, rcx
        rcx = 0x556B7AC4C41BA176;               //mov rcx, 0x556B7AC4C41BA176
        rax += rcx;             //add rax, rcx
        rcx = 0xDB95CFA63A837B35;               //mov rcx, 0xDB95CFA63A837B35
        rax *= rcx;             //imul rax, rcx
        rcx = GameGlobals::module_base;           //lea rcx, [0xFFFFFFFFFDED991F]
        rax -= rcx;             //sub rax, rcx
        return rax;
    }
    case 15:
    {
        r9 = k_memory.ReadNb<uintptr_t>(GameGlobals::module_base + 0x9F8D231);               //mov r9, [0x0000000007E666E6]
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x13;           //shr rcx, 0x13
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x26;           //shr rcx, 0x26
        rax ^= rcx;             //xor rax, rcx
        rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
        rcx = rotl64(rcx, 0x10);               //rol rcx, 0x10
        rcx ^= r9;              //xor rcx, r9
        rcx = __bswap_64(rcx);            //bswap rcx
        rax *= k_memory.ReadNb<uintptr_t>(rcx + 0xf);              //imul rax, [rcx+0x0F]
        rcx = 0xC3662514DBA4DBF7;               //mov rcx, 0xC3662514DBA4DBF7
        rax *= rcx;             //imul rax, rcx
        rcx = GameGlobals::module_base;           //lea rcx, [0xFFFFFFFFFDED8FDA]
        rax -= rcx;             //sub rax, rcx
        rcx = 0x149A63B9809B97BA;               //mov rcx, 0x149A63B9809B97BA
        rax += rcx;             //add rax, rcx
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
        rcx = rax;              //mov rcx, rax
        rcx >>= 0xF;            //shr rcx, 0x0F
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x1E;           //shr rcx, 0x1E
        rax ^= rcx;             //xor rax, rcx
        rcx = rax;              //mov rcx, rax
        rcx >>= 0x3C;           //shr rcx, 0x3C
        rax ^= rcx;             //xor rax, rcx
        rax -= rbx;             //sub rax, rbx
        return rax;
    }
    }
}

uint64_t umul128(uint64_t a, uint64_t b, uint64_t *high) {
  __int128 result = (__int128) a * (__int128) b;
  *high = (uint64_t) (result >> 64);
  return (uint64_t) result;
}
uint16_t GameDecrypts::get_bone_index(uint32_t bone_index)
{
    const uint64_t mb = GameGlobals::module_base;
    uint64_t rax = mb, rbx = mb, rcx = mb, rdx = mb, rdi = mb, rsi = mb, r8 = mb, r9 = mb, r10 = mb, r11 = mb, r12 = mb, r13 = mb, r14 = mb, r15 = mb;
    rdi = bone_index;
    rcx = rdi * 0x13C8;
    rax = 0xA6AF61148FC004F5;               //mov rax, 0xA6AF61148FC004F5
    rax = umul128(rax, rcx, (uintptr_t*)&rdx);             //mul rcx
    rax = rcx;              //mov rax, rcx
    r11 = GameGlobals::module_base;           //lea r11, [0xFFFFFFFFFD90E3C5]
    rax -= rdx;             //sub rax, rdx
    r10 = 0x530C87C3FBF1E35F;               //mov r10, 0x530C87C3FBF1E35F
    rax >>= 0x1;            //shr rax, 0x01
    rax += rdx;             //add rax, rdx
    rax >>= 0xD;            //shr rax, 0x0D
    rax = rax * 0x26C3;             //imul rax, rax, 0x26C3
    rcx -= rax;             //sub rcx, rax
    rax = 0x93A280B63C96E0F3;               //mov rax, 0x93A280B63C96E0F3
    r8 = rcx * 0x26C3;              //imul r8, rcx, 0x26C3
    rax = umul128(rax, r8, (uintptr_t*)&rdx);              //mul r8
    rdx >>= 0xD;            //shr rdx, 0x0D
    rax = rdx * 0x377D;             //imul rax, rdx, 0x377D
    r8 -= rax;              //sub r8, rax
    rax = 0xBFA02FE80BFA02FF;               //mov rax, 0xBFA02FE80BFA02FF
    rax = umul128(rax, r8, (uintptr_t*)&rdx);              //mul r8
    rax = 0x90FDBC090FDBC091;               //mov rax, 0x90FDBC090FDBC091
    rdx >>= 0x7;            //shr rdx, 0x07
    rcx = rdx * 0xAB;               //imul rcx, rdx, 0xAB
    rax = umul128(rax, r8, (uintptr_t*)&rdx);              //mul r8
    rdx >>= 0x6;            //shr rdx, 0x06
    rcx += rdx;             //add rcx, rdx
    rax = rcx * 0xE2;               //imul rax, rcx, 0xE2
    rcx = r8 * 0xE4;                //imul rcx, r8, 0xE4
    rcx -= rax;             //sub rcx, rax
    rax = k_memory.ReadNb<uint16_t>(rcx + r11 * 1 + 0xA0466E0);                //movzx eax, word ptr [rcx+r11*1+0xA0466E0]
    r8 = rax * 0x13C8;              //imul r8, rax, 0x13C8
    rax = r10;              //mov rax, r10
    rax = umul128(rax, r8, (uintptr_t*)&rdx);              //mul r8
    rax = r10;              //mov rax, r10
    rdx >>= 0xB;            //shr rdx, 0x0B
    rcx = rdx * 0x18A9;             //imul rcx, rdx, 0x18A9
    r8 -= rcx;              //sub r8, rcx
    r9 = r8 * 0x1920;               //imul r9, r8, 0x1920
    rax = umul128(rax, r9, (uintptr_t*)&rdx);              //mul r9
    rdx >>= 0xB;            //shr rdx, 0x0B
    rax = rdx * 0x18A9;             //imul rax, rdx, 0x18A9
    r9 -= rax;              //sub r9, rax
    rax = 0xF0F0F0F0F0F0F0F1;               //mov rax, 0xF0F0F0F0F0F0F0F1
    rax = umul128(rax, r9, (uintptr_t*)&rdx);              //mul r9
    rcx = r9;               //mov rcx, r9
    rdx >>= 0xB;            //shr rdx, 0x0B
    rax = rdx * 0x660;              //imul rax, rdx, 0x660
    rcx -= rax;             //sub rcx, rax
    rax = 0xAAAAAAAAAAAAAAAB;               //mov rax, 0xAAAAAAAAAAAAAAAB
    rax = umul128(rax, r9, (uintptr_t*)&rdx);              //mul r9
    rcx <<= 0x3;            //shl rcx, 0x03
    rdx >>= 0x1;            //shr rdx, 0x01
    rax = rdx + rdx * 2;            //lea rax, [rdx+rdx*2]
    rax += rax;             //add rax, rax
    rcx -= rax;             //sub rcx, rax
    r15 = k_memory.ReadNb<uint16_t>(rcx + r11 * 1 + 0xA04FDE0);                //movsx r15d, word ptr [rcx+r11*1+0xA04FDE0]
    return r15;
}
