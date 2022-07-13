/**
* SkillEnhancer.cpp
*
* Simple wallhack for Counter-Strike: Global Offensive.
*
* @author: yky - https://github.com/ykyoshi
* @version: 0.9
*/


#include <windows.h>	// Required for DWORD. Using Win32 API is usually not advised.
#include <iostream>	// Required for input / output streams.
#include <TlHelp32.h>	// Required for Windows processes and modules.
#include <string>	// Required for strings.
#include <sstream>	// Required for string streams.

using namespace std;	// Not required.


/**
* Offsets declaration.
* CSGO offsets are avalaible here: https://github.com/frk1/hazedumper.
* TODO: use hazedumper to scan offsets programatically.
*/
DWORD entityList = 0x4D07DD4;		// Enemies are entities.
DWORD localPlayerO = 0xCF5A4C;		// Add to the client module to get local player entity.
DWORD glowIndex = 0xA40C;		// Add to an entity to get the glow index of this entity.
//DWORD bSpotted = 0x93D;		// Unused. Get whether or not an entity is spotted (appears on radar) in boolean.
DWORD bDormant = 0xED;			// Get whether or not an entity is dormant (alive / connected) in boolean.
DWORD glowObjectManager = 0x5248228;	// Glow is used in various occasions in Gun Game modes.
DWORD teamNum = 0xF4;			// Add to an entity to get the team number of this entity.
DWORD localPlayer;			// Local player entity.
DWORD client;				// client_panorama.dll module.
DWORD teamId;				// Local player's team.
DWORD glowPointer;			// Used on entities to make them glow.


/**
* ProcMem class.
* This class contains variables and templates used to read and write in process memory.
*/
class ProcMem {
protected:
	HANDLE hProcess;	// A process handle is an integer that identifies a process.
	DWORD dwPID;		// Used to get process ID with TH32.

public:
	ProcMem();	// Constructor.
	~ProcMem();	// Destructor.
	int iSizeOfArray(int *iArray);

	// Read template function.
	template <class cData>
	cData Read(DWORD dwAddress)
	{
		cData cRead;
		ReadProcessMemory(hProcess, (LPVOID)dwAddress, &cRead, sizeof(cData), NULL);
		return cRead;
	}

	// Read template function.
	template <class cData>
	cData Read(DWORD dwAddress, char *Offset, BOOL Type)
	{
		int iSize = iSizeOfArray(Offset) - 1;
		dwAddress = Read<DWORD>(dwAddress);
		for (int i = 0; i < iSize; i++)
			dwAddress = Read<DWORD>(dwAddress + Offset[i]);
		if (!Type)
			return dwAddress + Offset[iSize];
		else
			return Read<cData>(dwAddress + Offset[iSize]);
	}

	// Write template function.
	template <class cData>
	void Write(DWORD dwAddress, cData Value)
	{
		WriteProcessMemory(hProcess, (LPVOID)dwAddress, &Value, sizeof(cData), NULL);
	}

	// Write template function.
	template <class cData>
	void Write(DWORD dwAddress, char *Offset, cData Value)
	{
		Write<cData>(Read<cData>(dwAddress, Offset, false), Value);
	}

	// Virtual functions can be overridden in derived classes. Very different from Java.
	virtual void Process(char* ProcessName);
	virtual DWORD Module(LPSTR ModuleName);
};

// Constructor.
ProcMem::ProcMem() {
}

// Destructor.
ProcMem::~ProcMem() {
	CloseHandle(hProcess);
}

// Function that returns length of an array (int).
int ProcMem::iSizeOfArray(int *iArray) {
	for (int iLength = 1; iLength < MAX_PATH; iLength++)
		if (iArray[iLength] == '*')
			return iLength;
	cout << "\nLENGTH: Failed to read length of array\n";
	return 0;
}

// Function that finds a process handle from its name.
void ProcMem::Process(char* ProcessName)
{
	HANDLE hPID = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);	// Takes a snapshot of processes, modules, etc.
	PROCESSENTRY32 ProcEntry;
	ProcEntry.dwSize = sizeof(ProcEntry);

	// Loop through processes.
	do
		if (!strcmp(ProcEntry.szExeFile, ProcessName))
		{
			dwPID = ProcEntry.th32ProcessID;
			CloseHandle(hPID);
			hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPID);
			return;
		}
	while (Process32Next(hPID, &ProcEntry));
	cout << "\ncsgo.exe process not found\n";
	system("pause");
	exit(0);
}

// Function that finds a process module from its name.
DWORD ProcMem::Module(LPSTR ModuleName)
{
	HANDLE hModule = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwPID);	// Takes a snapshot of modules.
	MODULEENTRY32 mEntry;
	mEntry.dwSize = sizeof(mEntry);

	// Loop through modules.
	do
		if (!strcmp(mEntry.szModule, ModuleName))
		{
			CloseHandle(hModule);
			return (DWORD)mEntry.modBaseAddr;
		}
	while (Module32Next(hModule, &mEntry));
	cout << "client_panorama.dll not found. Trying again..." << endl;
	return 0;
}

ProcMem PM;


/**
* VirtualHX class.
* This class contains the hack's functions. It uses ProcMem functions to find CSGO process and its module,
* read undefined offsets, and write the glow hack into our PM object.
*/
class VirtualHX {
private:
	// Write glow (mObj) on player (mObj2).
	static void player_glow(DWORD mObj, DWORD mObj2, float r, float g, float b) {
		PM.Write<float>((mObj + (mObj2 * 0x38)) + 0x4, r);
		PM.Write<float>((mObj + (mObj2 * 0x84)) + 0x8, g);
		PM.Write<float>((mObj + (mObj2 * 0x38)) + 0xC, b);
		PM.Write<float>((mObj + (mObj2 * 0x38)) + 0x10, 0.7f);
		PM.Write<BOOL>((mObj + (mObj2 * 0x38)) + 0x24, true);
		PM.Write<BOOL>((mObj + (mObj2 * 0x38)) + 0x25, false);
	}

public:
	static void engine_start() {
		try {
			PM.Process("csgo.exe");		// Get csgo.exe handle.
		}
		catch (std::exception e) {
			cout << e.what() << std::endl;
		}
	}
	static unsigned long __stdcall hx_thread(void*) {
		client = PM.Module("client_panorama.dll");			// Get client_panorama.dll.
		glowPointer = PM.Read<DWORD>(client + glowObjectManager);	// Read glow.
		localPlayer = PM.Read<DWORD>(client + localPlayerO);		// Read local player entity.
		if (!localPlayer) return EXIT_FAILURE;
		teamId = PM.Read<DWORD>(localPlayer + teamNum);			// Get local player's team.
		while (true) {
			Sleep(50);
			if (glowPointer != NULL) {

				// Loop through entities. Each entity looped through is mentioned as 'current player'.
				for (int i = 0; i < 64; i++) {

					// /!\ One of the Read sometimes accesses an invalid address and crashes csgo.exe process. /!\
					DWORD currentPlayer = PM.Read<int>(client + entityList + i * 0x10);	// Get current player entity.
					if (currentPlayer != NULL) {
						DWORD currentPlayerTeamId = PM.Read<int>(currentPlayer + teamNum);	// Get current player's team.
						DWORD currentPlayerGlowIndex = PM.Read<int>(currentPlayer + glowIndex);	// Get current player's glow index.
						DWORD mObj = glowPointer;
						DWORD mObj2 = currentPlayerGlowIndex;
						bool currentPlayerDormant;
						currentPlayerDormant = PM.Read<bool>(currentPlayer + bDormant);		// Get current player's dormant status.

																								// Check if current player is enemy and alive.
						if (currentPlayerTeamId != teamId && currentPlayerDormant == false) {
							player_glow(mObj, mObj2, 1.f, 0.5f, 0.f);	// Call glow on current player.
						}

						// TODO: this specific condition is useless.
						else if (currentPlayerDormant == true) {
							continue;	// Go to next entity.
						}
					}

					// TODO: this specific condition is useless.
					else if (currentPlayer == NULL) {
						continue;	// Go to next entity.
					}
				}
			}
		}
		return EXIT_SUCCESS;
	}
};


/**
* Main function.
* Sets console title and prints, calls engine_start function and reads user's input.
*/
int main() {
	SetConsoleTitle("Skill Enhancer");
	bool enabled = false;
	HANDLE HX = NULL;
	VirtualHX::engine_start();
	Sleep(400);	// Useless in this case, but it's usually good to not print all messages at the same time.
	cout << "F1 : ON/OFF..." << endl;
	while (TRUE) {
		Sleep(1);
		// F1 starts and stops the hack.
		if (GetAsyncKeyState(VK_F1) & 1) {
			enabled = !enabled;
			if (enabled) {
				cout << "Enhancer : ON" << endl;
				HX = CreateThread(NULL, NULL, &VirtualHX::hx_thread, NULL, NULL, NULL);
			}
			else {
				cout << "Enhancer : OFF" << endl;
				TerminateThread(HX, 0);
				CloseHandle(HX);
			}
		}
	}
}
