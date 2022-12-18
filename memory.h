#include "memflow_win32.h"
#include "byteswap.h"
#include <cstring>
#include <stdio.h>
#include <mutex>

/* FOR PATTERN SCANNER */
#include <vector>
#include <memory>
#include <sstream>
#include <optional>

typedef void* LPVOID;
/* =================== */

/* =================== */
typedef unsigned long DWORD;
typedef uint8_t BYTE;
/* =================== */

/* =================== */
static CloneablePhysicalMemoryObj *conn = 0;
static Kernel *kernel = 0;
/* =================== */

typedef struct Process
{
	Win32Process* hProcess = 0;
	Win32ProcessInfo* pInfo = 0;
	uint64_t baseaddr = 0;
	uintptr_t modulesize = 0;
	uint64_t peb_address = 0;
}Process;

enum class process_status : BYTE
{
	NOT_FOUND,
	FOUND_NO_ACCESS,
	FOUND_READY
};

class Memory
{
private:
	Process proc;
	VirtualMemoryObj* mem;
	PhysicalMemoryObj* p_mem;
	process_status status = process_status::NOT_FOUND;
	std::mutex m;
public:
	~Memory() 
	{ 
		if (mem)
			virt_free(mem);
		if (p_mem)
			phys_free(p_mem);

		if (proc.hProcess)
			process_free(proc.hProcess);
	}

	uint64_t get_proc_baseaddr();

	uint64_t get_peb_addr();

	uintptr_t get_module_size();

	process_status get_proc_status();

	void check_proc();

	void open_proc(const char* name);

	void close_proc();

	void load_proc_info(const char* name);

	bool ReadSized(uint64_t address, LPVOID out, size_t size);

	template<typename T>
	bool Read(uint64_t address, T& out);

	template<typename T>
	T ReadNb(uint64_t address);

	template<typename T>
	bool ReadArray(uint64_t address, T out[], size_t len);

	template<typename T>
	bool Write(uint64_t address, const T& value);

	template<typename T>
	bool WriteArray(uint64_t address, const T value[], size_t len);

	uint64_t ScanPointer(uint64_t ptr_address, const uint32_t offsets[], int level);

	class signature
	{
		uintptr_t m_base;
		size_t m_size;
		std::unique_ptr<char[]> m_data;
		uintptr_t m_temp;
	public:
		signature(Memory& outter)
		{
			m_base = (uintptr_t)outter.proc.baseaddr;
			m_size = outter.proc.modulesize;
			m_data = std::make_unique<char[]>(m_size);
			outter.ReadSized(m_base, m_data.get(), m_size);

			m_temp = m_base;
		}

		~signature()
		{
			m_data.reset();
		}

		signature& scan(std::string pattern)
		{
			m_temp = 0;

			std::stringstream ss(pattern);
			std::vector<std::string> substrings;
			std::string substring;
			while (std::getline(ss, substring, ' ')) substrings.push_back(substring);;

			std::vector<std::optional<char>> pattern_2;
			for (auto& str : substrings)
			{
				if (str == "?" || str == "??")
					pattern_2.push_back(std::nullopt);
				else
					pattern_2.push_back(strtol(str.c_str(), nullptr, 16));
			}

			for (size_t i = 0; i < m_size - pattern_2.size() + 1; i++)
			{
				size_t j;
				for (j = 0; j < pattern_2.size(); j++)
				{
					if (!pattern_2[j].has_value())
						continue;
					if (pattern_2[j].value() != m_data.get()[i + j])
						break;
				}
				if (j == pattern_2.size())
				{
					m_temp = m_base + i;
					return *this;
				}
			}
			return *this;
		}

		signature& add(int value) { m_temp += value; return *this; }

		signature& sub(int value) { m_temp -= value; return *this; }

		signature& rip(Memory& outter)
		{
			int value;
			outter.Read<int>(m_temp, value);
			m_temp += value + 4;
			return *this;
		}

		template<typename T> T as() { return (T)m_temp; }
	};
};

inline bool Memory::ReadSized(uint64_t address, LPVOID out, size_t size)
{
	std::lock_guard<std::mutex> l(m);
	return virt_read_raw_into(mem, address, (uint8_t*)&out, size) == 0;
}

template<typename T>
inline bool Memory::Read(uint64_t address, T& out)
{
	std::lock_guard<std::mutex> l(m);
	return mem && virt_read_raw_into(mem, address, (uint8_t*)&out, sizeof(T)) == 0;
}

template<typename T>
inline T Memory::ReadNb(uint64_t address)
{
	T buffer;
	std::lock_guard<std::mutex> l(m);
	virt_read_raw_into(mem, address, (uint8_t*)&buffer, sizeof(T));
	return buffer;
}

template<typename T>
inline bool Memory::ReadArray(uint64_t address, T out[], size_t len)
{
	std::lock_guard<std::mutex> l(m);
	return mem && virt_read_raw_into(mem, address, (uint8_t*)out, sizeof(T) * len) == 0;
}

template<typename T>
inline bool Memory::Write(uint64_t address, const T& value)
{
	std::lock_guard<std::mutex> l(m);
	return mem && virt_write_raw(mem, address, (uint8_t*)&value, sizeof(T)) == 0;
}

template<typename T>
inline bool Memory::WriteArray(uint64_t address, const T value[], size_t len)
{
	std::lock_guard<std::mutex> l(m);
	return mem && virt_write_raw(mem, address, (uint8_t*)value, sizeof(T) * len) == 0;
}
