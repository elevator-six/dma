#include "memory.h"

uint64_t Memory::get_proc_baseaddr()
{
	return proc.baseaddr;
}

uint64_t Memory::get_peb_addr()
{
    return proc.peb_address;
}

uintptr_t Memory::get_module_size()
{
	return proc.modulesize;
}

process_status Memory::get_proc_status()
{
	return status;
}

void Memory::check_proc()
{
	if (status == process_status::FOUND_READY)
	{
		short c;
        Read<short>(proc.baseaddr, c);

		if (c != 0x5A4D)
		{
			status = process_status::FOUND_NO_ACCESS;
			close_proc();
		}
	}
}

void Memory::open_proc(const char* name)
{
    if(!conn)
    {
        ConnectorInventory *inv = inventory_scan();
        conn = inventory_create_connector(inv, "qemu_procfs", "");
        inventory_free(inv);
    }

    if (conn)
    {
        if(!kernel)
        {
            kernel = kernel_build(conn);
        }

        if(kernel)
        {
            Kernel *tmp_ker = kernel_clone(kernel);
		    proc.hProcess = kernel_into_process(tmp_ker, name);
        }
		
        if (proc.hProcess)
        {
			Win32ModuleInfo *module = process_module_info(proc.hProcess, name);

			if (module)
            {
				OsProcessModuleInfoObj *obj = module_info_trait(module);
				proc.baseaddr = os_process_module_base(obj);
				proc.modulesize = os_process_module_size(obj);
				os_process_module_free(obj);
				mem = process_virt_mem(proc.hProcess);
                status = process_status::FOUND_READY;
            }
            else
            {
                status = process_status::FOUND_NO_ACCESS;
				close_proc();
            }
        }
        else
        {
            status = process_status::NOT_FOUND;
        }
    }
    else
    {
        printf("Can't create connector\n");
		exit(0);
    }
}

void Memory::close_proc()
{
	if (proc.hProcess)
	{
		process_free(proc.hProcess);
		virt_free(mem);	
	}

	proc.hProcess = 0;
	proc.baseaddr = 0;
	mem = 0;
}

void Memory::load_proc_info(const char* name)
{
	size_t process_count = 0;
	Win32ProcessInfo *processes[512];

	if (!conn)
	{
		ConnectorInventory *inv = inventory_scan();
		conn = inventory_create_connector(inv, "qemu_procfs", "");
		inventory_free(inv);
	}

	if (conn)
	{
		if (!kernel)
		{
			kernel = kernel_build(conn);
		}

		if (kernel)
		{
			Kernel *tmp_ker = kernel_clone(kernel);
			process_count = kernel_process_info_list(kernel, processes, 512);
		}

		if (process_count != 0)
		{
			for (size_t i = 0; i < process_count; i++) {
				Win32ProcessInfo *process = processes[i];
				OsProcessInfoObj *info = process_info_trait(process);
				char local_name[32];
				os_process_info_name(info, local_name, 32);

				if (local_name[0] == name[0] && local_name[1] == name[1] && local_name[2] == name[2] && local_name[3] == name[3] && local_name[4] == name[4] && local_name[5])
				{
						printf(" Process info found!\n");
						proc.peb_address = process_info_peb(process);
				}
				uint64_t p_a = process_info_peb_native(process);

				os_process_info_free(info);
				process_info_free(process);
			}
		}
	}
}

uint64_t Memory::ScanPointer(uint64_t ptr_address, const uint32_t offsets[], int level)
{
	if (!ptr_address)
		return 0;

	uint64_t lvl = ptr_address;

	for (int i = 0; i < level; i++)
	{
		if (!Read<uint64_t>(lvl, lvl) || !lvl)
			return 0;
		lvl += offsets[i];
	}

	return lvl;
}
