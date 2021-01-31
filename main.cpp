#include <Windows.h>
#include <conio.h>

#include "hlsdk.h"
#include "main.h"
#include "detours.h"

cl_clientfunc_t* pClient;
cl_clientfunc_t Client;

cl_enginefunc_t* pEngine;
cl_enginefunc_t Engine;

engine_studio_api_s* pStudio;
engine_studio_api_s Studio;

DWORD ClientBase, ClientEnd;
DWORD HwBase, HwEnd;

int Hooked_R_StudioDrawModel(int flags)
{
	return R_StudioDrawModel(flags);
}

int Hooked_R_StudioDrawPlayer(int flags, entity_state_t* pplayer)
{
	return R_StudioDrawPlayer(flags, pplayer);
}

void mainthread()
{
	int ms = 0;
	while (!ClientBase)
	{
		if (ms > 20000)
			exit(EXIT_FAILURE);

		ClientBase = (DWORD)GetModuleHandle("client.dll");

		if (!ClientBase)
		{
			ms += 1000;
			Sleep(1000);
			_cprintf("Could not get Client base....%d\n", ms);
		}
	}

	ClientEnd = ClientBase + PIMAGE_NT_HEADERS(ClientBase + (DWORD)PIMAGE_DOS_HEADER(ClientBase)->e_lfanew)->OptionalHeader.SizeOfImage - 1;

	_cprintf("clientdll: base %#x, end: %#x\n", ClientBase, ClientEnd);

	ms = 0;

	while (!HwBase)
	{
		if (ms > 20000)
			exit(EXIT_FAILURE);

		HwBase = (DWORD)GetModuleHandle("hw.dll");

		if (!HwBase)
		{
			ms += 1000;
			Sleep(1000);
			_cprintf("Could not get Hw base....%d\n", ms);
		}
	}

	HwEnd = HwBase + PIMAGE_NT_HEADERS(HwBase + (DWORD)PIMAGE_DOS_HEADER(HwBase)->e_lfanew)->OptionalHeader.SizeOfImage - 1;

	_cprintf("hwdll: base %#x, end: %#x\n", HwBase, HwEnd);

	// Got modules... now hooks

	const auto FindPattern = [&](
		IN const DWORD base,
		IN const DWORD end,
		IN const CHAR* pattern,
		IN const CHAR* mask,
		IN const DWORD offset
		) -> DWORD
	{
		// We get length from mask, otherwise corrupted length.
		const INT patternLength = lstrlenA(mask);
		bool found = FALSE;
		for (DWORD i = base; i < end - patternLength; i++)
		{
			found = TRUE;
			for (int idx = 0; idx < patternLength; idx++)
			{
				if (mask[idx] == 'x' && pattern[idx] != *(CHAR*)(i + idx))
				{
					found = FALSE;
					break;
				}
			}
			if (found)
				return i + offset;
		}
		return 0x0;
	};

	const auto FarProc = [&](
		IN const DWORD addr,
		IN const DWORD base,
		IN const DWORD end
		) -> const bool
	{
		return ((addr < base) || (addr > end));
	};

	const auto IsAddressValidPtr = [&](
		IN const char* name,
		IN const DWORD base,
		IN const DWORD end,
		IN const DWORD addr
		) -> const bool
	{
		if (!addr)
		{
			_cprintf("ERROR %s: The address is null. %#x\n", name, addr);
			return false;
		}

		if (FarProc(addr, base, end))
		{
			_cprintf("ERROR %s: The address is not in range. %#x / %#x\n", name, addr, *(DWORD*)addr);
			return false;
		} else
		{
			if (FarProc(*(DWORD*)addr, base, end))
			{
				_cprintf("ERROR %s: The address pointer is not in range. original addr: %#x / %#x\n", name, addr, *(DWORD*)addr);
				return false;
			}
		}

		_cprintf("%s: The address is valid. %#x / %#x\n", name, addr, *(DWORD*)addr);

		return true;
	};

	// Dont check for ptr
	const auto IsAddressValid = [&](
		IN const char* name,
		IN const DWORD base,
		IN const DWORD end,
		IN const DWORD addr
		) -> const bool
	{
		if (!addr)
		{
			_cprintf("ERROR %s: The address is null. %#x\n", name, addr);
			return false;
		}

		if (FarProc(addr, base, end))
		{
			_cprintf("ERROR %s: The address is not in range. %#x / %#x\n", name, addr, *(DWORD*)addr);
			return false;
		} 

		_cprintf("%s: The address is valid. %#x / %#x\n", name, addr, *(DWORD*)addr);

		return true;
	};

	// Clientfuncs:
	DWORD Clientfuncs = FindPattern(HwBase, HwEnd, "\xFF\x15\x60\xED", "xxxx", 0x2);
	if (!IsAddressValidPtr("Clientfuncs", HwBase, HwEnd, Clientfuncs))
	{
		Sleep(2000);
		exit(EXIT_FAILURE);
	}

	pClient = (cl_clientfunc_t*)*(DWORD*)(Clientfuncs);
	_cprintf("Found ClientFuncs on address: %#x\n", (DWORD)Clientfuncs);

	// Enginefuncs:
	DWORD Enginefuncs = ((DWORD)pClient->Initialize + 0x1D);
	if (!IsAddressValidPtr("Enginefuncs", ClientBase, ClientEnd, Enginefuncs))
	{
		Sleep(2000);
		exit(EXIT_FAILURE);
	}

	pEngine = (cl_enginefunc_t*)*(DWORD*)(Enginefuncs);
	_cprintf("Found Enginefuncs on address: %#x\n", (DWORD)Enginefuncs);

	// Studiofuncs:
	DWORD Studiofuncs = ((DWORD)pClient->HUD_GetStudioModelInterface + 0x1A);
	if (!IsAddressValidPtr("Studiofuncs", ClientBase, ClientEnd, Studiofuncs))
	{
		Sleep(2000);
		exit(EXIT_FAILURE);
	}

	pStudio = (engine_studio_api_s*)*(DWORD*)(Studiofuncs);
	_cprintf("Found Studiofuncs on address: %#x\n", (DWORD)Studiofuncs);

	// Got all hooks, copy originals
	memcpy(&Client, &pClient, sizeof(cl_clientfunc_t));
	memcpy(&Engine, &pEngine, sizeof(cl_enginefunc_t));
	memcpy(&Studio, &pStudio, sizeof(engine_studio_api_s));

	_cprintf("Original pointers has been copied into locals.\n");

	// Now function hook
	const auto CheckForDetourErrorCode = [&](
		IN const LONG errcode,
		IN const char* name
		) -> void
	{
		if (errcode == NO_ERROR)
			_cprintf("%s: No failure while detouring.\n", name);
		else
		{
			_cprintf("ERROR %s: Failure detouring. %d\n", name, errcode);
			Sleep(2000);
			exit(EXIT_FAILURE);
		}
	};

	typedef int (*pfnR_StudioDrawModel_t)(int flags);
	typedef int (*pfnR_StudioDrawPlayer_t)(int flags, entity_state_t* pplayer);

	// StudioDrawModel hook
	DWORD StudioDrawModel = FindPattern(ClientBase, ClientEnd, "\x74\x00\x56\xB9\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x8B", "x?xx????x????x", 0x1C);
	if (!IsAddressValid("StudioDrawModel", ClientBase, ClientEnd, StudioDrawModel))
	{
		Sleep(2000);
		exit(EXIT_FAILURE);
	}

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)StudioDrawModel, Hooked_R_StudioDrawModel);

	LONG errcode = DetourTransactionCommit();

	// We check if the function fails or not.
	CheckForDetourErrorCode(errcode, "StudioDrawModel");

	// StudioDrawPlayer hook
	DWORD StudioDrawPlayer = FindPattern(ClientBase, ClientEnd, "\xA0\x00\x00\x00\x00\x53\x32", "x????xx", 0x0);
	if (!IsAddressValid("StudioDrawPlayer", ClientBase, ClientEnd, StudioDrawPlayer))
	{
		Sleep(2000);
		exit(EXIT_FAILURE);
	}

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)StudioDrawPlayer, Hooked_R_StudioDrawPlayer);

	errcode = DetourTransactionCommit();

	// We check if the function fails or not.
	CheckForDetourErrorCode(errcode, "StudioDrawPlayer");

	R_StudioInit();

	_cprintf("Finished!\n");
}

BOOL WINAPI DllMain(
	_In_ HINSTANCE hInstance,
	_In_ DWORD dwReason,
	_In_ LPVOID lpvReserved
)
{
	AllocConsole();

	switch (dwReason)
	{
		case DLL_PROCESS_ATTACH:
		{

			CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)mainthread, 0, 0, 0);

			break;
		}
		case DLL_PROCESS_DETACH:
		{
			break;
		}
	}

	return TRUE;
}
